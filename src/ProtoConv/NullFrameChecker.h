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

#ifndef NULLFRAMECHECKER_H
#define NULLFRAMECHECKER_H

#include "FrameChecker.h"

class NullFrameChecker : public FrameChecker
{
public:
	inline Frame CheckFrame(const buf_t& readbuf) override
	{
		//Don't check anything. Everything is a valid 'frame'.
		return Frame(readbuf.size());
	}
};

#endif // NULLFRAMECHECKER_H
