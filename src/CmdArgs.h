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

#ifndef CMDARGS_H
#define CMDARGS_H

#include <tclap/CmdLine.h>
#include <thread>
#include <exception>
#include <asio.hpp>

struct CmdArgs
{
	CmdArgs(int argc, char* argv[]):
		cmd("A minimal UDP multiplexer/hub/broker. A simple way to bolt-on rudimentary multi-cast/multi-path or combine connections.",' ',MP_VERSION),
		Hub("H", "hub", "Hub/Star mode: Forward datagrams from/to all endpoints."),
		Trunk("T", "trunk", "Trunk mode: Forward frames from a 'trunk' to other endpoints. Forward datagrams from other endpoints to the trunk."),
		Prune("P", "prune", "Like Trunk mode, but limits flow to one (first in best dressed) branch"),
		LocalAddr("l", "local", "Local ip address. Defaults to 0.0.0.0 for all ipv4 interfaces.", false, "0.0.0.0", "localaddr"),
		LocalPort("p", "port", "Local port to listen/receive on.", true, 0, "port"),
		SoRcvBuf("Z", "so_rcvbuf", "Datagram socket receive buffer size.", false, 512L*1024, "rcv buf size"),
		CacheTimeout("o", "timeout", "Milliseconds to keep an idle endpoint cached",false,10000,"timeout"),
		TrunkAddr("r", "trunk_ip", "Remote trunk ip address.", false, "", "trunkhost"),
		TrunkPort("t", "trunk_port", "Remote trunk port.", false, 0, "trunkport"),
		BranchAddrs("B", "branch_ip", "Remote endpoint addresses to permanently cache. Use -b to provide respective ports in the same order.", false, "branchhost"),
		BranchPorts("b", "branch_port", "Remote endpoint port to permanently cache. Use -B to provide respective addresses in the same order.", false, "branchport"),
		ConsoleLevel("c", "console_logging", "Console log level: off, critical, error, warn, info, debug, or trace. Default critical.", false, "critical", "console log level"),
		FileLevel("f", "file_logging", "File log level: off, critical, error, warn, info, debug, or trace. Default error.", false, "error", "file log level"),
		LogFile("F", "log_file", "Log filename. Defaults to ./MiniPlex.log", false, "MiniPlex.log", "log filename"),
		LogSize("S", "log_size", "Roll the log file at this many kB. Defaults to 5000", false, 5000, "size in kB"),
		LogNum("N", "log_num", "Keep this many log files when rolling the log. Defaults to 3", false, 3, "number of files"),
		ConcurrencyHint("x", "concurrency", "A hint for the number of threads in thread pool. Defaults to detected hardware concurrency.",false,std::thread::hardware_concurrency(),"numthreads"),
		Benchmark("M", "benchmark", "Run a loopback test for fixed duration (see -m) and exit."),
		BenchDuration("m", "benchmark_duration", "Number of milliseconds to run the loopback benchmark test. Defaults to 10000.",false,10000,"milliseconds")
	{
		cmd.add(BenchDuration);
		cmd.add(Benchmark);
		cmd.add(ConcurrencyHint);
		cmd.add(LogNum);
		cmd.add(LogSize);
		cmd.add(LogFile);
		cmd.add(FileLevel);
		cmd.add(ConsoleLevel);
		cmd.add(BranchPorts);
		cmd.add(BranchAddrs);
		cmd.add(TrunkPort);
		cmd.add(TrunkAddr);
		cmd.add(CacheTimeout);
		cmd.add(SoRcvBuf);
		cmd.add(LocalAddr);
		cmd.add(LocalPort);
		cmd.xorAdd({&Hub,&Trunk,&Prune});
		cmd.parse(argc, argv);

		if(!Hub && (TrunkAddr.getValue() == "" || TrunkPort.getValue() == 0))
			throw std::invalid_argument("Trunk ip and port required when not in Hub mode.");

		asio::error_code err;
		asio::ip::address::from_string(LocalAddr.getValue(),err);
		if(err)
			throw std::invalid_argument("Invalid Local IP address: "+LocalAddr.getValue());
		if(!Hub)
		{
			asio::ip::address::from_string(TrunkAddr.getValue(),err);
			if(err)
				throw std::invalid_argument("Invalid Trunk IP address: "+TrunkAddr.getValue());
		}
		for(const auto& B : BranchAddrs.getValue())
		{
			asio::ip::address::from_string(B,err);
			if(err)
				throw std::invalid_argument("Invalid Branch IP address: "+B);
		}
		if(BranchAddrs.getValue().size() != BranchPorts.getValue().size())
			throw std::invalid_argument("Please provide the same number of branch IPs and branch ports. They will be paired.");
		if(Prune && BranchAddrs.getValue().size() > 0)
			throw std::invalid_argument("Permanent cache entries (-B/b) invalidates Prune mode.");
	}
	TCLAP::CmdLine cmd;
	TCLAP::SwitchArg Hub;
	TCLAP::SwitchArg Trunk;
	TCLAP::SwitchArg Prune;
	TCLAP::ValueArg<std::string> LocalAddr;
	TCLAP::ValueArg<uint16_t> LocalPort;
	TCLAP::ValueArg<size_t> SoRcvBuf;
	TCLAP::ValueArg<size_t> CacheTimeout;
	TCLAP::ValueArg<std::string> TrunkAddr;
	TCLAP::ValueArg<uint16_t> TrunkPort;
	TCLAP::MultiArg<std::string> BranchAddrs;
	TCLAP::MultiArg<uint16_t> BranchPorts;
	TCLAP::ValueArg<std::string> ConsoleLevel;
	TCLAP::ValueArg<std::string> FileLevel;
	TCLAP::ValueArg<std::string> LogFile;
	TCLAP::ValueArg<size_t> LogSize;
	TCLAP::ValueArg<size_t> LogNum;
	TCLAP::ValueArg<int> ConcurrencyHint;
	TCLAP::SwitchArg Benchmark;
	TCLAP::ValueArg<size_t> BenchDuration;
};

#endif // CMDARGS_H
