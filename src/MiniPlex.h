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
#include <string>

class CmdArgs;

class MiniPlex
{
public:
	MiniPlex(const CmdArgs& Args, asio::io_context &IOC);
	void Stop();

private:
	void RcvHandler(const asio::error_code err, const size_t n);

	const CmdArgs& Args;
	asio::io_context& IOC;
	asio::ip::udp::socket socket;
	TimeoutCache<asio::ip::udp::endpoint> EndPointCache;
	asio::ip::udp::endpoint trunk;

	std::array<uint8_t,65*1024> rcv_buf;
	asio::ip::udp::endpoint rcv_sender;
};

#endif // MINIPLEX_H
