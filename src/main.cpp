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


int main(int argc, char* argv[])
{
	CmdArgs Args(argc,argv);

	asio::io_context IOC(Args.ConcurrencyHint.getValue());
	auto work = asio::make_work_guard(IOC);

	MiniPlex MP(Args,IOC);

	std::vector<std::thread> threads;
	for(int i = 0; i < Args.ConcurrencyHint.getValue(); i++)
		threads.emplace_back([&](){ IOC.run(); });

	asio::signal_set signals(IOC,SIGINT,SIGTERM,SIGQUIT);
	signals.add(SIGABRT);
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


