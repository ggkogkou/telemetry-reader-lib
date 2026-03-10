#!/usr/bin/env python3

import sys
import os
import time
import argparse
import threading
import subprocess
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(PROJECT_ROOT / "lib" / "Scripts"))

from instrument_libraries.psu import keysight_n6705b
from test_automation.log_manager import LogManager


def cpp_stdout_reader(proc, command_queue, stop_event):
    """
    Reads machine-readable commands from the C++ collector stdout.
    Expected lines:
      CMD POWER_CYCLE reason=telemetry_timeout silence_s=20.1
      CMD TELEMETRY_RECOVERED
    """
    try:
        while not stop_event.is_set():
            line = proc.stdout.readline()
            if not line:
                break

            line = line.strip()
            if not line:
                continue

            if line.startswith("CMD "):
                command_queue.append(line)
    except Exception as e:
        print(f"✗ stdout reader error: {e}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--collector-exe", required=True, help="Path to foc_tlm_parser.exe")
    ap.add_argument("--telemetry-port", required=True, help="Serial port for telemetry collector, e.g. COM5")
    ap.add_argument("--telemetry-baud", type=int, default=460800)
    ap.add_argument("--telemetry-csv", default="telemetry.csv")
    ap.add_argument("--telemetry-timeout-s", type=float, default=20.0)
    ap.add_argument("--log-dir", default="./logs")
    ap.add_argument("--power-off-s", type=float, default=5.0)
    ap.add_argument("--cooldown-s", type=float, default=300.0)
    args = ap.parse_args()

    Path(args.log_dir).mkdir(parents=True, exist_ok=True)
    telemetry_csv_path = os.path.join(args.log_dir, args.telemetry_csv)

    collector_cmd = [
        args.collector_exe,
        "--port", args.telemetry_port,
        "--baud", str(args.telemetry_baud),
        "--csv", telemetry_csv_path,
        "--watchdog-timeout-s", str(args.telemetry_timeout_s),
    ]

    print("=== Starting C++ telemetry collector ===")
    print(" ".join(collector_cmd))

    collector_proc = subprocess.Popen(
        collector_cmd,
        stdout=subprocess.PIPE,
        text=True,
        bufsize=1,
    )

    stop_event = threading.Event()
    command_queue = []

    stdout_thread = threading.Thread(
        target=cpp_stdout_reader,
        args=(collector_proc, command_queue, stop_event),
        daemon=True,
    )
    stdout_thread.start()

    PSU_ADDR = "TCPIP::192.168.0.160"

    try:
        psu = keysight_n6705b(visa_string=PSU_ADDR)
        print(f"✓ Connected to PSU at {PSU_ADDR}")
        print(f"  Channels available: {psu.channels}")
        print(f"  Max voltage: {psu.max_voltage}V")
        print(f"  Max current: {psu.max_current}A")
    except Exception as e:
        print(f"✗ PSU connection failed: {e}")
        stop_event.set()
        collector_proc.terminate()
        raise

    psu_lock = threading.Lock()

    channel_config = [
        (2, 20.0, 1.0, "Channel_2"),
    ]

    print("\n=== Configuring PSU Channels ===")
    for ch, voltage, current, alias in channel_config:
        print(f"  CH{ch}: {voltage}V, {current}A limit, Alias: {alias}")

    channels = [cfg[0] for cfg in channel_config]
    voltages = [cfg[1] for cfg in channel_config]
    currents = [cfg[2] for cfg in channel_config]
    aliases_dict = {cfg[0]: cfg[3] for cfg in channel_config}

    with psu_lock:
        if len(channels) == 1:
            psu.set_voltages(channels[0], voltages[0])
            psu.set_currents(channels[0], currents[0])
        else:
            psu.set_voltages(channels, voltages)
            psu.set_currents(channels, currents)

    print(f"✓ Voltages set: {dict(zip(channels, voltages))}")
    print(f"✓ Current limits set: {dict(zip(channels, currents))}")

    with psu_lock:
        set_voltages = psu.get_set_voltages(channels)
        current_limits = psu.get_current_limits(channels)

    print("\nVerification:")
    print(f"  Programmed voltages: {dict(zip(channels, set_voltages))}")
    print(f"  Programmed current limits: {dict(zip(channels, current_limits))}")

    print("\n=== Enabling PSU Output ===")
    with psu_lock:
        if len(channels) == 1:
            psu.set_outputs(channels[0], True)
        else:
            psu.set_outputs(channels, True)

    time.sleep(1.0)
    print(f"✓ PSU output enabled on channels: {channels}")

    print("\n=== Initializing LogManager ===")
    try:
        logger = LogManager(base_dir=args.log_dir, timezone="UTC", use_wide_format=True)
        print(f"✓ LogManager initialized, output dir: {args.log_dir}")

        psu_channel_list = [(ch, aliases_dict[ch]) for ch in channels]
        logger.configure_psu_channels("PSU1", psu_channel_list)
        logger._write_psu_wide_header("PSU1")

        print("✓ Logging session started")
    except Exception as e:
        print(f"✗ LogManager setup failed: {e}")
        stop_event.set()
        collector_proc.terminate()
        raise

    print("\n=== Starting Continuous Logging ===")
    print("Press Ctrl+C to stop logging...\n")

    sample_count = 0
    start_time = time.time()
    elapsed = 0.0
    last_power_cycle_ts = 0.0
    cooldown_s = float(args.cooldown_s)
    power_off_s = float(args.power_off_s)

    logger.log_event("TEST_START", "")
    logger.log_event("PSU_ON", "")

    try:
        while True:
            if collector_proc.poll() is not None:
                raise RuntimeError(f"Telemetry collector exited with code {collector_proc.returncode}")

            try:
                with psu_lock:
                    voltage_str = psu.get_voltages(channels)
                    current_str = psu.get_currents(channels)

                voltages_measured = [float(v) for v in voltage_str.split(",")]
                currents_measured = [float(c) for c in current_str.split(",")]

                readings = {}
                for i, ch in enumerate(channels):
                    readings[ch] = {
                        "alias": aliases_dict[ch],
                        "voltage": voltages_measured[i],
                        "current": currents_measured[i],
                    }

                logger.log_psu_batch(readings, "PSU1")

                sample_count += 1
                elapsed = time.time() - start_time

            except Exception as e:
                print(f"✗ PSU logging error at sample {sample_count}: {e}")

            while command_queue:
                cmd = command_queue.pop(0)
                now = time.time()

                if cmd.startswith("CMD POWER_CYCLE"):
                    if (now - last_power_cycle_ts) < cooldown_s:
                        print(f"⚠ Ignoring POWER_CYCLE due to cooldown ({now - last_power_cycle_ts:.1f}s < {cooldown_s:.1f}s)")
                        continue

                    print(f"⚠ Received command from collector: {cmd}")
                    logger.log_event(
                        "power_cycle",
                        "Telemetry collector requested PSU power cycle",
                        {"command": cmd},
                    )

                    try:
                        with psu_lock:
                            if len(channels) == 1:
                                psu.set_outputs(channels[0], False)
                            else:
                                psu.set_outputs(channels, False)

                        time.sleep(power_off_s)

                        with psu_lock:
                            if len(channels) == 1:
                                psu.set_outputs(channels[0], True)
                            else:
                                psu.set_outputs(channels, True)

                        last_power_cycle_ts = time.time()
                        logger.log_event(
                            "power_cycle",
                            "PSU power cycle complete",
                            {"off_seconds": power_off_s},
                        )
                        print("✓ Power cycle completed")

                    except Exception as e:
                        last_power_cycle_ts = time.time()
                        print(f"✗ Power cycle failed: {e}")

                elif cmd.startswith("CMD TELEMETRY_RECOVERED"):
                    logger.log_event("telemetry_recovered", "Telemetry collector reported recovery")
                    print("✓ Telemetry recovered")

            time.sleep(0.1)

    except KeyboardInterrupt:
        elapsed = time.time() - start_time
        print(f"\n✓ Logging stopped by user after {sample_count} samples ({elapsed:.1f}s elapsed)")
    except Exception as e:
        print(f"\n✗ Unexpected error: {e}")

    print("\n=== Cleanup ===")

    try:
        stop_event.set()

        if collector_proc.poll() is None:
            collector_proc.terminate()
            try:
                collector_proc.wait(timeout=5.0)
            except subprocess.TimeoutExpired:
                collector_proc.kill()

        with psu_lock:
            if len(channels) == 1:
                psu.set_outputs(channels[0], False)
            else:
                psu.set_outputs(channels, False)

        print(f"✓ PSU output disabled on channels: {channels}")
        logger.log_event("PSU_OFF", "PSU off")

        logger.log_event(
            "TEST_END",
            f"PSU logging completed. Total samples: {sample_count}",
            {"duration_seconds": elapsed, "sample_count": sample_count},
        )

        print("\n=== Logging Summary ===")
        print(f"Total PSU samples: {sample_count}")
        print(f"Duration: {elapsed:.1f} seconds")
        print(f"Sample rate: {sample_count / elapsed:.2f} Hz" if elapsed > 0 else "Sample rate: N/A")
        print(f"Telemetry CSV: {telemetry_csv_path}")
        print("\n✓ All cleanup completed successfully")

    except Exception as e:
        print(f"✗ Cleanup error: {e}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())