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
 * @file   AD2S12101.hpp
 * @brief  A C++ implementation of the initial driver for the AD2S12101 device
 *
 * @note The original code can be found here: https://gitlab.cern.ch/mro/common/tools/tests/ad2s1210/rp-firmware
 * @note THe raspberry pi must be flashed separately for this to work
 *
 * @author Georgios Gkogkou <ggkogkou125@gmail.com>
 */

#pragma once

#include <algorithm>
#include <array>
#include <boost/asio.hpp>
#include <chrono>
#include <cstdint>
#include <span>
#include <string>
#include <thread>

class AD2S12101 {
public:
        /**
         * Constructor of the class; fully initializes the serial port using boost.asio
         *
         * @param ioContext
         * @param portName
         * @param baudRate
         */
        AD2S12101(boost::asio::io_context& ioContext, const std::string& portName, std::uint32_t baudRate);

        using RegisterAddressType_t = std::uint8_t;

        /**
         * @enum RegisterAddress
         *
         * An enum class that contains the addresses for the device registers
         */
        enum class RegisterAddress : RegisterAddressType_t {
                REG_1 = 0xff,
                REG_2 = 0x92,
                REG_3 = 0x80,
                REG_4 = 0x81,
        };

        /**
         * Function that initializes the device
         *
         * IMPORTANT: Run once, call this before everything else
         */
        void initDevice();

        /**
         * @struct PositionSample
         * Contains the position and error code
         */
        struct PositionSample {
                std::uint16_t position = 0;
                std::uint8_t error = 0;
        };

        /**
         * Function that reads the position from the resolver
         * @return The position (in raw value)
         */
        [[nodiscard]] uint32_t getPosition();

        /**
         * Function that reads the position from resolver and error code from the device
         * @return A struct containing both the position and the error code
         */
        [[nodiscard]] PositionSample getPositionAndError();

private:
        /**
         * The byte that corresponds to read operation
         */
        static constexpr std::uint8_t ReadCmd = 'R';

        /**
         * The byte that corresponds to write operation
         */
        static constexpr std::uint8_t WriteCmd = 'W';

        /**
         * The byte that corresponds to sample operation
         */
        static constexpr std::uint8_t SampleCmd = 's';

        /**
         * The byte that corresponds to reset operation
         */
        static constexpr std::uint8_t ResetCmd = 'r';

        /**
         * Function that writes to serial
         * @param word
         */
        void writeToSerial(std::span<const std::uint8_t> word);

        /**
         * Function that reads from serial
         *
         * @tparam N The number of bytes to read from serial
         * @return The response
         */
        template <std::size_t N>
        [[nodiscard]] std::array<std::uint8_t, N> readFromSerial() {
                std::array<std::uint8_t, N> response;
                boost::asio::read(serialPort, boost::asio::buffer(response));

                return response;
        }

        /**
         * Function that commands the reset of the device
         */
        void commandToResetDevice();

        /**
         * Function that commands to sample the device
         */
        void commandToSampleDevice();

        /**
         * Function that commands to read from the register with address
         * @param addr The register address to read from
         */
        void commandToReadFromRegister(RegisterAddress addr);

        /**
         * Function that commands to write to the register with address addr the value
         * @param addr The register address to write to
         * @param value The value to write to that register
         */
        void commandToWriteToRegister(RegisterAddress addr, std::uint8_t value);

        /**
         * The serial port that acts as the communication channel with Raspberry Pi Pico
         *
         * @note Initialized fully in constructor member initializer list
         */
        boost::asio::serial_port serialPort;
};
