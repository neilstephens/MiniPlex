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
#include "MiniPlex.h"
#include <asio.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/async.h>
#include <vector>
#include <exception>
#include <memory>

void setup_logs(const CmdArgs& Args);

int main(int argc, char* argv[])
{
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
			MP.Stop();
		}
	});

	IOC.run();

	spdlog::get("MiniPlex")->info("Joining threads.");
	for(auto& t : threads)
		t.join();

	spdlog::get("MiniPlex")->info("Shutdown cleanly: return 0.");
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

void setup_logs(const CmdArgs& Args)
{
	//these return level::off if no match
	auto file_level = spdlog::level::from_str(Args.FileLevel.getValue());
	auto console_level = spdlog::level::from_str(Args.ConsoleLevel.getValue());

	//check for no match
	if(file_level == spdlog::level::off && Args.FileLevel.getValue() != "off")
		throw std::runtime_error("Unknown file log level: "+Args.FileLevel.getValue());
	if(console_level == spdlog::level::off && Args.ConsoleLevel.getValue() != "off")
		throw std::runtime_error("Unknown console log level: "+Args.ConsoleLevel.getValue());

	//prep the sinks
	auto file = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(Args.LogFile.getValue(), Args.LogSize.getValue()*1024, Args.LogNum.getValue());
	auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	file->set_level(file_level);
	console->set_level(console_level);
	const std::vector<spdlog::sink_ptr> sinks = {file,console};

	//finally make our logger
	spdlog::init_thread_pool(4096, 1);
	auto log = std::make_shared<spdlog::async_logger>("MiniPlex", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::overrun_oldest);
	log->set_level(spdlog::level::trace);
	log->flush_on(spdlog::level::err);
	spdlog::register_logger(log);
	spdlog::flush_every(std::chrono::seconds(3));
}
