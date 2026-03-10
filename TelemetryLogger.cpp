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
 * @file   TelemetryLogger.cpp
 * @brief  A telemetry logger class that writes the decoded logs (produced by TelemetryParser) to a CSV file
 * @author Georgios Gkogkou <ggkogkou125@gmail.com>
 */

#include "TelemetryLogger.hpp"

TelemetryLogger::TelemetryLogger(const std::string& csvPath) {
        const auto FileExists = std::filesystem::exists(csvPath);

        const auto FileIsEmpty = [csvPath, FileExists]() -> bool {
                if (FileExists)
                        return std::filesystem::is_empty(csvPath);

                return true;
        }();

        csvFile.open(csvPath, std::ios::out | std::ios::app);

        if (!csvFile.is_open())
                throw std::runtime_error("Failed to open telemetry CSV file: " + csvPath);

        writeBuffer.resize(1 << 20);
        csvFile.rdbuf()->pubsetbuf(writeBuffer.data(), static_cast<std::streamsize>(writeBuffer.size()));

        if (FileIsEmpty)
                writeHeader();
}

TelemetryLogger::~TelemetryLogger() {
        if (csvFile.is_open()) {
                csvFile.flush();
                csvFile.close();
        }
}

void TelemetryLogger::log(double tsUnix, const RadiationTestTelemetry::Telemetry44& t) {
        csvFile << std::fixed << std::setprecision(6) << tsUnix << ',' << t.seq << ',' << t.t_us << ',' << t.ia_mA << ',' << t.ib_mA
                << ',' << t.id_mA << ',' << t.iq_mA << ',' << t.id_ref_mA << ',' << t.iq_ref_mA << ',' << t.angle_raw << ','
                << t.omega_mrad_s << ',' << t.encoder_error_code << '\n';

        rowsSinceFlush++;

        if (rowsSinceFlush >= FlushEveryNRows) {
                csvFile.flush();
                rowsSinceFlush = 0;
        }
}

void TelemetryLogger::writeHeader() {
        csvFile << "ts_unix,"
                << "seq,"
                << "t_us,"
                << "ia_mA,"
                << "ib_mA,"
                << "id_mA,"
                << "iq_mA,"
                << "id_ref_mA,"
                << "iq_ref_mA,"
                << "angle_raw,"
                << "omega_mrad_s,"
                << "encoder_error_code\n";
}
