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
#include <csignal>

MiniPlex::MiniPlex(const CmdArgs& Args, asio::io_context& IOC):
	Args(Args),
	IOC(IOC),
	local_ep(asio::ip::address::from_string(Args.LocalAddr.getValue()),Args.LocalPort.getValue()),
	socket(IOC,local_ep),
	socket_strand(IOC),
	process_strand(IOC),
	EndPointCache(process_strand,Args.CacheTimeout.getValue(),[](const asio::ip::udp::endpoint& ep)
	{
		auto ep_string = ep.address().to_string()+":"+std::to_string(ep.port());
		spdlog::get("MiniPlex")->debug("Cache entry for {} timed out.",ep_string);
	})
{
	asio::socket_base::receive_buffer_size option(Args.SoRcvBuf.getValue());
	socket.set_option(option);

	if(Args.Trunk || Args.Prune)
	{
		trunk = asio::ip::udp::endpoint(asio::ip::address::from_string(Args.TrunkAddr.getValue()),Args.TrunkPort.getValue());
		spdlog::get("MiniPlex")->info("Operating in Trunk mode to {}:{}",Args.TrunkAddr.getValue(),Args.TrunkPort.getValue());
	}
	else
	{
		spdlog::get("MiniPlex")->info("Operating in Hub mode.");
	}
	for(size_t i=0; i<Args.BranchAddrs.getValue().size(); i++)
	{
		auto branch = asio::ip::udp::endpoint(asio::ip::address::from_string(Args.BranchAddrs.getValue()[i]),Args.BranchPorts.getValue()[i]);
		EndPointCache.Add(branch,true);
	}
	socket_strand.post([this](){Rcv();});
	spdlog::get("MiniPlex")->info("Listening on {}:{}",Args.LocalAddr.getValue(),Args.LocalPort.getValue());
}

void MiniPlex::Rcv()
{
	spdlog::get("MiniPlex")->trace("Rcv(): {} rcv buffers available.",rcv_buf_q.size());
	if(rcv_buf_q.empty())
	{
		spdlog::get("MiniPlex")->debug("Rcv(): Allocating another rcv buffer.");
		rcv_buf_q.push_back(std::make_shared<rbuf_t>());
	}

	auto pBuf = rcv_buf_q.front();
	rcv_buf_q.pop_front();

	auto pRcvSender = std::make_shared<asio::ip::udp::endpoint>();
	socket.async_receive_from(asio::buffer(pBuf->data(),pBuf->size()),*pRcvSender,socket_strand.wrap([this,pBuf,pRcvSender](asio::error_code err, size_t n)
	{
		rx_count++;
		process_strand.post([this,err,pBuf,pRcvSender,n]()
		{
			RcvHandler(err,pBuf->data(),*pRcvSender,n);
			socket_strand.post([this,pBuf]()
			{
				rcv_buf_q.push_back(pBuf);
			});
		});
		Rcv();
	}));
}

void MiniPlex::RcvHandler(const asio::error_code err, const uint8_t* const buf, const asio::ip::udp::endpoint& rcv_sender, const size_t n)
{
	if(err)
	{
		spdlog::get("MiniPlex")->error("RcvHandler(): error code {}: '{}'",err.value(),err.message());
		return;
	}

	auto sender_string = rcv_sender.address().to_string()+":"+std::to_string(rcv_sender.port());
	spdlog::get("MiniPlex")->trace("RcvHandler(): {} bytes from {}",n,sender_string);

	const auto& cache_EPs = EndPointCache.Keys();

	if(Args.Prune && rcv_sender != trunk && !cache_EPs.empty() && rcv_sender != *cache_EPs.begin())
	{
		spdlog::get("MiniPlex")->debug("RcvHandler(): pruned branch {}",sender_string);
		return;
	}

	if(Args.Hub || rcv_sender != trunk)
	{
		auto added = EndPointCache.Add(rcv_sender);
		spdlog::get("MiniPlex")->trace("RcvHandler(): {} cache entry for {}",added?"Inserted":"Refreshed",sender_string);
	}

	// The C++20 way causes a malloc error when asio tries to copy a handler with this style shared_ptr
	//auto pForwardBuf = std::make_shared<uint8_t[]>(n);
	// Use the old way instead - only difference should be the control block is allocated separately
	auto pForwardBuf = std::shared_ptr<uint8_t>(new uint8_t[n],[](uint8_t* p){delete[] p;});
	std::memcpy(pForwardBuf.get(),buf,n);
	if(Args.Hub || rcv_sender == trunk)
	{
		spdlog::get("MiniPlex")->trace("RcvHandler(): Forwarding to {} branches",cache_EPs.size());
		for(const auto& endpoint : cache_EPs)
			if(endpoint != rcv_sender)
				socket_strand.post([this,pForwardBuf,n,ep{endpoint}]()
				{
					socket.async_send_to(asio::buffer(pForwardBuf.get(),n),ep,[pForwardBuf](asio::error_code,size_t){});
				});
	}
	else
	{
		spdlog::get("MiniPlex")->trace("RcvHandler(): Forwarding to trunk");
		socket_strand.post([this,pForwardBuf,n]()
		{
			socket.async_send_to(asio::buffer(pForwardBuf.get(),n),trunk,[pForwardBuf](asio::error_code,size_t){});
		});
	}
}

void MiniPlex::Benchmark()
{
	const size_t sock_pool_count = 100;
	std::vector<asio::ip::udp::socket> sock_pool;
	for(size_t i=0; i<sock_pool_count; i++)
	{
		sock_pool.emplace_back(IOC);
		sock_pool.back().open(asio::ip::udp::v4());
	}
	const auto duration = std::chrono::milliseconds(Args.BenchDuration.getValue());
	const auto start_time = std::chrono::steady_clock::now();
	size_t tx_count = 0;
	auto elapsed = std::chrono::milliseconds::zero();
	do
	{
		if(tx_count < rx_count+50) //assume os can buffer 50 packets
		{
			auto pSock = &sock_pool[tx_count++%sock_pool_count];
			IOC.post([ep{local_ep},pSock]()
			{
				auto pForwardBuf = std::shared_ptr<uint8_t>(new uint8_t[500],[](uint8_t* p){delete[] p;});
				pSock->async_send_to(asio::buffer(pForwardBuf.get(),500),ep,[pForwardBuf](asio::error_code,size_t){});
			});
		}
		else
			IOC.poll_one();
		elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
	}while(elapsed < duration && !IOC.stopped());
	spdlog::get("MiniPlex")->critical("Benchmark(): RX count {} over {}ms.",rx_count,elapsed.count());
	std::raise(SIGINT);
}
