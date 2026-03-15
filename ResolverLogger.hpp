#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <vector>

#include "AD2S12101.hpp"

class ResolverLogger {
public:
        /**
         * Constructor of the class
         * @param csvPath The path to the CSV file where the logs will be written in
         */
        explicit ResolverLogger(const std::string& csvPath);

        /**
         * Destructor of the class; flush and close the file
         */
        ~ResolverLogger();

        /**
         * Function that logs the position and error code
         * @param tsUnix The unix timestamp
         * @param positionSample Struct that contains position and error of sample
         */
        void log(double tsUnix, AD2S12101::PositionSample positionSample);

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
