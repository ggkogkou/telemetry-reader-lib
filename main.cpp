#include "TelemetryParser.hpp"
#include <iostream>

int main() {
        try {
                boost::asio::io_context io;
                RadiationTestTelemetry::TelemetryParser parser(io, "COM3", 460800);
                std::cout << "Serial parser initialized\n";
        } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << '\n';
                return 1;
        }

        return 0;
}
