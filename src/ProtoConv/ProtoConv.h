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

#ifndef PROTOCONV_H
#define PROTOCONV_H

#include "FrameChecker.h"
#include "StreamHandler.h"
#include "FragHandler.h"
#include <asio.hpp>
#include <atomic>
#include <memory>
#include <deque>

using buf_t = asio::basic_streambuf<std::allocator<char>>;

//using vector<uint8_t> and reinterpret_cast to serialise other types,
// so let's add a few checks
static_assert(std::is_same<uint8_t, unsigned char>::value,
"We require uint8_t to be implemented as unsigned char");
static_assert(sizeof(uint32_t)==4,
"We require uint32_t to be 4 bytes long");
static_assert(sizeof(uint16_t)==2,
"We require uint16_t to be 2 bytes long");
static_assert(sizeof(uint8_t)==sizeof(char),
"We require uint8_t to be the same size as char");

struct CmdArgs;

class ProtoConv
{
public:
	ProtoConv(const CmdArgs& aArgs, asio::io_context &aIOC);

private:
	using rbuf_t = std::array<uint8_t, 64L * 1024>;

	void RcvUDP();
	void RcvUDPHandler(const asio::error_code err, const uint8_t* const buf, const size_t n);
	void RcvStreamHandler(buf_t& buf);
	void WriteUDPHandler(std::shared_ptr<uint8_t> pBuf, const size_t sz);
	void AddDelim(std::vector<uint8_t>& data);

	const CmdArgs& Args;
	asio::io_context& IOC;

	std::shared_ptr<FrameChecker> pFramer;
	std::shared_ptr<FragHandler> pFragHandler;
	std::shared_ptr<StreamHandler> pStream;

	const asio::ip::udp::endpoint local_ep;
	const asio::ip::udp::endpoint remote_ep;
	asio::ip::udp::socket socket;
	asio::io_context::strand socket_strand;
	asio::io_context::strand process_strand;

	std::deque<std::shared_ptr<rbuf_t>> rcv_buf_q;
	const size_t MaxWriteQSz;
	const uint32_t Delim;
	uint32_t DelimSeq = 0;

	std::atomic_bool stopping = false;
};

#endif // PROTOCONV_H
