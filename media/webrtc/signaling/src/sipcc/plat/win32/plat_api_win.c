/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_ipc.h"
#include "cpr_types.h"

#include "windows.h"

#include "dns_utils.h"
#include "phone_debug.h"
#include "util_string.h"

static char ip_address[16];

char* sipcc_platGetIPAddr (void) {
	char szHostName[128] = "";
	struct sockaddr_in SocketAddress;
	struct hostent     *pHost        = 0;


	WSADATA WSAData;

	// Initialize winsock dll
	if(WSAStartup(MAKEWORD(1, 0), &WSAData))
	{
		return "";
	}


	if(gethostname(szHostName, sizeof(szHostName)))
	{
	  // Error handling -> call 'WSAGetLastError()'
	}

	pHost = gethostbyname(szHostName);
	if(!pHost)
	{
		return "";
	}

	/*
	 There is a potential problem here...what if the machine has multiple IP Addresses?
	 At the moment, I default to using the first
	 */
	memcpy(&SocketAddress.sin_addr, pHost->h_addr_list[0], pHost->h_length);
	sstrncpy(ip_address, inet_ntoa(SocketAddress.sin_addr), sizeof(ip_address));

	WSACleanup();
	return ip_address;
}
