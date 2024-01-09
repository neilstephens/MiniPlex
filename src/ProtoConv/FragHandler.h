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
#ifndef FRAGHANDLER_H
#define FRAGHANDLER_H

#include "FrameChecker.h"
#include <functional>

class FragHandler
{
public:
	FragHandler(const std::function<void(std::shared_ptr<uint8_t> pBuf, const size_t sz)>& write_handler):
		WriteHandler(write_handler)
	{}
	virtual ~FragHandler() = default;
	virtual void HandleFrame(const Frame& frame) = 0;
protected:
	const std::function<void(std::shared_ptr<uint8_t> pBuf, const size_t sz)> WriteHandler;
};

#endif // FRAGHANDLER_H
