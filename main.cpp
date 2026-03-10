// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Georgios Gkogkou <ggkogkou125@gmail.com>

/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * @file   main.cpp
 * @brief  The main() application entrance for a telemetry parser and logger tool that is used along with the FOC firmware
 * [here](https://gitlab.cern.ch/smm-rme/radiation-tolerant-systems/motor-driver-systems/field-oriented-control-samd21)
 * @author Georgios Gkogkou <ggkogkou125@gmail.com>
 */

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

#include "TelemetryLogger.hpp"
#include "TelemetryParser.hpp"

int main(int argc, char* argv[]) {
        std::string portName;
        std::uint32_t baudRate = 460800;
        float watchdogTimeoutSeconds = 0.0f;
        std::string csvPath;

        for (int i = 1; i < argc; ++i) {
                const std::string arg = argv[i];

                if (arg == "--port") {
                        if (i + 1 >= argc)
                                return 1;

                        portName = argv[++i];
                } else if (arg == "--baud") {
                        if (i + 1 >= argc)
                                return 1;

                        try {
                                baudRate = static_cast<std::uint32_t>(std::stoul(argv[++i]));
                        } catch (...) {
                                return 1;
                        }
                } else if (arg == "--watchdog-timeout-s") {
                        if (i + 1 >= argc)
                                return 1;

                        try {
                                watchdogTimeoutSeconds = std::stof(argv[++i]);

                                if (watchdogTimeoutSeconds < 0.0f)
                                        return 1;
                        } catch (...) {
                                return 1;
                        }
                } else if (arg == "--csv") {
                        if (i + 1 >= argc)
                                return 1;

                        csvPath = argv[++i];
                } else if (arg == "--help" || arg == "-h") {
                        return 0;
                } else {
                        return 1;
                }
        }

        if (portName.empty())
                return 1;

        try {
                boost::asio::io_context io;
                RadiationTestTelemetry::TelemetryParser parser(io, portName, baudRate);

                std::optional<TelemetryLogger> logger;
                if (!csvPath.empty())
                        logger.emplace(csvPath);

                using steadyClock_t = std::chrono::steady_clock;
                using systemClock_t = std::chrono::system_clock;

                const auto watchdogTimeout = std::chrono::duration<float>(watchdogTimeoutSeconds);

                auto lastValidTelemetryTime = steadyClock_t::now();
                bool powerCycleAlreadyRequested = false;

                while (true) {
                        auto frame = parser.readFrame(0.2f);
                        const auto nowSteady = steadyClock_t::now();

                        if (frame.has_value()) {
                                auto telemetry = parser.decodePayload44(*frame);

                                if (telemetry.has_value()) {
                                        lastValidTelemetryTime = nowSteady;

                                        if (logger) {
                                                const auto nowUnix = systemClock_t::now();
                                                const double tsUnix =
                                                        std::chrono::duration<double>(nowUnix.time_since_epoch()).count();

                                                logger->log(tsUnix, *telemetry);
                                        }

                                        if (powerCycleAlreadyRequested) {
                                                std::cout << "CMD TELEMETRY_RECOVERED\n";
                                                std::cout.flush();
                                                powerCycleAlreadyRequested = false;
                                        }
                                }
                        }

                        if (watchdogTimeoutSeconds > 0.0f) {
                                const auto silence = nowSteady - lastValidTelemetryTime;

                                if (!powerCycleAlreadyRequested && silence >= watchdogTimeout) {
                                        const auto silenceSeconds = std::chrono::duration<float>(silence).count();

                                        std::cout << "CMD POWER_CYCLE reason=telemetry_timeout silence_s=" << silenceSeconds << '\n';
                                        std::cout.flush();

                                        powerCycleAlreadyRequested = true;
                                }
                        }
                }
        } catch (...) {
                return 1;
        }

        return 0;
}
