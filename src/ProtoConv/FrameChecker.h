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
#ifndef FRAMECHECKER_H
#define FRAMECHECKER_H

#include <asio.hpp>

using buf_t = asio::basic_streambuf<std::allocator<char>>;

struct Frame
{
	explicit Frame(const size_t len,
		const bool fir = true,
		const bool fin = true,
		const uint64_t flow = 0,
		const size_t seq = 0):
		len(len),
		fir(fir),
		fin(fin),
		flow(flow),
		seq(seq)
	{}
	bool isFragment() const { return !(fir && fin);}
	operator bool() { return len == 0; };
	std::shared_ptr<uint8_t> pBuf = nullptr;
	size_t len;
	const bool fir;
	const bool fin;
	const uint64_t flow;
	const size_t seq;
};

struct FrameChecker
{
	virtual ~FrameChecker() = default;
	virtual Frame CheckFrame(const buf_t& readbuf) = 0;
};

#endif // FRAMECHECKER_H
