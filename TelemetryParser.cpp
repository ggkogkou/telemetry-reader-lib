#include "TelemetryParser.hpp"

RadiationTestTelemetry::TelemetryParser::TelemetryParser(boost::asio::io_context& ioContext, const std::string& portName,
                                                         std::uint32_t baudRate) : serialPort(ioContext) {
        serialPort.open(portName);
        serialPort.set_option(boost::asio::serial_port_base::baud_rate(baudRate));
        serialPort.set_option(boost::asio::serial_port_base::character_size(8));
        serialPort.set_option(boost::asio::serial_port_base::stop_bits(boost::asio::serial_port_base::stop_bits::one));
        serialPort.set_option(boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none));
        serialPort.set_option(boost::asio::serial_port_base::flow_control(boost::asio::serial_port_base::flow_control::none));

        buffer.reserve(4096);
}

void RadiationTestTelemetry::TelemetryParser::fillBuffer(std::size_t minBytes) {
        static constexpr std::size_t MaximumBytesSequence = 512;

        const std::size_t ReadSize = std::max<std::size_t>(minBytes, MaximumBytesSequence);

        std::vector<std::uint8_t> tmpBuffer(ReadSize);

        const std::size_t BytesRead = serialPort.read_some(boost::asio::buffer(tmpBuffer));

        if (BytesRead > 0)
                buffer.insert(buffer.end(), tmpBuffer.begin(), tmpBuffer.begin() + static_cast<std::ptrdiff_t>(BytesRead));
}

std::uint16_t RadiationTestTelemetry::TelemetryParser::crc16_CCITT(const std::span<std::uint8_t> data, std::uint16_t crc) {
        const auto DataSize = data.size();

        for (std::size_t i = 0; i < DataSize; i++) {
                crc ^= data[i] << 8;

                for (int bit = 0; bit < 8; bit++) {
                        if ((crc & 0x8000U) != 0U)
                                crc = static_cast<std::uint16_t>((crc << 1) ^ 0x1021U);
                        else
                                crc = static_cast<std::uint16_t>(crc << 1);
                }
        }

        return crc;
}

std::optional<std::vector<std::uint8_t>> RadiationTestTelemetry::TelemetryParser::readFrame(seconds_t timeoutSeconds) {
        using namespace RadiationTestTelemetry::PayloadDecodingConstants;

        const auto Deadline = std::chrono::steady_clock::now() + std::chrono::duration<float>(timeoutSeconds);

        while (std::chrono::steady_clock::now() < Deadline) {
                if (buffer.size() < 4) {
                        fillBuffer(4 - buffer.size());
                        continue;
                }

                auto syncIterator = std::search(buffer.begin(), buffer.end(), std::begin(std::array{SYNC_BYTE_0, SYNC_BYTE_1}),
                                                std::end(std::array{SYNC_BYTE_0, SYNC_BYTE_1}));

                if (syncIterator == buffer.end()) {
                        if (not buffer.empty()) {
                                const std::uint8_t LastByte = buffer.back();
                                buffer.clear();
                                buffer.push_back(LastByte);
                        }
                        fillBuffer();
                        continue;
                }

                const auto SyncIdx = static_cast<std::size_t>(std::distance(buffer.begin(), syncIterator));

                if (SyncIdx > 0)
                        buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(SyncIdx));

                if (buffer.size() < 4) {
                        fillBuffer(4 - buffer.size());
                        continue;
                }

                const std::uint8_t Length = buffer[2];

                if (const std::uint8_t MessageType = buffer[3]; MessageType != TYPE_TELEMETRY) {
                        buffer.erase(buffer.begin());
                        continue;
                }

                const std::size_t FrameSize = 2 + 1 + static_cast<std::size_t>(Length) + 2;

                if (buffer.size() < FrameSize) {
                        fillBuffer(FrameSize - buffer.size());
                        continue;
                }

                std::vector frame(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(FrameSize));

                buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(FrameSize));

                std::vector crcData(frame.begin() + 3, frame.begin() + 3 + Length);

                const auto CRC_Calc = crc16_CCITT(crcData);
                const auto CRC_ReceivedLo = static_cast<std::uint16_t>(frame[FrameSize-2]);
                const auto CRC_ReceivedHi = static_cast<std::uint16_t>(frame[FrameSize-1] << 8);
                const auto CRC_Received = static_cast<std::uint16_t>(CRC_ReceivedLo | CRC_ReceivedHi);

                if (CRC_Received != CRC_Calc) {
                        if (not buffer.empty()) {
                                const std::uint8_t LastByte = buffer.back();
                                buffer.clear();
                                buffer.push_back(LastByte);
                        }
                        continue;
                }

                return frame;
        }

        return std::nullopt;
}

bool RadiationTestTelemetry::TelemetryParser::decodePayload44(std::span<std::uint8_t> payload, Telemetry44& parametersMonitor) {

}
