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
#include "SerialPortsManager.h"
#include <spdlog/spdlog.h>

//TODO: Use handler tracker
SerialPortsManager::SerialPortsManager(asio::io_context& IOC, const std::vector<std::string>& devs, const std::function<void(buf_t&)>& read_handler):
	IOC(IOC),
	handler_tracker(std::make_shared<char>()),
	port_write_idx(0),
	num_ports(devs.size()),
	bufs(devs.size()),
	ReadHandler(read_handler)
{
	for(auto& dev : devs)
	{
		ports.emplace_back(IOC);
		ports.back().open(dev);
		strands.emplace_back(IOC);
	}
}

void SerialPortsManager::Start()
{
	size_t i = 0;
	for(auto& port : ports)
	{
		//TODO: add serial port settings to args
		port.set_option(asio::serial_port::baud_rate(9600));
		port.set_option(asio::serial_port::character_size(8));
		port.set_option(asio::serial_port::flow_control(asio::serial_port::flow_control::none));
		port.set_option(asio::serial_port::parity(asio::serial_port::parity::none));
		port.set_option(asio::serial_port::stop_bits(asio::serial_port::stop_bits::one));

		auto& strand = strands.at(i);
		auto& buf = bufs.at(i);
		strand.post([this,&strand,&port,&buf]()
		{
			Read(strand,port,buf);
		});
		i++;
	}
}

void SerialPortsManager::Stop()
{
	for(auto& port : ports)
	{
		asio::error_code err;
		port.close(err);
		if(err)
			spdlog::get("ProtoConv")->error("Close serial port error '{}'.",err.message());
	}
}

SerialPortsManager::~SerialPortsManager()
{
	//TODO: wait for outstanding handlers
}

void SerialPortsManager::Read(asio::io_context::strand& strand, asio::serial_port& port, buf_t& buf)
{
	asio::async_read(port,buf.prepare(65536), asio::transfer_at_least(1), strand.wrap([this,&buf,&port,&strand](asio::error_code err, size_t n)
	{
		if(err)
			spdlog::get("ProtoConv")->error("Read {} bytes from serial, return error '{}'.",n,err.message());
		else
		{
			buf.commit(n);
			ReadHandler(buf);
			Read(strand,port,buf);
		}
	}));
}

void SerialPortsManager::Write(std::vector<uint8_t>&& data)
{
	auto idx = port_write_idx++ % num_ports;
	auto& port = ports[idx];
	auto& strand = strands[idx];
	auto pBuf = std::make_shared<std::vector<uint8_t>>(std::move(data));

	strand.post([pBuf,&port]()
	{
		asio::async_write(port,asio::buffer(pBuf->data(),pBuf->size()),asio::transfer_all(),[pBuf](asio::error_code err, size_t n)
		{
			if(err)
				spdlog::get("ProtoConv")->error("Wrote {} bytes to serial, return error '{}'.",n,err.message());
			else
				spdlog::get("ProtoConv")->trace("Wrote {} bytes to serial.",n);
		});
	});
}
