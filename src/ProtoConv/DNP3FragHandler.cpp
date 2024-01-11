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
#include "DNP3FragHandler.h"
#include <spdlog/spdlog.h>

DNP3FragHandler::DNP3FragHandler(const std::function<void(std::shared_ptr<uint8_t> pBuf, const size_t sz)>& write_handler, asio::io_context& IOC):
	FragHandler(write_handler),
	IOC(IOC),
	marshalling_strand(IOC)
{}

void DNP3FragHandler::HandleFrame(const Frame& frame)
{
	if(frame.flow == 0)//unframed
	{
		spdlog::get("ProtoConv")->debug("DNP3FragHandler::HandleFrame(): Passing through unframed data.");
		WriteHandler(frame.pBuf,frame.len);
		return;
	}

	spdlog::get("ProtoConv")->trace("DNP3FragHandler::HandleFrame(): Got frame for flow 0x{:08x}.",frame.flow);
	marshalling_strand.post([this,frame{Frame(frame)}]()
	{
		auto& pTxFlow = TransportFlows[frame.flow];
		if(!pTxFlow)
		{
			spdlog::get("ProtoConv")->debug("DNP3FragHandler::HandleFrame(): Initialising new flow queue 0x{:08x}.",frame.flow);
			pTxFlow = std::make_shared<TransportFlow>(IOC,frame.flow);
		}
		pTxFlow->strand.post([this,pTxFlow,frame{std::move(frame)}]
		{
			if(frame.isFragment())
			{
				if((pTxFlow->hasFir && frame.fir) || (pTxFlow->hasFin && frame.fin) || pTxFlow->hasSeq.contains(frame.seq))
				{
					spdlog::get("ProtoConv")->warn("DNP3FragHandler::HandleFrame(): Flow 0x{:08x}, Duplicate FIR/FIN/Seq (FIR: {}, FIN: {}, SEQ: {}). Q: {}",pTxFlow->id,frame.fir,frame.fin,frame.seq,ToString(pTxFlow->frag_q));
					Flush(pTxFlow);
				}

				spdlog::get("ProtoConv")->trace("DNP3FragHandler::HandleFrame(): Queuing fragment (FIR: {}, FIN: {}, SEQ: {}) on flow 0x{:08x}.",frame.fir,frame.fin,frame.seq,frame.flow);
				if(frame.fin)
				{
					pTxFlow->hasFin = true;
					pTxFlow->finSeq = frame.seq;
				}
				if(frame.fir)
				{
					pTxFlow->hasFir = true;
					pTxFlow->firSeq = frame.seq;
				}
				pTxFlow->hasSeq.insert(frame.seq);
				pTxFlow->frag_q.emplace(std::move(frame));
				if(spdlog::get("ProtoConv")->should_log(spdlog::level::trace)) //don't eval if wouldn't log - too expensive
					spdlog::get("ProtoConv")->trace("DNP3FragHandler::HandleFrame(): Flow 0x{:08x} Q: {}.",pTxFlow->id,ToString(pTxFlow->frag_q));
				if((pTxFlow->hasFir && pTxFlow->hasFin)
				&& (pTxFlow->firSeq + pTxFlow->frag_q.size()-1)%64 == pTxFlow->finSeq)
				{
					spdlog::get("ProtoConv")->trace("DNP3FragHandler::HandleFrame(): Full set of fragments collected for flow 0x{:08x}.",pTxFlow->id);
					Flush(pTxFlow);
				}
			}
			else //Not a fragment
			{
				if(pTxFlow->pNextFrame)
				{
					spdlog::get("ProtoConv")->warn("DNP3FragHandler::HandleFrame(): Flow 0x{:08x}, Non-fragment frames backing up (FIR: {}, FIN: {}, SEQ: {}) with unfinished fragment Q: {}",pTxFlow->id,frame.fir,frame.fin,frame.seq,ToString(pTxFlow->frag_q));
					Flush(pTxFlow);
				}
				else if(!pTxFlow->frag_q.empty())
				{
					spdlog::get("ProtoConv")->debug("DNP3FragHandler::HandleFrame(): Flow 0x{:08x}, Non-fragment frame saved (FIR: {}, FIN: {}, SEQ: {}).",pTxFlow->id,frame.fir,frame.fin,frame.seq);
					pTxFlow->pNextFrame = std::make_shared<Frame>(std::move(frame));
				}
				else
					WriteHandler(frame.pBuf,frame.len);
			}
		});
	});
}

void DNP3FragHandler::Flush(const std::shared_ptr<TransportFlow>& pTxFlow)
{
	spdlog::get("ProtoConv")->trace("DNP3FragHandler::Flush(): Flow 0x{:08x}.",pTxFlow->id);
	while(!pTxFlow->frag_q.empty())
	{
		WriteHandler(pTxFlow->frag_q.top().pBuf,pTxFlow->frag_q.top().len);
		pTxFlow->frag_q.pop();
	}
	pTxFlow->hasFin = false;
	pTxFlow->hasFir = false;
	pTxFlow->hasSeq.clear();
	if(pTxFlow->pNextFrame)
	{
		WriteHandler(pTxFlow->pNextFrame->pBuf,pTxFlow->pNextFrame->len);
		pTxFlow->pNextFrame.reset();
	}
}
