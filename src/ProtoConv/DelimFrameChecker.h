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

#ifndef DELIMFRAMECHECKER_H
#define DELIMFRAMECHECKER_H

#include "FrameChecker.h"
#include <algorithm>
#include <spdlog/spdlog.h>
#include <span>

class DelimFrameChecker : public FrameChecker
{
public:
	DelimFrameChecker(const uint32_t Delim):
		Delim(Delim)
	{}
	inline Frame CheckFrame(const buf_t& readbuf) override
	{
		if(readbuf.size() < 10) //size of delim+seq+crc
			return Frame(0);

		const auto buf_begin = asio::buffers_begin(readbuf.data());
		const auto buf_end = asio::buffers_end(readbuf.data());

		size_t frame_len = 10;
		for(auto byte_it = buf_begin+3; byte_it+6 != buf_end; byte_it++)
		{
			std::span<const char> DelimBytes(reinterpret_cast<const char*>(&Delim),4);
			if(std::equal(DelimBytes.begin(),DelimBytes.end(),byte_it-3))
			{
				spdlog::get("ProtoConv")->trace("DelimFrameChecker::CheckFrame(): Found matching delimiter({}), frame length {}.",Delim,frame_len);
				//byte_it is on the last byte of the delimiter
				//next four are the (possible) sequence number
				//and last two the crc if it passes
				std::array<char,10> delim_seq_crc_bytes =
				{
					*(byte_it-3),*(byte_it-2),*(byte_it-1),*byte_it,       //delim
					*(byte_it+1),*(byte_it+2),*(byte_it+3),*(byte_it+4),   //seq
					*(byte_it+5),*(byte_it+6)                              //crc
				};
				uint32_t possible_seq; uint16_t possible_crc;
				memcpy(&possible_seq, delim_seq_crc_bytes.data() + 4, sizeof(possible_seq));
				memcpy(&possible_crc, delim_seq_crc_bytes.data() + 8, sizeof(possible_crc));
				auto crc_should_be = crc_ccitt((uint8_t*)delim_seq_crc_bytes.data(),8);
				if(possible_crc == crc_should_be)
				{
					spdlog::get("ProtoConv")->trace("DelimFrameChecker::CheckFrame(): CRC match. Seq {}, CRC 0x{:04x}.",possible_seq,possible_crc);
					return Frame(frame_len,true,true,1,possible_seq);
				}
				else
					spdlog::get("ProtoConv")->trace("DelimFrameChecker::CheckFrame(): CRC fail. Seq 0x{:08x}, CRC 0x{:04x} != 0x{:04x}.",possible_seq,possible_crc,crc_should_be);
			}

			//Hard upper limit in case we never find a delimiter (if someone forgots to turn them on on the sending side...)
			if(frame_len >= 64L*1024) //max UDP
			{
				spdlog::get("ProtoConv")->error("DelimFrameChecker::CheckFrame(): Hit upper limit for frame length ({}) with no valid delimiter.",frame_len);
				return Frame(frame_len,true,true,0);//flow zero means 'unframed'
			}

			frame_len++;
		}
		return Frame(0);
	}
private:
	uint32_t Delim;
};

#endif // DELIMFRAMECHECKER_H
