#pragma once

#include <algorithm>
#include <array>
#include <boost/asio.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
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

        using seconds_t = float;

        /**
         * Function that keeps filling the byte buffer till enough data exists
         *
         * Logic behind function:
         *
         * - Until timeout, if less than 4 bytes were buffered, fill more. They should be more than the necessary SUNC0, SYNC1, LEN,
         *      TYPE to be able to be decoded
         * - Search the buffer for SYNC0|SYNC1 for the re-synchronization
         * - Once sync is aligned at buffer[0], inspect len = buffer[2] and msgType = buffer[3] and discard byte if necessary
         * - Compute full frame size: SYNC0 + SYNC1 + LEN + length{type+payload} + CRC_HI + CRC_LO
         * - Validate the aforementioned data; check if they make sense
         * - Compute CRC over msgType + payload, and compare to received CRC
         *      - If CRC fails -> keep only last byte of buffer and continue
         *      - If CRC passes -> return the frame
         *
         * @param timeoutSeconds A timeout for the receiving data
         * @return
         */
        std::optional<std::vector<std::uint8_t>> readFrame(seconds_t timeoutSeconds = 2.0f);

        /**
         * Function that decoded the payload that is given to it as argument
         *
         * @param payload The telemetry payload (e.g. 44 bytes)
         * @param parametersMonitor
         * @return
         */
        bool decodePayload44(std::span<std::uint8_t> payload, Telemetry44& parametersMonitor);

        /**
         * Function that reads some bytes from serial and appends them to the internal buffer
         */
        void fillBuffer(std::size_t minBytes = 1);

        /**
         * Function that computes a CRC-16-CCITT checksum over a byte sequence
         *
         * @param data An std::span containing the bytes sequence
         * @param crc
         * @return
         */
        static std::uint16_t crc16_CCITT(std::span<std::uint8_t> data, std::uint16_t crc = 0xFFFF);
};

} // namespace RadiationTestTelemetry
