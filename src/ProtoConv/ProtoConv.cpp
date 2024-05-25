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

#include "ProtoConv.h"
#include "CmdArgs.h"
#include "SerialStreamHandler.h"
#include "SerialPortsManager.h"
#include "TCPStreamHandler.h"
#include "TCPSocketManager.h"
#include "NullFrameChecker.h"
#include "DNP3FrameChecker.h"
#include "DelimFrameChecker.h"
#include "NopFragHandler.h"
#include "DNP3FragHandler.h"
#include "DelimFragHandler.h"
#include <asio.hpp>
#include <spdlog/spdlog.h>
#include <memory>
#include <csignal>

ProtoConv::ProtoConv(const CmdArgs& Args, asio::io_context& IOC):
	Args(Args),
	IOC(IOC),
	local_ep(asio::ip::address::from_string(Args.LocalAddr.getValue()),Args.LocalPort.getValue()),
	remote_ep(asio::ip::address::from_string(Args.RemoteAddr.getValue()),Args.RemotePort.getValue()),
	socket(IOC,local_ep),
	socket_strand(IOC),
	process_strand(IOC),
	MaxWriteQSz(Args.MaxWriteQSz.getValue()),
	Delim(Args.Delim.getValue())
{
	auto writer = [this](std::shared_ptr<uint8_t> pBuf, const size_t sz){WriteUDPHandler(pBuf,sz);};
	if(Delim != 0)
	{
		pFramer = std::make_shared<DelimFrameChecker>(Delim);
		pFragHandler = std::make_shared<DelimFragHandler>(writer,IOC);
	}
	else if(Args.FrameProtocol.getValue() == "DNP3")
	{
		pFramer = std::make_shared<DNP3FrameChecker>();
		pFragHandler = std::make_shared<DNP3FragHandler>(writer,IOC);
	}
	else if(Args.FrameProtocol.getValue() == "Null")
	{
		pFramer = std::make_shared<NullFrameChecker>();
		pFragHandler = std::make_shared<NopFragHandler>(writer);
	}
	else
		throw std::invalid_argument("Unknown frame protocol: "+Args.FrameProtocol.getValue());

	asio::socket_base::receive_buffer_size option(Args.SoRcvBuf.getValue());
	socket.set_option(option);

	socket.connect(remote_ep);
	spdlog::get("ProtoConv")->info("Sending UDP to {}:{}",Args.RemoteAddr.getValue(),Args.RemotePort.getValue());

	if(Args.TCPAddr.getValue() != "")
	{
		auto ip = Args.TCPAddr.getValue();
		auto prt = std::to_string(Args.TCPPort.getValue());
		auto srv = !Args.TCPisClient.getValue();

		std::stringstream ss(Args.TCPConnectBackoffTimes.getValue());
		AutoOpenOpts autoOpen(true);
		uint32_t backoff_min, backoff_max, estab_reset_min;
		if(ss>>backoff_min>>backoff_max>>estab_reset_min)
		{
			autoOpen.min_retry_time_ms = backoff_min;
			autoOpen.max_retry_time_ms = backoff_max;
			autoOpen.min_established_time_ms = estab_reset_min;
		}
		else
			throw std::invalid_argument("Invalid TCP backoff times: '"+Args.TCPConnectBackoffTimes.getValue()+"'. Should take form '<MinRetryTime> <MaxRetryTime> <EstablishedResetTime>' in milliseconds.");

		spdlog::get("ProtoConv")->info("Operating in TCP {} mode {}:{}",srv?"Server":"Client",ip,prt);
		auto pSockMan = std::make_shared<TCPSocketManager>(IOC,srv,ip,prt,[this](buf_t& buf){RcvStreamHandler(buf);},[](bool){},MaxWriteQSz,autoOpen,ThrottleOpts(),
		[log{spdlog::get("ProtoConv")}](const std::string& lvl, const std::string& msg)
		{
			log->log(spdlog::level::from_str(lvl),msg);
		});
		pStream = std::make_shared<TCPStreamHandler>(pSockMan);
	}
	else if(!Args.SerialDevices.getValue().empty())
	{
		spdlog::get("ProtoConv")->info("Operating in Serial mode on {} devices",Args.SerialDevices.getValue().size());
		auto pSerialMan =  std::make_shared<SerialPortsManager>(IOC,Args.SerialDevices.getValue(),[this](buf_t& buf){RcvStreamHandler(buf);},MaxWriteQSz);

		if(!Args.SerialBaudRates.getValue().empty())
			pSerialMan->SetBaudRate(Args.SerialBaudRates.getValue());
		if(!Args.SerialCharSizes.getValue().empty())
			pSerialMan->SetCharSize(Args.SerialCharSizes.getValue());
		if(!Args.SerialFlowControls.getValue().empty())
			pSerialMan->SetFlowControl(Args.SerialFlowControls.getValue());
		if(!Args.SerialStopBits.getValue().empty())
			pSerialMan->SetStopBits(Args.SerialStopBits.getValue());

		pStream = std::make_shared<SerialStreamHandler>(pSerialMan);
	}

	socket_strand.post([this](){RcvUDP();});
	spdlog::get("ProtoConv")->info("Listening for UDP on {}:{}",Args.LocalAddr.getValue(),Args.LocalPort.getValue());
}

