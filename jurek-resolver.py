# SPDX-License-Identifier: Zlib
# SPDX-FileCopyrightText: 2025 CERN (home.cern)
# SPDX-Created: 2025-11-25
# SPDX-FileContributor: Author: Jurek Weber <jurek.weber@cern.ch>

"""Library providing an C232HM-EDHSL-0 FTDI driver for the SPI interface of the AD2S1210"""

from abc import abstractmethod, ABC
import time
import sys
import serial

try:
    from robot.api.deco import library, keyword
    from robot.api.exceptions import Failure, Error
except ImportError:
    print("Did not find robot framework!")


    # no-op decorator, if robot framework is not installed
    def _noop_decorator(*dargs, **dkwargs):
        if len(dargs) == 1 and callable(dargs[0]) and not dkwargs:
            return dargs[0]

        def wrapper(obj):
            return obj

        return wrapper


    library = _noop_decorator
    keyword = _noop_decorator

    Failure = AssertionError
    Error = AssertionError


class SpiInterface(ABC):
    """
    Abstract SPI class for the AD2S1210 that can be implemented by different backends
    """

    @abstractmethod
    def connect(self, url):
        """Connects the SPI interface"""

    @abstractmethod
    def sample(self):
        """Holds the sample pin low for a short while"""

    @abstractmethod
    def read(self, reg):
        """Enables the user to read a register via the SPI interface."""

    @abstractmethod
    def write(self, reg, val):
        """Enables the user to write a register via the SPI interface."""


class RpiSpi(SpiInterface):
    """Raspberry Pi Pico implementation for the AD2S1210"""

    def __init__(self):
        self.ser = None

    def connect(self, url):
        if self.ser is not None:
            raise Error("Already connected!")

        self.ser = serial.Serial(url, timeout=1)

    def _assert_init(self):
        if None in [self.ser]:
            raise Error("You have to be connected first.")

    def read(self, reg):
        self._assert_init()
        self.ser.write(b"R" + bytes([reg]))
        res = self.ser.read(1)
        assert (
                len(res) == 1
        ), "USB device did not respond in time. Is the Firmware the correct one?"
        res = int.from_bytes(res[0:1], byteorder="big")
        assert 0 <= res <= 255
        return res

    def write(self, reg, val):
        self._assert_init()
        assert 0 <= reg <= 255
        assert 0 <= val <= 255
        self.ser.write(b"W" + bytes([reg, val]))

    def sample(self):
        """Samples the AD2S1210 by holding low the sample pin"""
        self._assert_init()

        self.ser.write(b"s")

    def reset(self):
        """Resets the AD2S1210 by holding low the reset pin"""
        self._assert_init()

        self.ser.write(b"r")

        time.sleep(0.5)


