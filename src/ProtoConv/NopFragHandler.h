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
#ifndef NOPFRAGHANDLER_H
#define NOPFRAGHANDLER_H

#include "FragHandler.h"

class NopFragHandler : public FragHandler
{
public:
	NopFragHandler(const std::function<void(std::shared_ptr<uint8_t> pBuf, const size_t sz)>& write_handler):
		FragHandler(write_handler)
	{}
	void HandleFrame(const Frame& frame) override
	{
		WriteHandler(frame.pBuf,frame.len);
	}
};

#endif // NOPFRAGHANDLER_H
