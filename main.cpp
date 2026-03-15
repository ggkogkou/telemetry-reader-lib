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
 * @brief  The main() application entrance for a telemetry parser and logger tool
 * @author Georgios Gkogkou <ggkogkou125@gmail.com>
 */

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "AD2S12101.hpp"
#include "ResolverLogger.hpp"
#include "TelemetryLogger.hpp"
#include "TelemetryParser.hpp"

int main(int argc, char* argv[]) {
        std::string telemetryPortName;
        std::uint32_t telemetryBaudRate = 460800;
        std::string telemetryCsvPath;

        std::string resolverPortName;
        std::uint32_t resolverBaudRate = 460800;
        std::string resolverCsvPath;

        float watchdogTimeoutSeconds = 0.0f;

        for (int i = 1; i < argc; ++i) {
                const std::string arg = argv[i];

                if (arg == "--port") {
                        if (i + 1 >= argc)
                                return 1;

                        telemetryPortName = argv[++i];
                } else if (arg == "--telemetry-port") {
                        if (i + 1 >= argc)
                                return 1;

                        telemetryPortName = argv[++i];
                } else if (arg == "--baud") {
                        if (i + 1 >= argc)
                                return 1;

                        try {
                                telemetryBaudRate = static_cast<std::uint32_t>(std::stoul(argv[++i]));
                        } catch (...) {
                                return 1;
                        }
                } else if (arg == "--telemetry-baud") {
                        if (i + 1 >= argc)
                                return 1;

                        try {
                                telemetryBaudRate = static_cast<std::uint32_t>(std::stoul(argv[++i]));
                        } catch (...) {
                                return 1;
                        }
                } else if (arg == "--resolver-port") {
                        if (i + 1 >= argc)
                                return 1;

                        resolverPortName = argv[++i];
                } else if (arg == "--resolver-baud") {
                        if (i + 1 >= argc)
                                return 1;

                        try {
                                resolverBaudRate = static_cast<std::uint32_t>(std::stoul(argv[++i]));
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
                } else if (arg == "--csv" || arg == "--telemetry-csv") {
                        if (i + 1 >= argc)
                                return 1;

                        telemetryCsvPath = argv[++i];
                } else if (arg == "--resolver-csv") {
                        if (i + 1 >= argc)
                                return 1;

                        resolverCsvPath = argv[++i];
                } else if (arg == "--help" || arg == "-h") {
                        return 0;
                } else {
                        return 1;
                }
        }

        if (telemetryPortName.empty() && resolverPortName.empty())
                return 1;

        if (!telemetryCsvPath.empty() && telemetryPortName.empty())
                return 1;

        if (!resolverCsvPath.empty() && resolverPortName.empty())
                return 1;

        try {
                boost::asio::io_context io;
                std::optional<RadiationTestTelemetry::TelemetryParser> telemetryParser;
                if (!telemetryPortName.empty())
                        telemetryParser.emplace(io, telemetryPortName, telemetryBaudRate);

                const bool telemetryEnabled = telemetryParser.has_value();

                std::optional<AD2S12101> resolver;
                if (!resolverPortName.empty()) {
                        resolver.emplace(io, resolverPortName, resolverBaudRate);
                        resolver->initDevice();
                }

                std::optional<TelemetryLogger> telemetryLogger;
                if (!telemetryCsvPath.empty())
                        telemetryLogger.emplace(telemetryCsvPath);

                std::optional<ResolverLogger> resolverLogger;
                if (!resolverCsvPath.empty())
                        resolverLogger.emplace(resolverCsvPath);

                using steadyClock_t = std::chrono::steady_clock;
                using systemClock_t = std::chrono::system_clock;
                constexpr auto resolverPollingInterval = std::chrono::milliseconds(50);

                std::atomic<bool> stopRequested{false};
                std::atomic<bool> telemetryReaderFailed{false};
                std::atomic<bool> resolverReaderFailed{false};

                std::mutex telemetryStateMutex;
                auto lastValidTelemetryTime = steadyClock_t::now();
                bool powerCycleAlreadyRequested = false;

                const auto watchdogTimeout = std::chrono::duration<float>(watchdogTimeoutSeconds);

                std::thread telemetryReaderThread([&]() {
                        try {
                                while (!stopRequested.load()) {
                                        if (!telemetryParser.has_value()) {
                                                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                                continue;
                                        }

                                        auto frame = telemetryParser->readFrame(0.2f);

                                        if (!frame.has_value())
                                                continue;

                                        auto telemetry = telemetryParser->decodePayload44(*frame);
                                        if (!telemetry.has_value())
                                                continue;

                                        bool emitRecovered = false;

                                        {
                                                std::lock_guard<std::mutex> lock(telemetryStateMutex);
                                                lastValidTelemetryTime = steadyClock_t::now();

                                                if (powerCycleAlreadyRequested) {
                                                        emitRecovered = true;
                                                        powerCycleAlreadyRequested = false;
                                                }
                                        }

                                        if (telemetryLogger) {
                                                const auto nowUnix = systemClock_t::now();
                                                const double tsUnix =
                                                        std::chrono::duration<double>(nowUnix.time_since_epoch()).count();

                                                telemetryLogger->log(tsUnix, *telemetry);
                                        }

                                        if (emitRecovered) {
                                                std::cout << "CMD TELEMETRY_RECOVERED\n";
                                                std::cout.flush();
                                        }
                                }
                        } catch (...) {
                                telemetryReaderFailed.store(true);
                                stopRequested.store(true);
                        }
                });

                std::thread resolverReaderThread([&]() {
                        try {
                                while (!stopRequested.load()) {
                                        if (!resolver.has_value()) {
                                                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                                continue;
                                        }

                                        const auto positionSample = resolver->getPositionAndError();
                                        constexpr double CountsPerRevolution = 65536.0;
                                        constexpr double DegreesPerRevolution = 360.0;
                                        const double positionDeg =
                                                static_cast<double>(positionSample.position) / (CountsPerRevolution / DegreesPerRevolution);

                                        if (positionSample.error == 0) {
                                                std::cerr << "\xE2\x9C\x85 Position: " << std::fixed << std::setprecision(2)
                                                          << positionDeg << " deg\n";
                                        } else {
                                                std::cerr << "Resolver error=" << static_cast<unsigned int>(positionSample.error)
                                                          << " Position: " << std::fixed << std::setprecision(2) << positionDeg
                                                          << " deg\n";
                                        }
                                        std::cerr.flush();

                                        if (resolverLogger) {
                                                const auto nowUnix = systemClock_t::now();
                                                const double tsUnix =
                                                        std::chrono::duration<double>(nowUnix.time_since_epoch()).count();

                                                resolverLogger->log(tsUnix, positionSample);
                                        }

                                        std::this_thread::sleep_for(resolverPollingInterval);
                                }
                        } catch (...) {
                                resolverReaderFailed.store(true);
                                stopRequested.store(true);
                        }
                });

                std::thread watchdogThread([&]() {
                        while (!stopRequested.load()) {
                                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                                if (!telemetryEnabled || watchdogTimeoutSeconds <= 0.0f)
                                        continue;

                                bool shouldEmitPowerCycle = false;
                                float silenceSeconds = 0.0f;

                                {
                                        std::lock_guard<std::mutex> lock(telemetryStateMutex);

                                        const auto now = steadyClock_t::now();
                                        const auto silence = now - lastValidTelemetryTime;

                                        if (!powerCycleAlreadyRequested && silence >= watchdogTimeout) {
                                                powerCycleAlreadyRequested = true;
                                                shouldEmitPowerCycle = true;
                                                silenceSeconds = std::chrono::duration<float>(silence).count();
                                        }
                                }

                                if (shouldEmitPowerCycle) {
                                        std::cout << "CMD POWER_CYCLE reason=telemetry_timeout silence_s=" << silenceSeconds << '\n';
                                        std::cout.flush();
                                }
                        }
                });

                while (!stopRequested.load()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(250));
                }

                watchdogThread.join();
                telemetryReaderThread.join();
                resolverReaderThread.join();

                if (telemetryReaderFailed.load() || resolverReaderFailed.load())
                        return 1;

        } catch (...) {
                return 1;
        }

        return 0;
}
