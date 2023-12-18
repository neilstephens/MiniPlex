#ifndef PLATFORM_H_
#define PLATFORM_H_

#include <asio.hpp>

/// Platform specific socket options
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#include <Mstcpip.h>
inline void SetTCPKeepalives(asio::ip::tcp::socket& tcpsocket, bool enable=true, unsigned int initial_interval_s=7200, unsigned int subsequent_interval_s=75, unsigned int fail_count=9)
{
	DWORD dwBytesRet;
	tcp_keepalive alive;
	alive.onoff = enable;
	alive.keepalivetime = initial_interval_s * 1000;
	alive.keepaliveinterval = subsequent_interval_s * 1000;

	if(WSAIoctl(tcpsocket.native_handle(), SIO_KEEPALIVE_VALS, &alive, sizeof(alive), nullptr, 0, &dwBytesRet, nullptr, nullptr) == SOCKET_ERROR)
		throw std::runtime_error("Failed to set TCP SIO_KEEPALIVE_VALS");
}
#elif __APPLE__
inline void SetTCPKeepalives(asio::ip::tcp::socket& tcpsocket, bool enable=true, unsigned int initial_interval_s=7200, unsigned int subsequent_interval_s=75, unsigned int fail_count=9)
{
	int set = enable ? 1 : 0;
	if(setsockopt(tcpsocket.native_handle(), SOL_SOCKET,  SO_KEEPALIVE, &set, sizeof(set)) != 0)
		throw std::runtime_error("Failed to set TCP SO_KEEPALIVE");
	if(setsockopt(tcpsocket.native_handle(), IPPROTO_TCP, TCP_KEEPALIVE, &initial_interval_s, sizeof(initial_interval_s)) != 0)
		throw std::runtime_error("Failed to set TCP_KEEPALIVE");
}
#else
inline void SetTCPKeepalives(asio::ip::tcp::socket& tcpsocket, bool enable=true, unsigned int initial_interval_s=7200, unsigned int subsequent_interval_s=75, unsigned int fail_count=9)
{
	int set = enable ? 1 : 0;
	if(setsockopt(tcpsocket.native_handle(), SOL_SOCKET, SO_KEEPALIVE, &set, sizeof(set)) != 0)
		throw std::runtime_error("Failed to set TCP SO_KEEPALIVE");

	if(setsockopt(tcpsocket.native_handle(), SOL_TCP, TCP_KEEPIDLE, &initial_interval_s, sizeof(initial_interval_s)) != 0)
		throw std::runtime_error("Failed to set TCP_KEEPIDLE");

	if(setsockopt(tcpsocket.native_handle(), SOL_TCP, TCP_KEEPINTVL, &subsequent_interval_s, sizeof(subsequent_interval_s)) != 0)
		throw std::runtime_error("Failed to set TCP_KEEPINTVL");

	if(setsockopt(tcpsocket.native_handle(), SOL_TCP, TCP_KEEPCNT, &fail_count, sizeof(fail_count)) != 0)
		throw std::runtime_error("Failed to set TCP_KEEPCNT");
}
#endif

#endif //PLATFORM_H_
