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
	port_settings(devs.size()),
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
		port.set_option(port_settings.at(i).baud_rate);
		port.set_option(port_settings.at(i).character_size);
		port.set_option(port_settings.at(i).flow_control);
		port.set_option(port_settings.at(i).parity);
		port.set_option(port_settings.at(i).stop_bits);

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

void SerialPortsManager::SetBaudRate(const std::vector<size_t>& BRs)
{
	if(BRs.size() != num_ports)
		throw std::invalid_argument("Wrong number of baud rates supplied");

	auto port_settings_it = port_settings.begin();
	for(const auto& br : BRs)
	{
		port_settings_it->baud_rate = asio::serial_port::baud_rate(br);
		port_settings_it++;
	}
}
void SerialPortsManager::SetCharSize(const std::vector<size_t>& CSs)
{
	if(CSs.size() != num_ports)
		throw std::invalid_argument("Wrong number of char sizes supplied");

	auto port_settings_it = port_settings.begin();
	for(const auto& cs : CSs)
	{
		port_settings_it->character_size = asio::serial_port::character_size(cs);
		port_settings_it++;
	}
}
void SerialPortsManager::SetFlowControl(const std::vector<std::string>& FCs)
{
	if(FCs.size() != num_ports)
		throw std::invalid_argument("Wrong number of flow control settings supplied");

	auto port_settings_it = port_settings.begin();
	for(const auto& fc : FCs)
	{
		if(fc == "HARDWARE")
			port_settings_it->flow_control = asio::serial_port::flow_control(asio::serial_port::flow_control::hardware);
		else if(fc == "SOFTWARE")
			port_settings_it->flow_control = asio::serial_port::flow_control(asio::serial_port::flow_control::software);
		else
		{
			if(fc != "NONE")
				spdlog::get("ProtoConv")->error("Unknown flow control '{}'. Setting NONE.",fc);
			port_settings_it->flow_control = asio::serial_port::flow_control(asio::serial_port::flow_control::none);
		}
		port_settings_it++;
	}
}
void SerialPortsManager::SetParity(const std::vector<std::string>& Ps)
{
	if(Ps.size() != num_ports)
		throw std::invalid_argument("Wrong number of parity settings supplied");

	auto port_settings_it = port_settings.begin();
	for(const auto& p : Ps)
	{
		if(p == "EVEN")
			port_settings_it->parity = asio::serial_port::parity(asio::serial_port::parity::even);
		else if(p == "ODD")
			port_settings_it->parity = asio::serial_port::parity(asio::serial_port::parity::odd);
		else
		{
			if(p != "NONE")
				spdlog::get("ProtoConv")->error("Unknown parity '{}'. Setting NONE.",p);
			port_settings_it->parity = asio::serial_port::parity(asio::serial_port::parity::none);
		}
		port_settings_it++;
	}
}
void SerialPortsManager::SetStopBits(const std::vector<std::string>& SBs)
{
	if(SBs.size() != num_ports)
		throw std::invalid_argument("Wrong number of stop bit settings supplied");

	auto port_settings_it = port_settings.begin();
	for(const auto& sb : SBs)
	{
		if(sb == "ONEPOINTFIVE")
			port_settings_it->stop_bits = asio::serial_port::stop_bits(asio::serial_port::stop_bits::onepointfive);
		else if(sb == "TWO")
			port_settings_it->stop_bits = asio::serial_port::stop_bits(asio::serial_port::stop_bits::two);
		else
		{
			if(sb != "ONE")
				spdlog::get("ProtoConv")->error("Unknown stop bits setting '{}'. Setting ONE.",sb);
			port_settings_it->stop_bits = asio::serial_port::stop_bits(asio::serial_port::stop_bits::one);
		}
		port_settings_it++;
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