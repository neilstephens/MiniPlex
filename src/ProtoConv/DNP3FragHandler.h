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
#ifndef DNP3FRAGHANDLER_H
#define DNP3FRAGHANDLER_H

#include "FragHandler.h"
#include <asio.hpp>
#include <queue>
#include <set>
#include <memory>

struct FragCmp
{
	bool operator()(const Frame& l, const Frame& r)
	{
		if(r.fir) return true;
		if(l.fir) return false;
		if(r.fin) return false;
		if(l.fin) return true;
		if(l.seq > r.seq)
		{
			if((l.seq - r.seq) < (r.seq+64 - l.seq)) return true;
			return false;
		}
		if((r.seq - l.seq) < (l.seq+64 - r.seq)) return false;
		return true;
	}
};

struct TransportFlow
{
	TransportFlow() = delete;
	explicit TransportFlow(asio::io_service& IOC, const uint64_t id):
		strand(IOC),
		id(id),
		frag_q(FragCmp())
	{}
	asio::io_service::strand strand;
	const uint64_t id;
	std::priority_queue<Frame,std::deque<Frame>,FragCmp> frag_q;
	std::shared_ptr<Frame> pNextFrame = nullptr;
	std::set<uint8_t> hasSeq;
	bool hasFin = false;
	bool hasFir = false;
	uint8_t finSeq = 0;
	uint8_t firSeq = 0;
};

class DNP3FragHandler : public FragHandler
{
public:
	DNP3FragHandler(const std::function<void(std::shared_ptr<uint8_t> pBuf, const size_t sz)>& write_handler, asio::io_context& IOC);
	void HandleFrame(const Frame& frame) override;
private:
	void Flush(const std::shared_ptr<TransportFlow>& pTxFlow);
	asio::io_context& IOC;
	asio::io_service::strand marshalling_strand; //TODO: can TransportFlows be lockless threadsafe (like ODC EventDB) - then no need for this strand
	std::unordered_map<uint64_t,std::shared_ptr<TransportFlow>> TransportFlows;
};

inline std::string ToString(std::priority_queue<Frame,std::deque<Frame>,FragCmp> Q)
{
	std::ostringstream oss;
	while(!Q.empty())
	{
		auto f = Q.top();
		Q.pop();
		oss << ((f.fir||f.fin)?"(":"") << (f.fir?"FIR":"") << (f.fin?"FIN":"") << ((f.fir||f.fin)?")":"") << f.seq << (!Q.empty()?" ":"");
	}
	return oss.str();
}

#endif // DNP3FRAGHANDLER_H
