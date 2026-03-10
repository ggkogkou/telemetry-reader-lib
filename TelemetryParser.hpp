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
 * @file   TelemetryParser.hpp
 * @brief  A telemetry parser class that open a serial port, reads chunks of data and decodes into meaningful data
 * @author Georgios Gkogkou <ggkogkou125@gmail.com>
 */

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
        [[nodiscard]] std::optional<std::vector<std::uint8_t>> readFrame(seconds_t timeoutSeconds = 2.0f);

        /**
         * Function that decodes an already verified payload that is given to it as argument
         *
         * @param frame The telemetry payload (e.g. 44 bytes)
         * @return
         */
        [[nodiscard]] std::optional<Telemetry44> decodePayload44(std::span<std::uint8_t> frame) const;

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

        static std::uint32_t readU32LE(std::span<std::uint8_t> bytes);

        static std::int32_t readI32LE(std::span<std::uint8_t> bytes);
};

} // namespace RadiationTestTelemetry
