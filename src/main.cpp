#include "CmdArgs.h"
#include <asio.hpp>

std::array<uint8_t,65*1024> buf;
asio::ip::udp::endpoint sender;
void rcv(asio::ip::udp::socket& socket, asio::error_code err, size_t n)
{
	if(err)
		return;

	std::cout<<"rcv:"<<sender.address().to_string()<<std::endl;

	//TODO: do something instead of just print
	std::string_view str((char*)&buf,n);
	std::cout<<str<<std::flush;

	socket.async_receive_from(asio::buffer(buf),sender,[&](asio::error_code err, size_t n)
	{
		rcv(socket,err,n);
	});
}

int main(int argc, char* argv[])
{
	CmdArgs Args(argc,argv);

	asio::io_context ios(Args.ConcurrencyHint.getValue());
	auto work = asio::make_work_guard(ios);

	asio::ip::udp::socket socket(ios);
	socket.open(asio::ip::udp::v6());
	socket.bind(asio::ip::udp::endpoint(asio::ip::address::from_string(Args.LocalAddr.getValue()),Args.LocalPort.getValue()));

	std::vector<std::thread> threads;
	for(int i = 0; i < Args.ConcurrencyHint.getValue(); i++)
		threads.emplace_back([&](){ ios.run(); });

	socket.async_receive_from(asio::buffer(buf),sender,[&](asio::error_code err, size_t n)
	{
		rcv(socket,err,n);
	});

	asio::signal_set signals(ios);
	signals.add(SIGINT);
	signals.add(SIGTERM);
	signals.add(SIGABRT);
	signals.add(SIGQUIT);
	signals.async_wait([&](asio::error_code err, int)
	{
		if(!err)
		{
			work.reset();
			socket.cancel();
			socket.close();
		}
	});

	ios.run();

	for(auto& t : threads)
		t.join();

	return 0;
}