@library
class AD2S1210:  # pylint: disable=too-many-public-methods
    """
    AD2S1210 class providing functionality to configure and read the AD2S1210
    """

    CONFIG_REG = 0x92
    POSITION_REG_H = 0x80
    POSITION_REG_L = 0x81
    VELOCITY_REG_H = 0x82
    VELOCITY_REG_L = 0x83
    LOS_THRESHOLD_REG = 0x88
    DOS_OVERRANGE_THRESHOLD_REG = 0x89
    DOS_RESET_MAX_THRESHOLD_REG = 0x8B
    DOS_RESET_MIN_THRESHOLD_REG = 0x8C
    LOT_HIGH_THRESHOLD_REG = 0x8D
    LOT_LOW_THRESHOLD_REG = 0x8E
    EXCITATION_FREQ_REG = 0x91
    DOS_MISMATCH_THRESHOLD_REG = 0x8A
    ERROR_REG = 0xFF

    def __init__(self):
        self.spi = None
        self.slave = None
        self.gpio = None

    @keyword()
    def connect(self, rpi):
        """
        Connect to the AD2S1210 using an Raspberry Pi Pico 2040

        :param ftdi: The URL to connect of the USB device (e.g., "/dev/cu.usbmodemAD2S12101")
        """
        spi = RpiSpi()
        spi.connect(rpi)
        self.spi = spi

    @keyword()
    def disconnect(self):
        """
        Disconnect the FTDI
        """
        self.spi = None

    @keyword()
    def sample(self):
        """
        Hold the SAMPLE pin low for a short while, updating the Position and Velocity registers
        of the AD2S1210

        :param self: Description
        """
        self.spi.sample()

    @keyword()
    def get_config(self):
        """Get the config of the AD2S1210"""
        res = self.spi.read(self.CONFIG_REG)
        _assert_config_correctness(res)
        return res

    @keyword()
    def set_config(self, config):
        """Set config"""
        _assert_config_correctness(config)
        self.spi.write(AD2S1210.CONFIG_REG, config)

    @keyword()
    def clear_error(self):
        """
        Clears the error register in the AD2S1210, and throws an exception if an error bit is set
        """
        error = self.spi.read(self.ERROR_REG)
        if error != 0:
            raise ResolverException("Error in Chip", error)

    @keyword()
    def reset_chip(self):
        """
        Reset AD2S1210 by holding the hardware RESET pin low
        """
        self.spi.reset()

    @keyword()
    def get_pos(self):
        """
        Gets the position and returns it as a signed 16 bit integer. 2 LSBs are always zero
        `sample` function has to be called before, otherwise values do not update
        """
        h = self.spi.read(self.POSITION_REG_H)
        l = self.spi.read(self.POSITION_REG_L)

        return _convert_registers(h, l)

    @keyword()
    def get_velocity(self):
        """
        Gets the velocity and returns it as a signed 16 bit integer. 2 LSBs are always zero
        `sample` function has to be called before, otherwise values do not update
        """
        h = self.spi.read(self.VELOCITY_REG_H)
        l = self.spi.read(self.VELOCITY_REG_L)

        return _convert_registers(h, l, signed=True)

    @keyword()
    def get_excitation_freq(self):
        """
        The LOS threshold register determines the loss of signal threshold
        of the AD2S1210. The AD2S1210 allows the user to set the LOS
        threshold to a value between 0 V and 4.82 V. The resolution of
        the LOS threshold is seven bits, that is, 38 mV. Note that the MSB,
        D7, should be set to 0. The default value of the LOS threshold
        on power-up is 2.2 V. (datasheet p. 21)
        """
        val = self.spi.read(self.EXCITATION_FREQ_REG)
        _assert_register_correctness(val)
        return val

    @keyword()
    def set_excitation_freq(self, val):
        """The LOS threshold register determines the loss of signal threshold
        of the AD2S1210. The AD2S1210 allows the user to set the LOS
        threshold to a value between 0 V and 4.82 V. The resolution of
        the LOS threshold is seven bits, that is, 38 mV. Note that the MSB,
        D7, should be set to 0. The default value of the LOS threshold
        on power-up is 2.2 V. (datasheet p. 21)"""
        _assert_register_correctness(val)
        self.spi.write(self.EXCITATION_FREQ_REG, val)

    @keyword()
    def get_los_threshold(self):
        """
        The LOS threshold register determines the loss of signal threshold
        of the AD2S1210. The AD2S1210 allows the user to set the LOS
        threshold to a value between 0 V and 4.82 V. The resolution of
        the LOS threshold is seven bits, that is, 38 mV. Note that the MSB,
        D7, should be set to 0. The default value of the LOS threshold
        on power-up is 2.2 V. (datasheet p. 21)
        """
        val = self.spi.read(self.LOS_THRESHOLD_REG)
        _assert_register_correctness(val)
        return val

    @keyword()
    def set_los_threshold(self, val):
        """The LOS threshold register determines the loss of signal threshold
        of the AD2S1210. The AD2S1210 allows the user to set the LOS
        threshold to a value between 0 V and 4.82 V. The resolution of
        the LOS threshold is seven bits, that is, 38 mV. Note that the MSB,
        D7, should be set to 0. The default value of the LOS threshold
        on power-up is 2.2 V. (datasheet p. 21)"""
        _assert_register_correctness(val)
        self.spi.write(self.LOS_THRESHOLD_REG, val)

    @keyword()
    def get_dos_overrange_threshold(self):
        """The DOS overrange threshold register determines the degradation
        of signal threshold of the AD2S1210. The AD2S1210 allows the
        user to set the DOS overrange threshold to a value between 0 V
        and 4.82 V. The resolution of the DOS overrange threshold is
        7 bits, that is, 38 mV. The MSB, D7, must be set to 0. The default
        value of the DOS overrange threshold on power-up is 4.1 V."""
        val = self.spi.read(self.DOS_OVERRANGE_THRESHOLD_REG)
        _assert_register_correctness(val)
        return val

    @keyword()
    def set_dos_overrange_threshold(self, val):
        """The DOS overrange threshold register determines the degradation
        of signal threshold of the AD2S1210. The AD2S1210 allows the
        user to set the DOS overrange threshold to a value between 0 V
        and 4.82 V. The resolution of the DOS overrange threshold is
        7 bits, that is, 38 mV. The MSB, D7, must be set to 0. The default
        value of the DOS overrange threshold on power-up is 4.1 V."""
        _assert_register_correctness(val)
        self.spi.write(self.DOS_OVERRANGE_THRESHOLD_REG, val)

    @keyword()
    def get_dos_mismatch_threshold(self):
        """The DOS mismatch threshold register determines the signal
        mismatch threshold of the AD2S1210. The AD2S1210 allows
        the user to set the DOS mismatch threshold to a value between
        0 V and 4.82 V. The resolution of the DOS mismatch threshold
        is seven bits, that is, 38 mV . Note that the MSB, D7, should be
        set to 0.The default value of the DOS mismatch threshold on
        power-up is 380 mV."""
        val = self.spi.read(self.DOS_MISMATCH_THRESHOLD_REG)
        _assert_register_correctness(val)
        return val

    @keyword()
    def set_dos_mismatch_threshold(self, val):
        """The DOS mismatch threshold register determines the signal
        mismatch threshold of the AD2S1210. The AD2S1210 allows
        the user to set the DOS mismatch threshold to a value between
        0 V and 4.82 V. The resolution of the DOS mismatch threshold
        is seven bits, that is, 38 mV . Note that the MSB, D7, should be
        set to 0.The default value of the DOS mismatch threshold on
        power-up is 380 mV."""
        _assert_register_correctness(val)
        self.spi.write(self.DOS_MISMATCH_THRESHOLD_REG, val)

    @keyword()
    def get_dos_reset_max(self):
        """The AD2S1210 continuously stores themaximum
        magnitude of the monitor signal in internal registers. The difference
        between the maximum is calculated to determine if
        a DOS mismatch has occurred.

        When the fault register is cleared, the registers that store the maximum amplitudes of the
        monitor signal are reset to the values stored in the DOS reset maximum threshold register
        """
        val = self.spi.read(self.DOS_RESET_MAX_THRESHOLD_REG)
        _assert_register_correctness(val)
        return val

    @keyword()
    def set_dos_reset_max(self, val):
        """The AD2S1210 continuously stores the maximum
        magnitude of the monitor signal in internal registers. The difference
        between the maximum is calculated to determine if
        a DOS mismatch has occurred.

        When the fault register is cleared, the registers that store the maximum amplitudes of the
        monitor signal are reset to the values stored in the DOS reset maximum threshold register
        """
        _assert_register_correctness(val)
        self.spi.write(self.DOS_RESET_MAX_THRESHOLD_REG, val)

    @keyword()
    def get_dos_reset_min(self):
        """The AD2S1210 continuously stores the minimum
        magnitude of the monitor signal in internal registers. The difference
        between the minimum is calculated to determine if
        a DOS mismatch has occurred.

        When the fault register is cleared, the registers that store the minimum amplitudes of the
        monitor signal are reset to the values stored in the DOS reset minimum threshold register
        """
        val = self.spi.read(self.DOS_RESET_MIN_THRESHOLD_REG)
        _assert_register_correctness(val)
        return val

    @keyword()
    def set_dos_reset_min(self, val):
        """The AD2S1210 continuously stores the minimum
        magnitude of the monitor signal in internal registers. The difference
        between the minimum is calculated to determine if
        a DOS mismatch has occurred.

        When the fault register is cleared, the registers that store the minimum amplitudes of the
        monitor signal are reset to the values stored in the DOS reset minimum threshold register
        """
        _assert_register_correctness(val)
        self.spi.write(self.DOS_RESET_MIN_THRESHOLD_REG, val)

    @keyword()
    def get_lot_high_threshold(self):
        """The LOT high threshold register determines the loss of position
        tracking threshold for the AD2S1210.
        The range of the LOT high threshold, the LSB size, and the default value of the LOT
        high threshold on power-up are dependent on the resolution
        setting of the AD2S1210, and are outlined in Table 19.
        """
        val = self.spi.read(self.LOT_HIGH_THRESHOLD_REG)
        _assert_register_correctness(val)
        return val

    @keyword()
    def set_lot_high_threshold(self, val):
        """The LOT high threshold register determines the loss of position
        tracking threshold for the AD2S1210.
        The range of the LOT high threshold, the LSB size, and the default value of the LOT
        high threshold on power-up are dependent on the resolution
        setting of the AD2S1210, and are outlined in Table 19.
        """
        _assert_register_correctness(val)
        self.spi.write(self.LOT_HIGH_THRESHOLD_REG, val)

    @keyword()
    def get_lot_low_threshold(self):
        """The LOT low threshold register determines the level of hysteresis
        on the loss of position tracking fault detection.

        Table 19 datasheet
        """
        val = self.spi.read(self.LOT_LOW_THRESHOLD_REG)
        _assert_register_correctness(val)
        return val

    @keyword()
    def set_lot_low_threshold(self, val):
        """The LOT low threshold register determines the level of hysteresis
        on the loss of position tracking fault detection.

        Table 19 datasheet
        """
        _assert_register_correctness(val)
        self.spi.write(self.LOT_LOW_THRESHOLD_REG, val)


