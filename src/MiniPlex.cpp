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
	ActiveBranches(process_strand,Args.CacheTimeout.getValue(),[this](const asio::ip::udp::endpoint& ep)
	{
		if(PermaBranches.contains(ep))
			InactivePermaBranches.insert(ep);
		auto ep_string = ep.address().to_string()+":"+std::to_string(ep.port());
		spdlog::get("MiniPlex")->debug("Cache entry for {} timed out.",ep_string);
	}),
	AddrVM(4096)
{
	asio::socket_base::receive_buffer_size option(Args.SoRcvBuf.getValue());
	socket.set_option(option);

	if(Args.Hub)
	{
		spdlog::get("MiniPlex")->info("Operating in Hub mode.");
		ModeHandler = std::bind(&MiniPlex::Hub,this,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4);
	}
	else if(Args.Trunk)
	{
		spdlog::get("MiniPlex")->info("Operating in Trunk mode.");
		ModeHandler = std::bind(&MiniPlex::Trunk,this,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4);
	}
	else if(Args.Prune)
	{
		spdlog::get("MiniPlex")->info("Operating in Prune mode.");
		ModeHandler = std::bind(&MiniPlex::Prune,this,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4);
	}
	else if(Args.Switch)
	{
		spdlog::get("MiniPlex")->info("Operating in Switch mode.");
		ModeHandler = std::bind(&MiniPlex::Switch,this,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4);
		try
		{
			AddrVM.program_load(Args.ByteCodeFile.getValue());
		}
		catch (const std::exception& e)
		{
			spdlog::get("MiniPlex")->critical("Switch mode VM load/validate error: {}",e.what());
			throw std::exception(e);
		}
	}
	else
		throw std::runtime_error("Mode error");

	if(Args.Trunk || Args.Prune)
	{
		trunk = asio::ip::udp::endpoint(asio::ip::address::from_string(Args.TrunkAddr.getValue()),Args.TrunkPort.getValue());
		spdlog::get("MiniPlex")->info("Trunking to {}:{}",Args.TrunkAddr.getValue(),Args.TrunkPort.getValue());
	}

	for(size_t i=0; i<Args.BranchAddrs.getValue().size(); i++)
	{
		auto branch = asio::ip::udp::endpoint(asio::ip::address::from_string(Args.BranchAddrs.getValue()[i]),Args.BranchPorts.getValue()[i]);
		PermaBranches.insert(branch);
		InactivePermaBranches.insert(branch);
	}

	socket_strand.post([this](){Rcv();});
	spdlog::get("MiniPlex")->info("Listening on {}:{}",Args.LocalAddr.getValue(),Args.LocalPort.getValue());
}

void MiniPlex::Rcv()
{
	spdlog::get("MiniPlex")->trace("Rcv(): {} rcv buffers available.",rcv_buf_q.size());
	if(rcv_buf_q.empty()) [[unlikely]]
	{
		if(rcv_buf_count < Args.MaxProcessQ.getValue())
		{
			spdlog::get("MiniPlex")->debug("Rcv(): Allocating another datagram buffer.");
			rcv_buf_q.push_back(MakeSharedBuf());
			rcv_buf_count++;
		}
		else [[unlikely]]
		{
			spdlog::get("MiniPlex")->debug("Rcv(): Max datagram buffers allocated. Delaying read.");
			//the process strand should be posting writes - so wait for write availabiltiy as a way to delay
			socket.async_wait(asio::ip::udp::socket::wait_write,socket_strand.wrap([this](asio::error_code)
			{
				Rcv();
			}));
			return;
		}
	}

	auto pBuf = rcv_buf_q.front();
	rcv_buf_q.pop_front();

	auto pRcvSender = std::make_shared<asio::ip::udp::endpoint>();
	socket.async_receive_from(asio::buffer(pBuf->data(),pBuf->size()),*pRcvSender,socket_strand.wrap([this,pBuf,pRcvSender](asio::error_code err, size_t n)
	{
		rx_count++;
		process_strand.post([this,err,pBuf,pRcvSender,n]()
		{
			RcvHandler(err,pBuf,*pRcvSender,n);
		});
		Rcv();
	}));
}

