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
#include "SerialStreamHandler.h"
#include "TCPStreamHandler.h"
#include "TCPSocketManager.h"
#include "NullFrameChecker.h"
#include "DNP3FrameChecker.h"
#include <asio.hpp>
#include <spdlog/spdlog.h>
#include <memory>
#include <csignal>

ProtoConv::ProtoConv(const CmdArgs& Args, asio::io_context& IOC):
	Args(Args),
	IOC(IOC),
	local_ep(asio::ip::address::from_string(Args.LocalAddr.getValue()),Args.LocalPort.getValue()),
	remote_ep(asio::ip::address::from_string(Args.RemoteAddr.getValue()),Args.RemotePort.getValue()),
	socket(IOC,local_ep)
{
	if(Args.FrameProtocol.getValue() == "DNP3")
		pFramer = std::make_shared<DNP3FrameChecker>();
	else if(Args.FrameProtocol.getValue() == "Null")
		pFramer = std::make_shared<NullFrameChecker>();
	else
		throw std::invalid_argument("Unknown frame protocol: "+Args.FrameProtocol.getValue());

	socket.connect(remote_ep);
	spdlog::get("ProtoConv")->info("Sending UDP to {}:{}",Args.RemoteAddr.getValue(),Args.RemotePort.getValue());

	if(Args.TCPAddr.getValue() != "")
	{
		auto ip = Args.TCPAddr.getValue();
		auto prt = std::to_string(Args.TCPPort.getValue());
		auto srv = !Args.TCPisClient.getValue();
		spdlog::get("ProtoConv")->info("Operating in TCP {} mode {}:{}",srv?"Server":"Client",ip,prt);
		auto pSockMan = std::make_shared<TCPSocketManager>(IOC,srv,ip,prt,[this](buf_t& buf){RcvStreamHandler(buf);},[](bool){},1000,true);
		pStream = std::make_shared<TCPStreamHandler>(pSockMan);
	}
	else if(!Args.SerialDevices.getValue().empty())
	{
		spdlog::get("ProtoConv")->info("Operating in Serial mode on {} devices",Args.SerialDevices.getValue().size());
		//TODO: set up serial stream

		pStream = std::make_shared<SerialStreamHandler>();
	}

	RcvUDP();
	spdlog::get("ProtoConv")->info("Listening for UDP on {}:{}",Args.LocalAddr.getValue(),Args.LocalPort.getValue());
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

	std::vector<uint8_t> data(n);
	memcpy(data.data(),rcv_buf.data(),n);
	pStream->Write(std::move(data));

	RcvUDP();
}

void ProtoConv::RcvStreamHandler(buf_t& buf)
{
	spdlog::get("ProtoConv")->trace("RcvStreamHandler(): {} bytes in buffer.",buf.size());
	while(auto frame_len = pFramer->CheckFrame(buf))
	{
		// The C++20 way causes a malloc error when asio tries to copy a handler with this style shared_ptr
		//auto pForwardBuf = std::make_shared<uint8_t[]>(n);
		// Use the old way instead - only difference should be the control block is allocated separately
		auto pForwardBuf = std::shared_ptr<char>(new char[frame_len],[](char* p){delete[] p;});
		buf.sgetn(pForwardBuf.get(),frame_len);

		spdlog::get("ProtoConv")->trace("RcvStreamHandler(): Forwarding frame length {} to UDP",frame_len);
		socket.async_send_to(asio::buffer(pForwardBuf.get(),frame_len),remote_ep,[pForwardBuf](asio::error_code,size_t){});
	}
}

