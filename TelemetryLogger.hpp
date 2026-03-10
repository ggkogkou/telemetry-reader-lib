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
 * @file   TelemetryLogger.hpp
 * @brief  A telemetry logger class that writes the decoded logs (produced by TelemetryParser) to a CSV file
 * @author Georgios Gkogkou <ggkogkou125@gmail.com>
 */

#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <iomanip>

#include "TelemetryParser.hpp"

class TelemetryLogger {
public:
        /**
         * Contructor of the class
         * @param csvPath The path to the CSV file where the logs will be written in
         */
        explicit TelemetryLogger(const std::string& csvPath);

        /**
         * Final flush before object is destroyed
         */
        ~TelemetryLogger();

        /**
         * Function that logs the decoded data
         * @param tsUnix The UNIX timestamp for synchronization of different scripts/logs
         * @param telemetry The struct of the parameters that are being monitored
         */
        void log(double tsUnix, const RadiationTestTelemetry::Telemetry44& telemetry);

private:
        /**
         * The CSV file that contains the telemetry
         */
        std::ofstream csvFile;

        /**
         * Buffer to save the data before flushing to the file
         */
        std::vector<char> writeBuffer;

        /**
         * Counter for the rows that are buffered before flushing to the CSV file
         */
        std::size_t rowsSinceFlush = 0;

        /**
         * The maximum number of rows to buffer before flushing to the CSV file
         */
        static constexpr std::size_t FlushEveryNRows = 500;

        /**
         * Function that writes the header when the file is freshly created
         */
        void writeHeader();
};
