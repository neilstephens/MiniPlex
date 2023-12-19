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
		cmd("Protocol adapter to convert between a stream and datagrams",' ',"0.0.1"),
		LocalAddr("a", "localaddr", "Local ip address for datagrams. Defaults to 0.0.0.0 for all ipv4 interfaces.", false, "0.0.0.0", "local addr"),
		LocalPort("l", "localport", "Local port to listen/receive datagrams on.", true, 0, "local port"),
		RemoteAddr("A", "remoteaddr", "Remote ip address for datagrams.", true, "", "remote addr"),
		RemotePort("r", "remoteport", "Remote port for datagrams.", true, 0, "remote port"),
		TCPAddr("T", "tcphost", "If converting TCP, this is the remote IP address for the connection.", false, "", "remote tcp host"),
		LocalTCPAddr("L", "localtcp", "If converting TCP, local ip address to use. Defaults to 0.0.0.0 for all ipv4 interfaces.", false, "0.0.0.0", "local tcp addr"),
		TCPPort("t", "tcpport", "TCP port if converting TCP.", false, 0, "remote tcp port"),
		SerialDevice("s", "serialdevice", "Device if converting serial.", false, "", "serial device"),
		FrameProtocol("p", "frameprotocol", "Parse stream frames based on this protocol", false, "DNP3", "frame protocol"),
		ConsoleLevel("c", "console_logging", "Console log level: off, critical, error, warn, info, debug, or trace. Default off.", false, "off", "console log level"),
		FileLevel("f", "file_logging", "File log level: off, critical, error, warn, info, debug, or trace. Default error.", false, "error", "file log level"),
		LogFile("F", "log_file", "Log filename. Defaults to ./ProtoConv.log", false, "ProtoConv.log", "log filename"),
		LogSize("S", "log_size", "Roll the log file at this many kB. Defaults to 5000", false, 5000, "size in kB"),
		LogNum("N", "log_num", "Keep this many log files when rolling the log. Defaults to 3", false, 3, "number of files"),
		ConcurrencyHint("x", "concurrency", "A hint for the number of threads in thread pool. Defaults to detected hardware concurrency.",false,std::thread::hardware_concurrency(),"numthreads")
	{
		cmd.add(ConcurrencyHint);
		cmd.add(LogNum);
		cmd.add(LogSize);
		cmd.add(LogFile);
		cmd.add(FileLevel);
		cmd.add(ConsoleLevel);
		cmd.add(FrameProtocol);
		cmd.add(SerialDevice);
		cmd.add(TCPPort);
		cmd.add(TCPAddr);
		cmd.add(RemotePort);
		cmd.add(RemoteAddr);
		cmd.add(LocalAddr);
		cmd.add(LocalPort);
		cmd.parse(argc, argv);

		if(TCPAddr.getValue() != "" && SerialDevice.getValue() != "")
			throw std::invalid_argument("Choose TCP Address or Serial Device, not both");

		if(TCPAddr.getValue() == "" && SerialDevice.getValue() == "")
			throw std::invalid_argument("Provide TCP Address or Serial Device");

		asio::error_code err;
		asio::ip::address::from_string(LocalAddr.getValue(),err);
		if(err)
			throw std::invalid_argument("Invalid Local IP address: "+LocalAddr.getValue());

		asio::ip::address::from_string(LocalTCPAddr.getValue(),err);
		if(err)
			throw std::invalid_argument("Invalid Local IP address: "+LocalAddr.getValue());

		asio::ip::address::from_string(RemoteAddr.getValue(),err);
		if(err)
			throw std::invalid_argument("Invalid Remote IP address: "+RemoteAddr.getValue());

		if(TCPAddr.getValue() != "")
		{
			asio::ip::address::from_string(TCPAddr.getValue(),err);
			if(err)
				throw std::invalid_argument("Invalid TCP IP address: "+TCPAddr.getValue());
		}
	}
	TCLAP::CmdLine cmd;
	TCLAP::ValueArg<std::string> LocalAddr;
	TCLAP::ValueArg<uint16_t> LocalPort;
	TCLAP::ValueArg<std::string> RemoteAddr;
	TCLAP::ValueArg<uint16_t> RemotePort;
	TCLAP::ValueArg<std::string> TCPAddr;
	TCLAP::ValueArg<std::string> LocalTCPAddr;
	TCLAP::ValueArg<uint16_t> TCPPort;
	TCLAP::ValueArg<std::string> SerialDevice;
	TCLAP::ValueArg<std::string> FrameProtocol;
	TCLAP::ValueArg<std::string> ConsoleLevel;
	TCLAP::ValueArg<std::string> FileLevel;
	TCLAP::ValueArg<std::string> LogFile;
	TCLAP::ValueArg<size_t> LogSize;
	TCLAP::ValueArg<size_t> LogNum;
	TCLAP::ValueArg<int> ConcurrencyHint;
};

#endif // CMDARGS_H