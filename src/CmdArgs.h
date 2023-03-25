#ifndef CMDARGS_H
#define CMDARGS_H

#include <tclap/CmdLine.h>
#include <thread>

struct CmdArgs
{
	CmdArgs(int argc, char* argv[]):
		cmd("A minimal UDP/TCP multiplexer/hub/broker. A simple way to bolt-on rudimentary multi-cast/multi-path or combine connections.",' ',"0.1.0"),
		Hub("H", "hub", "Hub/Star mode: Forward datagrams from/to all endpoints."),
		Trunk("T", "trunk", "Trunk mode: Forward frames from a 'trunk' to other endpoints. Forward datagrams from other endpoints to the trunk."),
		Prune("P", "prune", "Like Trunk mode, but limits flow to one (first in best dressed) branch"),
		LocalAddr("l", "local", "Local ip address. Defaults to 0.0.0.0 for all interfaces.", false, "::", "localaddr"),
		LocalPort("p", "port", "Local port to listen/receive on.", true, 0, "port"),
		TrunkAddr("r", "trunkip", "Remote trunk ip address.", false, "", "trunkhost"),
		TrunkPort("t", "trunkport", "Remote trunk port.", false, 0, "trunkport"),
		ConcurrencyHint("x", "concurrency", "A hint for the number of threads in thread pool. Defaults to detected hardware concurrency.",false,std::thread::hardware_concurrency(),"numthreads")
	{
		cmd.add(ConcurrencyHint);
		cmd.add(TrunkPort);
		cmd.add(TrunkAddr);
		cmd.add(LocalAddr);
		cmd.add(LocalPort);
		cmd.xorAdd({&Hub,&Trunk,&Prune});
		cmd.parse(argc, argv);
	}
	TCLAP::CmdLine cmd;
	TCLAP::SwitchArg Hub;
	TCLAP::SwitchArg Trunk;
	TCLAP::SwitchArg Prune;
	TCLAP::ValueArg<std::string> LocalAddr;
	TCLAP::ValueArg<uint16_t> LocalPort;
	TCLAP::ValueArg<std::string> TrunkAddr;
	TCLAP::ValueArg<uint16_t> TrunkPort;
	TCLAP::ValueArg<int> ConcurrencyHint;
};

#endif // CMDARGS_H
