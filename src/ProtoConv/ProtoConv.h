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
#include <asio.hpp>
#include <atomic>
#include <memory>

typedef asio::basic_streambuf<std::allocator<char>> buf_t;

struct CmdArgs;

class ProtoConv
{
public:
	ProtoConv(const CmdArgs& Args, asio::io_context &IOC);

private:

	void RcvUDP();
	void RcvUDPHandler(const asio::error_code err, const size_t n);

	const CmdArgs& Args;
	asio::io_context& IOC;

	std::shared_ptr<FrameChecker> pFramer;
	std::shared_ptr<StreamHandler> pStream;

	const asio::ip::udp::endpoint local_ep;
	const asio::ip::udp::endpoint remote_ep;
	asio::ip::udp::socket socket;

	std::array<uint8_t, 64L * 1024> rcv_buf{};

	std::atomic_bool stopping = false;
};

#endif // PROTOCONV_H