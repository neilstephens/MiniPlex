#include "CmdArgs.h"
#include "MiniPlex.h"
#include <asio.hpp>


int main(int argc, char* argv[])
{
	CmdArgs Args(argc,argv);

	asio::io_context IOC(Args.ConcurrencyHint.getValue());
	auto work = asio::make_work_guard(IOC);

	MiniPlex MP(Args,IOC);

	std::vector<std::thread> threads;
	for(int i = 0; i < Args.ConcurrencyHint.getValue(); i++)
		threads.emplace_back([&](){ IOC.run(); });

	asio::signal_set signals(IOC);
	signals.add(SIGINT);
	signals.add(SIGTERM);
	signals.add(SIGABRT);
	signals.add(SIGQUIT);
	signals.async_wait([&](asio::error_code err, int)
	{
		if(!err)
		{
			work.reset();
			MP.Stop();
		}
	});

	IOC.run();

	for(auto& t : threads)
		t.join();

	return 0;
}


