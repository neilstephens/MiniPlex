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

#include "ProtoConv.h"
#include "CmdArgs.h"
#include <asio.hpp>
#include <spdlog/spdlog.h>
#include <memory>
#include <csignal>

ProtoConv::ProtoConv(const CmdArgs& Args, asio::io_context& IOC):
	Args(Args),
	IOC(IOC),
	local_ep(asio::ip::address::from_string(Args.LocalAddr.getValue()),Args.LocalPort.getValue()),
	socket(IOC,local_ep)
{
	if(Args.TCPAddr.getValue() != "")
	{
		spdlog::get("ProtoConv")->info("Operating in TCP mode to {}:{}",Args.TCPAddr.getValue(),Args.TCPPort.getValue());
		//TODO: setup tcp stream
	}
	else if(Args.SerialDevice.getValue() != "")
	{
		spdlog::get("ProtoConv")->info("Operating in Serial mode on {}",Args.SerialDevice.getValue());
		//TODO: set up serial stream
	}

	if(Args.FrameProtocol.getValue() == "DNP3")
	{
		//TODO: setup framer
	}
	else
	{
		throw std::invalid_argument("Unknown frame protocol: "+Args.FrameProtocol.getValue());
	}

	socket.connect(remote_ep);
	spdlog::get("ProtoConv")->info("Sending to {}:{}",Args.RemoteAddr.getValue(),Args.RemotePort.getValue());
	RcvUDP();
	spdlog::get("ProtoConv")->info("Listening on {}:{}",Args.LocalAddr.getValue(),Args.LocalPort.getValue());
}

void ProtoConv::RcvUDP()
{
	socket.async_receive(asio::buffer(rcv_buf.data(),rcv_buf.size()),[this](asio::error_code err, size_t n)
	{
		RcvUDPHandler(err,n);
	});
}

void ProtoConv::RcvUDPHandler(const asio::error_code err, const size_t n)
{
	if(err)
	{
		spdlog::get("ProtoConv")->error("RcvUDPHandler(): error code {}: '{}'",err.value(),err.message());
		RcvUDP();
		return;
	}
	spdlog::get("ProtoConv")->trace("RcvUDPHandler(): {} bytes.",n);

	//TODO: write to stream

	RcvUDP();
}

