#ifndef MINIPLEX_H
#define MINIPLEX_H

#include <asio.hpp>

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
	std::array<uint8_t,65*1024> rcv_buf;
	asio::ip::udp::endpoint rcv_sender;
};

#endif // MINIPLEX_H
