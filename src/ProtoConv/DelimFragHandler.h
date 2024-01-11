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
#ifndef DELIMFRAGHANDLER_H
#define DELIMFRAGHANDLER_H

#include "FragHandler.h"
#include <asio.hpp>
#include <spdlog/spdlog.h>
#include <deque>
#include <queue>

struct FrameCmp
{
	bool operator()(const Frame& l, const Frame& r)
	{
		if(l.seq > r.seq)
		{
			return (l.seq - r.seq) < std::numeric_limits<uint32_t>::max()/2 ? true : false;
		}
		return (r.seq - l.seq) < std::numeric_limits<uint32_t>::max()/2 ? false : true;
	}
};

inline std::string ToString(std::priority_queue<Frame,std::deque<Frame>,FrameCmp> Q)
{
	std::ostringstream oss;
	while(!Q.empty())
	{
		auto f = Q.top();
		Q.pop();
		oss << f.seq << (!Q.empty()?" ":"");
	}
	return oss.str();
}

class DelimFragHandler : public FragHandler
{
public:
	DelimFragHandler(const std::function<void(std::shared_ptr<uint8_t> pBuf, const size_t sz)>& write_handler, asio::io_context& IOC):
		FragHandler(write_handler),
		strand(IOC)
	{}
	void HandleFrame(const Frame& frame) override
	{
		if(frame.flow != 1 || frame.len < 10)//unframed
		{
			spdlog::get("ProtoConv")->warn("DelimFragHandler::HandleFrame(): Passing through unframed data. Flow {}, Len {}.",frame.flow,frame.len);
			WriteHandler(frame.pBuf,frame.len);
			return;
		}

		strand.post([this,frame{frame}]()
		{
			frame_q.emplace(std::move(frame));

			if(frame_q.size() > 100)//TODO: make configurable
			{
				spdlog::get("ProtoConv")->warn("DelimFragHandler::HandleFrame(): Too many out of sequence frames ({}), jumping ahead.",frame_q.size());
				expected_seq = frame_q.top().seq;
			}

			while(!frame_q.empty() && frame_q.top().seq == expected_seq)
			{
				WriteHandler(frame_q.top().pBuf,frame_q.top().len-10); //strip delimiter
				frame_q.pop();
				expected_seq++;
			}

			if(spdlog::get("ProtoConv")->should_log(spdlog::level::trace)) //don't eval if wouldn't log - too expensive
				spdlog::get("ProtoConv")->trace("DelimFragHandler::HandleFrame(): Q: {}",ToString(frame_q));
		});
	}
private:
	asio::io_service::strand strand;
	uint32_t expected_seq = 0;
	std::priority_queue<Frame,std::deque<Frame>,FrameCmp> frame_q;
};

#endif // DELIMFRAGHANDLER_H
