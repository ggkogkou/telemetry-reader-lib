#include "ResolverLogger.hpp"

ResolverLogger::ResolverLogger(const std::string& csvPath) {
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

ResolverLogger::~ResolverLogger() {
        if (csvFile.is_open()) {
                csvFile.flush();
                csvFile.close();
        }
}

void ResolverLogger::log(double tsUnix, AD2S12101::PositionSample positionSample) {
        constexpr double CountsPerRevolution = 65536.0;
        constexpr double DegreesPerRevolution = 360.0;

        const double positionDeg = static_cast<double>(positionSample.position) / (CountsPerRevolution / DegreesPerRevolution);

        csvFile << std::fixed << std::setprecision(6) << tsUnix << ',' << static_cast<unsigned int>(positionSample.error) << ','
                << positionDeg << '\n';

        rowsSinceFlush++;

        if (rowsSinceFlush >= FlushEveryNRows) {
                csvFile.flush();
                rowsSinceFlush = 0;
        }
}

void ResolverLogger::writeHeader() {
        csvFile << "ts_unix,"
                << "error,"
                << "position_deg\n";
}
