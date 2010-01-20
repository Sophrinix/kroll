/**
 * Appcelerator Kroll - licensed under the Apache Public License 2
 * see LICENSE in the root folder for details on the license.
 * Copyright (c) 2009 Appcelerator, Inc. All Rights Reserved.
 */

// Copyright (c) 2004-2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
// 
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
#include "../utils.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>

namespace UTILS_NS
{
namespace PlatformUtils
{
	void GetNodeId(NodeId& id)
	{
		//Based on code from:
		//http://adywicaksono.wordpress.com/2007/11/08/detecting-mac-address-using-c-application/
		struct ifreq ifr;
		struct ifconf ifc;
		char buf[1024];
		u_char addr[6] = {'\0','\0','\0','\0','\0','\0'};
		int s,a;

		s = socket(AF_INET, SOCK_DGRAM, 0);
		if (s != -1)
		{
			ifc.ifc_len = sizeof(buf);
			ifc.ifc_buf = buf;
			ioctl(s, SIOCGIFCONF, &ifc);
			struct ifreq* IFR = ifc.ifc_req;
			bool success = false;
			for (a = ifc.ifc_len / sizeof(struct ifreq); --a >= 0; IFR++) {
				strcpy(ifr.ifr_name, IFR->ifr_name);
				if (ioctl(s, SIOCGIFFLAGS, &ifr) == 0
					&& (!(ifr.ifr_flags & IFF_LOOPBACK))
					&& (ioctl(s, SIOCGIFHWADDR, &ifr) == 0))
				{
					success = true;
					bcopy(ifr.ifr_hwaddr.sa_data, addr, 6);
					break;
				}
			}
			close(s);

			if (success)
			{
				memcpy(&id, addr, sizeof(id));
			}
			else
			{
				throw "00:00:00:00:00:00";
			}
		}
	}

	std::string GetUsername()
	{
		char* loginName = getlogin();
		if (loginName != NULL)
		{
			return loginName;
		}
		else if (EnvironmentUtils::Has("USER"))
		{
			return EnvironmentUtils::Get("USER");
		}
		else
		{
			return "unknown";
		}
	}
}
}
