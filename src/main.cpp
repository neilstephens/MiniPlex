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

#include "CmdArgs.h"
#include "LogSetup.h"
#include "MiniPlex.h"
#include <asio.hpp>
#include <spdlog/spdlog.h>
#include <vector>
#include <thread>
#include <exception>
#include <memory>
#include <unistd.h>

int main(int argc, char* argv[])
{
	#ifdef HAVE_CLOSEFROM
	//close any inherrited file descriptors
	closefrom(3);
	#endif
try
{
	CmdArgs Args(argc,argv);
	setup_logs(Args);

	asio::io_context IOC(Args.ConcurrencyHint.getValue());
	auto work = asio::make_work_guard(IOC);

	MiniPlex MP(Args,IOC);

	std::vector<std::thread> threads;
	for(int i = 0; i < Args.ConcurrencyHint.getValue(); i++)
		threads.emplace_back([&](){ IOC.run(); });

	spdlog::get("MiniPlex")->info("Thread pool started {} threads.",threads.size());

	asio::signal_set signals(IOC,SIGINT,SIGTERM,SIGABRT);
	signals.async_wait([&](asio::error_code err, int sig)
	{
		if(!err)
		{
			spdlog::get("MiniPlex")->critical("Signal {}: shutting down.",sig);
			work.reset();
			IOC.stop();
		}
	});

	if(Args.Benchmark)
		MP.Benchmark();
	IOC.run();

	spdlog::get("MiniPlex")->info("Joining threads.");
	for(auto& t : threads)
		t.join();

	spdlog::get("MiniPlex")->info("Shutdown cleanly: return 0.");
	spdlog::apply_all([](const std::shared_ptr<spdlog::logger>& l) {l->flush(); });
	spdlog::drop_all();
	spdlog::shutdown();
	return 0;
}
catch(const std::exception& e)
{
	if(auto log = spdlog::get("MiniPlex"))
		log->critical("Exception: '{}'",e.what());
	else
		std::cerr<<"Exception: '"<<e.what()<<"'"<<std::endl;

	spdlog::shutdown();
	return 1;
}
}
