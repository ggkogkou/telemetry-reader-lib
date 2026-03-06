#pragma once

#include <boost/asio.hpp>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/**
 * @namespace RadiationTestTelemetry
 *
 * All the necessary functions to read from serial and decode the telemetry for the FOC firmware
 */
namespace RadiationTestTelemetry {

/**
 * @namespace PayloadDecodingConstants
 *
 * All the necessary constants that are used to decode the received encoder payload
 */
namespace PayloadDecodingConstants {
inline constexpr std::uint8_t SYNC_BYTE_0 = 0xA5;
inline constexpr std::uint8_t SYNC_BYTE_1 = 0x5A;
inline constexpr std::uint8_t TYPE_TELEMETRY = 0x01;
inline constexpr std::size_t PAYLOAD_44_SIZE = 44;
} // namespace PayloadDecodingConstants

/**
 * @struct Telemetry44
 *
 * A struct that represents the 44-byte payload
 */
struct Telemetry44 {
        std::uint32_t seq = 0;
        std::uint32_t t_us = 0;
        std::int32_t ia_mA = 0;
        std::int32_t ib_mA = 0;
        std::int32_t id_mA = 0;
        std::int32_t iq_mA = 0;
        std::int32_t id_ref_mA = 0;
        std::int32_t iq_ref_mA = 0;
        std::uint32_t angle_raw = 0;
        std::int32_t omega_mrad_s = 0;
        std::uint32_t encoder_error_code = 0;
};

static_assert(sizeof(Telemetry44) == PayloadDecodingConstants::PAYLOAD_44_SIZE, "The Telemetry44 struct is different than 44 bytes");

/**
 * @class TelemetryParser
 *
 * A class that implements the reading and decoding of the FOC firmware telemetry. It uses Boost.Asio to be cross-platform.
 */
class TelemetryParser {
public:
        /**
         * Constructor of the class; fully initializes the serial port using boost.asio
         *
         * @param ioContext
         * @param portName
         * @param baudRate
         */
        TelemetryParser(boost::asio::io_context& ioContext, const std::string& portName, std::uint32_t baudRate);

private:
        /**
         * The serial port object
         *
         * @note Initialized fully in constructor member initializer list
         */
        boost::asio::serial_port serialPort;

        /**
         * A buffer for storing the received data
         */
        std::vector<std::uint8_t> buffer{};
};

} // namespace RadiationTestTelemetry
