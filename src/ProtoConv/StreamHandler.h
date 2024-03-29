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
#ifndef STREAMHANDLER_H
#define STREAMHANDLER_H

#include <vector>
#include <cstdint>

class StreamHandler
{
public:
	StreamHandler();
	virtual ~StreamHandler() = default;

	virtual void Write(std::vector<uint8_t>&& data) = 0;
};

#endif // STREAMHANDLER_H
