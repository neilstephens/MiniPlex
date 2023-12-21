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

//TODO: add port options etc.
SerialPortsManager::SerialPortsManager(asio::io_context& IOC, const std::vector<std::string>& devs, const std::function<void(buf_t&)>& read_handler):
	IOC(IOC),
	ReadHandler(read_handler)
{
	for(auto& dev : devs)
	{
		ports.emplace_back(IOC);
		ports.back().open(dev);
		bufs.emplace_back();
		Read(bufs.back());
	}
	port_it = ports.begin();
}

SerialPortsManager::~SerialPortsManager()
{
	//TODO: cancel and wait for outstanding handlers
	for(auto& port : ports)
		port.close();
}

void SerialPortsManager::Read(buf_t& buf)
{
	//TODO: use a strand wrapped handler
	asio::async_read(ports.back(),buf.prepare(65536), asio::transfer_at_least(1), [this,&buf](asio::error_code err, size_t n)
	{
		if(err)
			spdlog::get("ProtoConv")->error("Read {} bytes from serial, return error '{}'.",n,err.message());
		else
		{
			ReadHandler(buf);
			Read(buf);
		}
	});
}

void SerialPortsManager::Write(std::vector<uint8_t>&& data)
{
	auto pBuf = std::make_shared<std::vector<uint8_t>>(std::move(data));

	//TODO: use a strand wrapped handler
	asio::async_write(*port_it,asio::buffer(pBuf->data(),pBuf->size()),asio::transfer_all(),[pBuf](asio::error_code err, size_t n)
	{
		if(err)
			spdlog::get("ProtoConv")->error("Wrote {} bytes to serial, return error '{}'.",n,err.message());
		else
			spdlog::get("ProtoConv")->trace("Wrote {} bytes to serial.",n);
	});

	port_it++;
	if(port_it == ports.end())
		port_it = ports.begin();
}
