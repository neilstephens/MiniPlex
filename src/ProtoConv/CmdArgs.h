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
		cmd("Protocol adapter to convert between a stream and datagrams.",' ',MP_VERSION),
		LocalAddr("a", "localaddr", "Local ip address for datagrams. Defaults to 0.0.0.0 for all ipv4 interfaces.", false, "0.0.0.0", "local addr"),
		LocalPort("l", "localport", "Local port to listen/receive datagrams on.", true, 0, "local port"),
		RemoteAddr("A", "remoteaddr", "Remote ip address for datagrams.", true, "", "remote addr"),
		RemotePort("r", "remoteport", "Remote port for datagrams.", true, 0, "remote port"),
		SoRcvBuf("B", "so_rcvbuf", "Datagram socket receive buffer size.", false, 512L*1024, "rcv buf size"),
		MaxWriteQSz("Q", "write_queue_size", "Max number of messages to buffer in the stream writer queue before dropping (older) data.", false, 1024, "max write queue size"),
		Delim("D", "packet_delimiter", "Use a packet delimiter (inserted in the stream with sequence and CRC) instead of protocol framing.", false, 0, "packet delimiter"),
		TCPAddr("T", "tcphost", "If converting TCP, this is the remote IP address for the connection.", false, "", "remote tcp host"),
		TCPisClient("C","tcpisclient", "If converting TCP, this is defines if it's a client or server connection.", false, true, "tcp is client"),
		TCPPort("t", "tcpport", "TCP port if converting TCP.", false, 0, "remote tcp port"),
		SerialDevices("s", "serialdevices", "List of serial devices, if converting serial", false, "serial devices"),
		SerialBaudRates("b", "serialbauds", "List of serial board rates, if converting serial", false, "serial bauds rates"),
		SerialFlowControls("L", "serialflowctl", "List of serial flow control settings, if converting serial", false, "serial flow ctl settings"),
		SerialCharSizes("Z", "serialcharsize", "List of serial char sizes, if converting serial", false, "serial char sizes"),
		SerialStopBits("i", "serialstopbits", "List of serial stop bits settings, if converting serial", false, "serial stop bits"),
		FrameProtocol("p", "frameprotocol", "Parse stream frames based on this protocol", false, "DNP3", "frame protocol"),
		ConsoleLevel("c", "console_logging", "Console log level: off, critical, error, warn, info, debug, or trace. Default off.", false, "off", "console log level"),
		FileLevel("f", "file_logging", "File log level: off, critical, error, warn, info, debug, or trace. Default error.", false, "error", "file log level"),
		LogFile("F", "log_file", "Log filename. Defaults to ./ProtoConv.log", false, "ProtoConv.log", "log filename"),
		LogSize("S", "log_size", "Roll the log file at this many kB. Defaults to 5000", false, 5000, "log file size in kB"),
		LogNum("N", "log_num", "Keep this many log files when rolling the log. Defaults to 3", false, 3, "number of log files"),
		ConcurrencyHint("x", "concurrency", "A hint for the number of threads in thread pool. Defaults to detected hardware concurrency.",false,std::thread::hardware_concurrency(),"numthreads")
	{
		cmd.add(ConcurrencyHint);
		cmd.add(LogNum);
		cmd.add(LogSize);
		cmd.add(LogFile);
		cmd.add(FileLevel);
		cmd.add(ConsoleLevel);
		cmd.xorAdd(FrameProtocol,Delim);
		cmd.add(SerialStopBits);
		cmd.add(SerialCharSizes);
		cmd.add(SerialFlowControls);
		cmd.add(SerialBaudRates);
		cmd.add(SerialDevices);
		cmd.add(TCPPort);
		cmd.add(TCPisClient);
		cmd.add(TCPAddr);
		cmd.add(MaxWriteQSz);
		cmd.add(SoRcvBuf);
		cmd.add(RemotePort);
		cmd.add(RemoteAddr);
		cmd.add(LocalAddr);
		cmd.add(LocalPort);
		cmd.parse(argc, argv);

		if(TCPAddr.getValue() != "" && !SerialDevices.getValue().empty())
			throw std::invalid_argument("Choose TCP Address or Serial Devices, not both");

		if(TCPAddr.getValue() == "" && SerialDevices.getValue().empty())
			throw std::invalid_argument("Provide TCP Address or Serial Devices");

		asio::error_code err;
		asio::ip::address::from_string(LocalAddr.getValue(),err);
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
	TCLAP::ValueArg<size_t> SoRcvBuf;
	TCLAP::ValueArg<size_t> MaxWriteQSz;
	TCLAP::ValueArg<uint32_t> Delim;
	TCLAP::ValueArg<std::string> TCPAddr;
	TCLAP::ValueArg<bool> TCPisClient;
	TCLAP::ValueArg<uint16_t> TCPPort;
	TCLAP::MultiArg<std::string> SerialDevices;
	TCLAP::MultiArg<size_t> SerialBaudRates;
	TCLAP::MultiArg<std::string> SerialFlowControls;
	TCLAP::MultiArg<size_t> SerialCharSizes;
	TCLAP::MultiArg<std::string> SerialStopBits;
	TCLAP::ValueArg<std::string> FrameProtocol;
	TCLAP::ValueArg<std::string> ConsoleLevel;
	TCLAP::ValueArg<std::string> FileLevel;
	TCLAP::ValueArg<std::string> LogFile;
	TCLAP::ValueArg<size_t> LogSize;
	TCLAP::ValueArg<size_t> LogNum;
	TCLAP::ValueArg<int> ConcurrencyHint;
};

#endif // CMDARGS_H