class ResolverException(Exception):
    """Resolver reported an error code"""

    ERRORS = {
        0: "Configuration parity error",
        1: "Phase error exceeds phase lock range",
        2: "Velocity exceeds maximum tracking rate",
        3: "Tracking error exceeds LOT threshold",
        4: "Sine/cosine inputs exceed DOS mismatch threshold",
        5: "Sine/cosine inputs exceed DOS overrange threshold",
        6: "Sine/cosine inputs below LOS threshold",
        7: "Sine/cosine inputs clipped",
    }

    def __init__(self, message, error_code):
        super().__init__(message)
        self.error_code = error_code

    def __str__(self):
        error = []
        for _i in range(0, 8):
            if self.error_code & (1 << _i) != 0:
                error.append(self.ERRORS[_i])

        return f"Error Code: 0x{self.error_code:02x} ({error})"


def _convert_registers(h: int, l: int, signed=False):
    assert 0 <= h <= 255
    assert 0 <= l <= 255

    integer = int.from_bytes([h, l], "big", signed=signed)

    return integer


def _assert_register_correctness(_reg):
    """Asserts that the MSB is 0 as stated in datasheet"""
    if _reg & (1 << 7) != 0:
        raise Failure(
            f"the 7th bit read from a register should always be 0, otherwise it is an address"
            f"(got {_reg:x})"
        )