void MiniPlex::RcvHandler(const asio::error_code err, p_rbuf_t buf, const asio::ip::udp::endpoint& rcv_sender, const size_t n)
{
	if(err) [[unlikely]]
	{
		spdlog::get("MiniPlex")->error("RcvHandler(): error code {}: '{}'",err.value(),err.message());
		return;
	}
	if(spdlog::get("MiniPlex")->should_log(spdlog::level::trace)) [[unlikely]]
	{
		auto sender_string = rcv_sender.address().to_string()+":"+std::to_string(rcv_sender.port());
		spdlog::get("MiniPlex")->trace("RcvHandler(): {} bytes from {}",n,sender_string);
	}

	const auto& branches = Branches(rcv_sender);
	ModeHandler(branches,rcv_sender,buf,n);
}

void MiniPlex::Hub(const std::list<asio::ip::udp::endpoint>& branches, const asio::ip::udp::endpoint& rcv_sender, p_rbuf_t buf, const size_t n)
{
	Forward(buf,n,rcv_sender,branches,"active branches");
	Forward(buf,n,rcv_sender,InactivePermaBranches,"inactive fixed branches");
}

void MiniPlex::Trunk(const std::list<asio::ip::udp::endpoint>& branches, const asio::ip::udp::endpoint& rcv_sender, p_rbuf_t buf, const size_t n)
{
	if(rcv_sender == trunk)
	{
		Forward(buf,n,rcv_sender,branches,"active branches");
		Forward(buf,n,rcv_sender,InactivePermaBranches,"inactive fixed branches");
	}
	else
		Forward(buf,n,rcv_sender,std::list{trunk},"trunk");
}

void MiniPlex::Prune(const std::list<asio::ip::udp::endpoint>& branches, const asio::ip::udp::endpoint& rcv_sender, p_rbuf_t buf, const size_t n)
{
	if(rcv_sender != trunk && !branches.empty() && rcv_sender != *branches.begin()) [[unlikely]]
	{
		auto sender_string = rcv_sender.address().to_string()+":"+std::to_string(rcv_sender.port());
		spdlog::get("MiniPlex")->debug("Prune(): pruned packet from branch {}",sender_string);
		return;
	}
	if(rcv_sender == trunk)
	{
		if(branches.empty())
			Forward(buf,n,rcv_sender,PermaBranches,"fixed branches");
		else
			Forward(buf,n,rcv_sender,std::list{*branches.begin()},"active prune branch");
	}
	else
		Forward(buf,n,rcv_sender,std::list{trunk},"trunk");
}

void MiniPlex::Switch(const std::list<asio::ip::udp::endpoint>& branches, const asio::ip::udp::endpoint& rcv_sender, p_rbuf_t buf, const size_t n)
{
	auto [success,src,dst] = GetSrcDst(buf,n);
	if(!success) [[unlikely]]
	{
		auto sender_string = rcv_sender.address().to_string()+":"+std::to_string(rcv_sender.port());
		spdlog::get("MiniPlex")->error("Switch(): Failed to get packet addresses. Branch {}.",sender_string);
		return;
	}

	//Get any active branches for src and dst
	// and make sure the sender branch is associated with src addr
	const auto& src_branches = AddressBranches(rcv_sender,src,true);
	const auto& dst_branches = AddressBranches(rcv_sender,dst);

	//Only accept packets for a src address if it's on the active branch for that addr
	const auto& active_src_branch = *src_branches.begin();
	if(active_src_branch != rcv_sender) [[unlikely]]
	{
		auto sender_string = rcv_sender.address().to_string()+":"+std::to_string(rcv_sender.port());
		spdlog::get("MiniPlex")->debug("Switch(): dropped packet from branch {} - another branch is active for src addr ({})",sender_string,src);
		return;
	}

	//Check if there's an active branch for the dst addr
	if(dst_branches.empty()) [[unlikely]]
	{
		//No active branch for that addr - broadcast it to all
		Forward(buf,n,rcv_sender,branches,"active branches");
		Forward(buf,n,rcv_sender,InactivePermaBranches,"inactive fixed branches");
		return;
	}
	//Otherwise just send it to the active associated branch
	Forward(buf,n,rcv_sender,std::list{*dst_branches.begin()},"active address branch");
}

