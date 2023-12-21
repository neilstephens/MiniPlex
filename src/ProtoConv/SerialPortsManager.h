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

using buf_t = asio::basic_streambuf<std::allocator<char>>;

class SerialPortsManager
{
public:
	SerialPortsManager(asio::io_context& IOC, const std::vector<std::string>& devs, const std::function<void(buf_t& readbuf)>& read_handler);
	~SerialPortsManager();
	void Write(std::vector<uint8_t>&& data);

private:
	asio::io_context& IOC;
	std::vector<asio::serial_port> ports;
	std::vector<asio::serial_port>::iterator port_it;
	std::vector<buf_t> bufs;
	const std::function<void(buf_t& readbuf)>& ReadHandler;

	void Read(buf_t& buf);
};

#endif // SERIALPORTSMANAGER_H
