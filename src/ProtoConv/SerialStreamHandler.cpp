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
#include "SerialStreamHandler.h"
#include <spdlog/spdlog.h>
#include <asio.hpp>
#include <vector>
#include <string>

SerialStreamHandler::SerialStreamHandler(std::shared_ptr<SerialPortsManager> pSerialMan):
	pSerialMan(pSerialMan)
{
	this->pSerialMan->Start();
}

SerialStreamHandler::~SerialStreamHandler()
{
	pSerialMan->Stop();
}

void SerialStreamHandler::Write(std::vector<uint8_t>&& data)
{
	pSerialMan->Write(std::move(data));
}
