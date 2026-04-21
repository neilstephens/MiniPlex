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

//Some platform-libc combinations don't support setting hardware flow control (looking at you musl)

#if defined(__linux__) && defined(MUSL_LIBC)
//Linux combined with musl can fall back to native ioctl to set CRTSCTS

#include <linux/termios.h>
#include <cerrno>
#include <system_error>

// Avoid sys/ioctl.h conflicting with linux/termios.h
extern "C" int ioctl(int fd, unsigned long request, ...);

bool native_hardware_flow_control(int fd, bool val)
{
	struct termios2 tio;
	if (ioctl(fd, TCGETS2, &tio) < 0)
		throw std::system_error(errno, std::generic_category(), "TCGETS2");

	if(val)
		tio.c_cflag |= CRTSCTS;
	else
		tio.c_cflag &= ~CRTSCTS;

	if (ioctl(fd, TCSETS2, &tio) < 0)
		throw std::system_error(errno, std::generic_category(), "TCSETS2");

	return true;
}
#endif //linux+musl
