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

#include "MiniPlex.h"
#include "CmdArgs.h"
#include <asio.hpp>
#include <spdlog/spdlog.h>
#include <memory>

MiniPlex::MiniPlex(const CmdArgs& Args, asio::io_context& IOC):
	Args(Args),
	IOC(IOC),
	socket(IOC,asio::ip::udp::endpoint(asio::ip::address::from_string(Args.LocalAddr.getValue()),Args.LocalPort.getValue())),
	EndPointCache(IOC,Args.CacheTimeout.getValue())
{
	if(Args.Trunk || Args.Prune)
	{
		trunk = asio::ip::udp::endpoint(asio::ip::address::from_string(Args.TrunkAddr.getValue()),Args.TrunkPort.getValue());
	}
	socket.async_receive_from(asio::buffer(rcv_buf),rcv_sender,[this](asio::error_code err, size_t n)
	{
		RcvHandler(err,n);
	});
}

void MiniPlex::Stop()
{
	socket.cancel();
	socket.close();
	EndPointCache.Clear();
}

void MiniPlex::RcvHandler(const asio::error_code err, const size_t n)
{
	if(err)
		return;

	auto sender_string = rcv_sender.address().to_string()+":"+std::to_string(rcv_sender.port());
	spdlog::get("MiniPlex")->trace("RcvHandler(): {} bytes from {}",n,sender_string);

	auto cache_EPs = EndPointCache.Keys();

	if(Args.Prune && rcv_sender != trunk && cache_EPs.size() && rcv_sender != cache_EPs[0])
	{
		spdlog::get("MiniPlex")->debug("RcvHandler(): pruned branch {}",sender_string);
		return;
	}

	if(Args.Hub || rcv_sender != trunk)
	{
		spdlog::get("MiniPlex")->trace("RcvHandler(): refreshed cache for {}",sender_string);
		EndPointCache.Add(rcv_sender);
	}

	auto pForwardBuf = std::make_shared<uint8_t[]>(n);
	if(Args.Hub || rcv_sender == trunk)
	{
		spdlog::get("MiniPlex")->trace("RcvHandler(): Forwarding to {} branches",cache_EPs.size());
		std::memcpy(pForwardBuf.get(),&rcv_buf,n);
		for(const auto& endpoint : cache_EPs)
			if(endpoint != rcv_sender)
				socket.async_send_to(asio::buffer(pForwardBuf.get(),n),endpoint,[pForwardBuf](asio::error_code,size_t){});
	}
	else
	{
		spdlog::get("MiniPlex")->trace("RcvHandler(): Forwarding to trunk");
		socket.async_send_to(asio::buffer(pForwardBuf.get(),n),trunk,[pForwardBuf](asio::error_code,size_t){});
	}

	socket.async_receive_from(asio::buffer(rcv_buf),rcv_sender,[this](asio::error_code err, size_t n)
	{
		RcvHandler(err,n);
	});
}
