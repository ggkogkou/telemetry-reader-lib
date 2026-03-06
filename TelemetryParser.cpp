#include "TelemetryParser.hpp"

RadiationTestTelemetry::TelemetryParser::TelemetryParser(boost::asio::io_context& ioContext, const std::string& portName,
                                                         std::uint32_t baudRate) : serialPort(ioContext) {
        serialPort.open(portName);
        serialPort.set_option(boost::asio::serial_port_base::baud_rate(baudRate));
        serialPort.set_option(boost::asio::serial_port_base::character_size(8));
        serialPort.set_option(boost::asio::serial_port_base::stop_bits(boost::asio::serial_port_base::stop_bits::one));
        serialPort.set_option(boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none));
        serialPort.set_option(boost::asio::serial_port_base::flow_control(boost::asio::serial_port_base::flow_control::none));
}


