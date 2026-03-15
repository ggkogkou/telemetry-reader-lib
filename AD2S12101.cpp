#include "AD2S12101.hpp"

AD2S12101::AD2S12101(boost::asio::io_context& ioContext, const std::string& portName, std::uint32_t baudRate) : serialPort(ioContext) {
        serialPort.open(portName);
        serialPort.set_option(boost::asio::serial_port_base::baud_rate(baudRate));
        serialPort.set_option(boost::asio::serial_port_base::character_size(8));
        serialPort.set_option(boost::asio::serial_port_base::stop_bits(boost::asio::serial_port_base::stop_bits::one));
        serialPort.set_option(boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none));
        serialPort.set_option(boost::asio::serial_port_base::flow_control(boost::asio::serial_port_base::flow_control::none));
}

void AD2S12101::initDevice() {
        commandToResetDevice();

        std::this_thread::sleep_for(std::chrono::seconds(1));

        commandToSampleDevice();
        commandToReadFromRegister(RegisterAddress::REG_1);
        commandToSampleDevice();
        commandToReadFromRegister(RegisterAddress::REG_1);
        commandToWriteToRegister(RegisterAddress::REG_2, 0x7f);

        [[maybe_unused]] const auto StartupResponse = readFromSerial<2>();
}

uint32_t AD2S12101::getPosition() {
        return getPositionAndError().position;
}

AD2S12101::PositionSample AD2S12101::getPositionAndError() {
        commandToSampleDevice();
        commandToReadFromRegister(RegisterAddress::REG_3);
        commandToReadFromRegister(RegisterAddress::REG_4);
        commandToReadFromRegister(RegisterAddress::REG_1);

        const auto RegisterContents = readFromSerial<3>();
        const auto Position = static_cast<std::uint16_t>((static_cast<std::uint16_t>(RegisterContents[0]) << 8) |
                                                         static_cast<std::uint16_t>(RegisterContents[1]));

        return {.position = Position, .error = RegisterContents[2]};
}

void AD2S12101::commandToResetDevice() {
        static constexpr std::array cmd{ResetCmd};
        writeToSerial(cmd);
}

void AD2S12101::commandToSampleDevice() {
        static constexpr std::array cmd{SampleCmd};
        writeToSerial(cmd);
}

void AD2S12101::commandToReadFromRegister(RegisterAddress addr) {
        const std::array rBuffer{ReadCmd, static_cast<RegisterAddressType_t>(addr)};
        writeToSerial(rBuffer);
}

void AD2S12101::commandToWriteToRegister(RegisterAddress addr, std::uint8_t value) {
        std::array wBuffer{WriteCmd, static_cast<RegisterAddressType_t>(addr), value};
        writeToSerial(wBuffer);
}

void AD2S12101::writeToSerial(std::span<const std::uint8_t> word) {
        boost::asio::write(serialPort, boost::asio::buffer(word.data(), word.size()));
}
