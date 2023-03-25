#include "MiniPlex.h"
#include "CmdArgs.h"
#include <asio.hpp>

MiniPlex::MiniPlex(const CmdArgs& Args, asio::io_context& IOC):
	Args(Args),
	IOC(IOC),
	socket(IOC)
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

	std::cout<<"RcvHandler():"<<rcv_sender.address().to_string()<<std::endl;

	//TODO: do something instead of just print
	std::string_view str((char*)&rcv_buf,n);
	std::cout<<str<<std::flush;

	socket.async_receive_from(asio::buffer(rcv_buf),rcv_sender,[&](asio::error_code err, size_t n)
	{
		RcvHandler(err,n);
	});
}
