/*	MiniPlex
 *
 *	Copyright (c) 2023: Neil Stephens
 *
 *	Licensed under the Apache License, Version 2.0 (the "License");
 *	you may not use this file except in compliance with the License.
 *	You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 *	Unless required by applicable law or agreed to in writing, software
 *	distributed under the License is distributed on an "AS IS" BASIS,
 *	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *	See the License for the specific language governing permissions and
 *	limitations under the License.
 */
#ifndef SERIALPORTSMANAGER_H
#define SERIALPORTSMANAGER_H

#include <asio.hpp>
#include <vector>
#include <string>
#include <functional>
#include <atomic>
#include <deque>

using buf_t = asio::basic_streambuf<std::allocator<char>>;

struct SerialDeviceSettings
{
	asio::serial_port::baud_rate baud_rate = asio::serial_port::baud_rate(38400);
	asio::serial_port::character_size character_size = asio::serial_port::character_size(8);
	asio::serial_port::flow_control flow_control;
	asio::serial_port::parity parity;
	asio::serial_port::stop_bits stop_bits;
};

class SerialPortsManager
{
public:
	SerialPortsManager(asio::io_context& IOC, const std::vector<std::string>& devs, const std::function<void(buf_t& readbuf)>& read_handler, const size_t MaxWriteQSz);
	~SerialPortsManager();
	void Write(std::vector<uint8_t>&& data);
	void Start();
	void Stop();

	void SetBaudRate(const std::vector<size_t>& BRs);
	void SetCharSize(const std::vector<size_t>& CSs);
	void SetFlowControl(const std::vector<std::string>& FCs);
	void SetParity(const std::vector<std::string>& Ps);
	void SetStopBits(const std::vector<std::string>& SBs);

private:
	asio::io_context& IOC;
	std::vector<asio::io_context::strand> strands;
	std::shared_ptr<void> handler_tracker;
	std::vector<SerialDeviceSettings> port_settings;
	std::vector<asio::serial_port> ports;
	std::deque<size_t> idle_port_idx_q;
	std::deque<std::shared_ptr<std::vector<uint8_t>>> write_q;
	const size_t MaxWriteQSz;
	asio::io_context::strand write_q_strand;
	const size_t num_ports;
	std::vector<buf_t> bufs;
	const std::function<void(buf_t& readbuf)> ReadHandler;

	void Read(asio::io_context::strand& strand, asio::serial_port& port, buf_t& buf, std::shared_ptr<void> tracker);
	void Write(std::shared_ptr<std::vector<uint8_t>> pBuf, const size_t idx, std::shared_ptr<void> tracker);
};

#endif // SERIALPORTSMANAGER_H