def _assert_config_correctness(_conf):
    """Throws an error if config is invalid"""
    if _conf & (1 << 6) == 0:
        raise Failure(
            f"the 6th bit of the control register should always be set to 1, "
            f"(got {_conf:x}) datasheet table 22"
        )


if __name__ == "__main__":
    import csv

    if len(sys.argv) != 3:
        print(f"use the following way:\n\tpython {sys.argv[0]} /dev/USBDEVICE output.tsv")
        sys.exit(1)

    chip = AD2S1210()
    chip.connect(sys.argv[1])
    chip.reset_chip()
    # old_conf = chip.get_config()

    with open(sys.argv[2], "a", encoding="utf8", newline="") as tsv_file:
        tsv = csv.writer(tsv_file, delimiter="\t", lineterminator="\n")
        tsv.writerow(["timestamp", "error", "angle_deg"])

        while True:
            time.sleep(0.05)
            chip.sample()
            ts = time.time()

            err = 0
            try:
                chip.clear_error()
            except ResolverException as e:
                print(f"❌ {e}")
                err = e.error_code

            pos = chip.get_pos()
            pos_deg = pos / (2 ** 16 / 360)

            tsv.writerow([ts, err, pos_deg])
            tsv_file.flush()

            if err != 0:
                continue

            vel = chip.get_velocity()
            print(f"✅ POS {pos:7d} = {pos_deg:>9.2f}°")