//returns: success, src, dst
std::tuple<bool,uint64_t,uint64_t> MiniPlex::GetSrcDst(p_rbuf_t buf, const size_t n)
{
	try
	{
		auto virt_buf = AddrVM.map_data_mem(buf->data(),n);
		auto src_addr = AddrVM.stack_push<uint64_t>(0);
		auto dst_addr = AddrVM.stack_push<uint64_t>(0);
		AddrVM.register_set(10,virt_buf); //a0=&buf;
		AddrVM.register_set(11,n);        //a1=n;
		AddrVM.register_set(12,src_addr); //a2=&src;
		AddrVM.register_set(13,dst_addr); //a3=&dst;
		AddrVM.execute_program();
		auto res = AddrVM.register_get(10);
		auto dst = AddrVM.stack_pop<uint64_t>();
		auto src = AddrVM.stack_pop<uint64_t>();
		return {res==0,src,dst};
	}
	catch(const std::exception& e) { spdlog::get("MiniPlex")->error("GetSrcDst(): VM execution error: {}",e.what()); }
	return {false,0,0};
}

p_rbuf_t MiniPlex::MakeSharedBuf(rbuf_t* buf)
{
	auto recycler = socket_strand.wrap([this](rbuf_t* p)
	{
		if(!stopping)
			rcv_buf_q.push_back(MakeSharedBuf(p));
		else
			delete p;
	});
	return p_rbuf_t(buf ? buf : new rbuf_t, recycler);
}

template<typename T> void MiniPlex::Forward(
	const p_rbuf_t& pBuf,
	const size_t size,
	const asio::ip::udp::endpoint& sender,
	const T& branches,
	const char* desc)
{
	spdlog::get("MiniPlex")->trace("Forward(): sending to {} {}",branches.size(),desc);
	for(const auto& endpoint : branches)
		if(endpoint != sender)
			socket_strand.post([this,pBuf,size,ep{endpoint}]()
			{
				socket.async_send_to(asio::buffer(pBuf.get(),size),ep,[pBuf](asio::error_code,size_t){});
				tx_count++;
			});
}

//Cache or refresh given branch endpoint, and return the list
const std::list<asio::ip::udp::endpoint>& MiniPlex::Branches(const asio::ip::udp::endpoint& ep)
{
	if(ep != trunk)
	{
		auto added = ActiveBranches.Add(ep);
		if(added)
		{
			InactivePermaBranches.erase(ep);
			auto ep_string = ep.address().to_string()+":"+std::to_string(ep.port());
			spdlog::get("MiniPlex")->debug("Branches(): New cache entry for {}",ep_string);
		}
		else if(spdlog::get("MiniPlex")->should_log(spdlog::level::trace))
		{
			auto ep_string = ep.address().to_string()+":"+std::to_string(ep.port());
			spdlog::get("MiniPlex")->trace("Branches(): Refreshed cache entry for {}",ep_string);
		}
	}
	return ActiveBranches.Keys();
}

//Cache or refresh given branch endpoint, and return the list
const std::list<asio::ip::udp::endpoint>& MiniPlex::AddressBranches(const asio::ip::udp::endpoint& ep, const uint64_t addr, const bool associate)
{
	//Add cache for address if it doesn't already exist
	if(!AddrBranches.contains(addr)) [[unlikely]]
	{
		AddrBranches.emplace(addr,TimeoutCache<asio::ip::udp::endpoint>(process_strand,Args.CacheTimeout.getValue(),[addr](const asio::ip::udp::endpoint& cache_ep)
		{
			auto ep_string = cache_ep.address().to_string()+":"+std::to_string(cache_ep.port());
			spdlog::get("MiniPlex")->debug("Address ({}) cache timeout for branch {}.",addr,ep_string);
		}));
	}

	//optionally associate the branch with the address
	if(associate)
	{
		auto added = AddrBranches.at(addr).Add(ep);
		if(added)
		{
			auto ep_string = ep.address().to_string()+":"+std::to_string(ep.port());
			spdlog::get("MiniPlex")->debug("AddressBranches(): New branch ({}) for address {}", ep_string, addr);
		}
		else if(spdlog::get("MiniPlex")->should_log(spdlog::level::trace))
		{
			auto ep_string = ep.address().to_string()+":"+std::to_string(ep.port());
			spdlog::get("MiniPlex")->trace("AddressBranches(): Refreshed branch ({}) for address {}", ep_string, addr);
		}
	}

	return AddrBranches.at(addr).Keys();
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
	size_t spoof_count = 0;
	auto elapsed = std::chrono::milliseconds::zero();
	do
	{
		if(spoof_count < rx_count+50) //assume os can buffer 50 packets
		{
			auto pSock = &sock_pool[spoof_count++%sock_pool_count];
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
	spdlog::get("MiniPlex")->critical("Benchmark(): RX/TX count {}/{} over {}ms.",rx_count,tx_count,elapsed.count());
	std::raise(SIGINT);
}
