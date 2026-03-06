#include <iostream>
#include "TelemetryParser.hpp"

int main() {
        try {
                boost::asio::io_context io;
                RadiationTestTelemetry::TelemetryParser parser(io, "COM3", 460800);

                while (true) {
                        auto frame = parser.readFrame(2.0);

                        if (!frame.has_value()) {
                                std::cout << "Timeout waiting for frame\n";
                                continue;
                        }

                        auto telemetry = parser.decodePayload44(*frame);

                        if (!telemetry.has_value()) {
                                std::cout << "Received non-44-byte payload\n";
                                continue;
                        }

                        std::cout << "seq=" << telemetry->seq << " t_us=" << telemetry->t_us << " ia_mA=" << telemetry->ia_mA
                                  << " ib_mA=" << telemetry->ib_mA << " omega_mrad_s=" << telemetry->omega_mrad_s
                                  << " encoder_error_code=" << telemetry->encoder_error_code << '\n';
                }
        } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << '\n';
                return 1;
        }

        return 0;
}