void ProtoConv::RcvUDP()
{
	spdlog::get("ProtoConv")->trace("RcvUDP(): {} rcv buffers available.",rcv_buf_q.size());
	if(rcv_buf_q.empty())
	{
		spdlog::get("ProtoConv")->debug("RcvUDP(): Allocating another rcv buffer.");
		rcv_buf_q.push_back(std::make_shared<rbuf_t>());
	}

	auto pBuf = rcv_buf_q.front();
	rcv_buf_q.pop_front();

	socket.async_receive(asio::buffer(pBuf->data(),pBuf->size()),socket_strand.wrap([this,pBuf](asio::error_code err, size_t n)
	{
		process_strand.post([this,err,pBuf,n]()
		{
			RcvUDPHandler(err,pBuf->data(),n);
			socket_strand.post([this,pBuf]()
			{
				rcv_buf_q.push_back(pBuf);
			});
		});
		RcvUDP();
	}));
}

void ProtoConv::RcvUDPHandler(const asio::error_code err, const uint8_t* const buf, const size_t n)
{
	if(err)
		spdlog::get("ProtoConv")->error("RcvUDPHandler(): error code {}: '{}'",err.value(),err.message());
	else
		spdlog::get("ProtoConv")->trace("RcvUDPHandler(): {} bytes.",n);

	std::vector<uint8_t> data(n);
	memcpy(data.data(),buf,n);
	AddDelim(data);
	pStream->Write(std::move(data));
}

void ProtoConv::RcvStreamHandler(buf_t& buf)
{
	spdlog::get("ProtoConv")->trace("RcvStreamHandler(): {} bytes in buffer.",buf.size());
	while(auto frame = pFramer->CheckFrame(buf))
	{
		if(frame.len > buf.size())
		{
			spdlog::get("ProtoConv")->error("RcvStreamHandler(): Frame checker claims frame size (), which is > size of buffer ().",frame.len,buf.size());
			frame.len = buf.size();
		}
		// The C++20 way causes a malloc error when asio tries to copy a handler with this style shared_ptr
		//auto pForwardBuf = std::make_shared<uint8_t[]>(n);
		// Use the old way instead - only difference should be the control block is allocated separately
		auto pForwardBuf = std::shared_ptr<uint8_t>(new uint8_t[frame.len],[](uint8_t* p){delete[] p;});
		const size_t ncopied = buf.sgetn(reinterpret_cast<char*>(pForwardBuf.get()),frame.len);
		if(ncopied != frame.len)
		{
			spdlog::get("ProtoConv")->warn("RcvStreamHandler(): Failed to copy whole frame to datagram buffer. Frame size {}, copied {}.",frame.len,ncopied);
			frame.len = ncopied;
		}

		frame.pBuf = pForwardBuf;
		pFragHandler->HandleFrame(frame);
	}
}

void ProtoConv::WriteUDPHandler(std::shared_ptr<uint8_t> pBuf, const size_t sz)
{
	socket_strand.post([this,pBuf,sz]()
	{
		spdlog::get("ProtoConv")->trace("WriteUDPHandler(): Forwarding frame length {} to UDP",sz);
		socket.async_send(asio::buffer(pBuf.get(),sz),[pBuf](asio::error_code,size_t){});
		//this is safe to call again before the handler is called - because it's non-composed
		//	according to stackoverflow there is a queue for the file descriptor under-the-hood
	});
}

void ProtoConv::AddDelim(std::vector<uint8_t>& data)
{
	if(Delim == 0)
		return;

	//reserve space for 32b delim + 32b sequence + 16b CRC
	data.reserve(data.size()+10);

	using ByteView = std::basic_string_view<const uint8_t>;

	//insert the delimiter
	ByteView DelimBytes(reinterpret_cast<const uint8_t*>(&Delim),4);
	auto delim_it = data.insert(data.end(),DelimBytes.begin(),DelimBytes.end());

	//insert the sequence number
	const uint32_t seq = DelimSeq++;
	ByteView SeqBytes(reinterpret_cast<const uint8_t*>(&seq),4);
	data.insert(data.end(),SeqBytes.begin(),SeqBytes.end());

	//calculate and insert the CRC
	const uint8_t* const delim_addr = &(*(delim_it));
	const uint16_t crc = crc_ccitt(delim_addr,8);
	ByteView CRCBytes(reinterpret_cast<const uint8_t*>(&crc),2);
	data.insert(data.end(),CRCBytes.begin(),CRCBytes.end());

	spdlog::get("ProtoConv")->trace("AddDelim(): Delim {}, Seq {}, CRC 0x{:04x}",Delim,seq,crc);
}

