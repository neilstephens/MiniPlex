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

#include "MiniPlex.h"
#include "CmdArgs.h"
#include <asio.hpp>

MiniPlex::MiniPlex(const CmdArgs& Args, asio::io_context& IOC):
	Args(Args),
	IOC(IOC),
	socket(IOC),
	EndPointCache(IOC,Args.CacheTimeout.getValue())
{
	socket.open(asio::ip::udp::v6());
	socket.bind(asio::ip::udp::endpoint(asio::ip::address::from_string(Args.LocalAddr.getValue()),Args.LocalPort.getValue()));
	socket.async_receive_from(asio::buffer(rcv_buf),rcv_sender,[&](asio::error_code err, size_t n)
	{
		RcvHandler(err,n);
	});
}

void MiniPlex::Stop()
{
	socket.cancel();
	socket.close();
}

void MiniPlex::RcvHandler(const asio::error_code err, const size_t n)
{
	if(err)
		return;

	auto sender_string = rcv_sender.address().to_string()+":"+std::to_string(rcv_sender.port());
	EndPointCache.Add(sender_string);

	std::cout<<"RcvHandler():"<<sender_string<<std::endl;

	//TODO: do something instead of just print
	std::string_view str((char*)&rcv_buf,n);
	std::cout<<str<<std::flush;

	socket.async_receive_from(asio::buffer(rcv_buf),rcv_sender,[&](asio::error_code err, size_t n)
	{
		RcvHandler(err,n);
	});
}
