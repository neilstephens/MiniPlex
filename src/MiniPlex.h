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
#include <asio.hpp>
#include <atomic>
#include <deque>
#include <list>
#include <set>
#include <functional>

struct CmdArgs;

class MiniPlex
{
public:
	MiniPlex(const CmdArgs& Args, asio::io_context &IOC);
	void Benchmark();

private:
	using rbuf_t = std::array<uint8_t, 64L * 1024>;

	void Rcv();
	void RcvHandler(const asio::error_code err, const uint8_t* const buf, const asio::ip::udp::endpoint& rcv_sender, const size_t n);
	inline std::shared_ptr<uint8_t> MakeSharedBuf(const uint8_t* const buf, const size_t n);
	template<typename T> void Forward(
		const std::shared_ptr<uint8_t>& pBuf,
		const size_t size,
		const asio::ip::udp::endpoint& sender,
		const T& branches,
		const char* desc);
	const std::list<asio::ip::udp::endpoint>& Branches(const asio::ip::udp::endpoint& ep, const std::string& sender_string);

	void Hub(const std::list<asio::ip::udp::endpoint>& branches, const asio::ip::udp::endpoint& rcv_sender, const uint8_t* const buf, const size_t n);
	void Trunk(const std::list<asio::ip::udp::endpoint>& branches, const asio::ip::udp::endpoint& rcv_sender, const uint8_t* const buf, const size_t n);
	void Prune(const std::list<asio::ip::udp::endpoint>& branches, const asio::ip::udp::endpoint& rcv_sender, const uint8_t* const buf, const size_t n);
	std::function<void(const std::list<asio::ip::udp::endpoint>&, const asio::ip::udp::endpoint&, const uint8_t* const, const size_t)> ModeHandler;

	const CmdArgs& Args;
	asio::io_context& IOC;
	const asio::ip::udp::endpoint local_ep;
	asio::ip::udp::socket socket;
	asio::io_context::strand socket_strand;
	asio::io_context::strand process_strand;
	std::set<asio::ip::udp::endpoint> PermaBranches;
	TimeoutCache<asio::ip::udp::endpoint> ActiveBranches;
	std::set<asio::ip::udp::endpoint> InactivePermaBranches;
	asio::ip::udp::endpoint trunk;

	std::deque<std::shared_ptr<rbuf_t>> rcv_buf_q;

	std::atomic_bool stopping = false;

	std::atomic<size_t> rx_count = 0;
};

#endif // MINIPLEX_H
