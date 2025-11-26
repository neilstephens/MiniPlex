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

#ifndef MINIPLEX_H
#define MINIPLEX_H

#include "TimeoutCache.h"
#include "TinyRISCV64.h"
#include <asio.hpp>
#include <atomic>
#include <deque>
#include <list>
#include <set>
#include <tuple>
#include <functional>

using rbuf_t = std::array<uint8_t, 64L * 1024>;
using p_rbuf_t = std::shared_ptr<rbuf_t>;

struct CmdArgs;

class MiniPlex
{
public:
	MiniPlex(const CmdArgs& Args, asio::io_context &IOC);
	void Benchmark();

private:

	void Rcv();
	void RcvHandler(const asio::error_code err, p_rbuf_t buf, const asio::ip::udp::endpoint& rcv_sender, const size_t n);
	p_rbuf_t MakeSharedBuf(rbuf_t* buf = nullptr);
	template<typename T> void Forward(
		const p_rbuf_t& pBuf,
		const size_t size,
		const asio::ip::udp::endpoint& sender,
		const T& branches,
		const char* desc);
	const std::list<asio::ip::udp::endpoint>& Branches(const asio::ip::udp::endpoint& ep);
	const std::list<asio::ip::udp::endpoint>& AddressBranches(const asio::ip::udp::endpoint& ep, const uint64_t addr, const bool associate = false);
	std::tuple<bool,uint64_t,uint64_t> GetSrcDst(p_rbuf_t buf, const size_t n);

	void Hub(const std::list<asio::ip::udp::endpoint>& branches, const asio::ip::udp::endpoint& rcv_sender, p_rbuf_t buf, const size_t n);
	void Trunk(const std::list<asio::ip::udp::endpoint>& branches, const asio::ip::udp::endpoint& rcv_sender, p_rbuf_t buf, const size_t n);
	void Prune(const std::list<asio::ip::udp::endpoint>& branches, const asio::ip::udp::endpoint& rcv_sender, p_rbuf_t buf, const size_t n);
	void Switch(const std::list<asio::ip::udp::endpoint>& branches, const asio::ip::udp::endpoint& rcv_sender, p_rbuf_t buf, const size_t n);
	std::function<void(const std::list<asio::ip::udp::endpoint>&, const asio::ip::udp::endpoint&, p_rbuf_t, const size_t)> ModeHandler;

	std::atomic_bool stopping = false;

	const CmdArgs& Args;
	asio::io_context& IOC;
	const asio::ip::udp::endpoint local_ep;
	asio::ip::udp::socket socket;
	asio::io_context::strand socket_strand;
	asio::io_context::strand process_strand;
	std::set<asio::ip::udp::endpoint> PermaBranches;
	TimeoutCache<asio::ip::udp::endpoint> ActiveBranches;
	std::unordered_map<uint64_t,TimeoutCache<asio::ip::udp::endpoint>> AddrBranches;
	TinyRISCV64::VM AddrVM;
	std::set<asio::ip::udp::endpoint> InactivePermaBranches;
	asio::ip::udp::endpoint trunk;

	std::deque<p_rbuf_t> rcv_buf_q;
	size_t rcv_buf_count; //not Q size - includes 'in flight' bufs

	//atomic rx/tx counts so Benchmark() can access them 'off strand'
	std::atomic<size_t> rx_count = 0;
	std::atomic<size_t> tx_count = 0;
};

#endif // MINIPLEX_H
