/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "cpr_ipc.h"
#include "cpr_types.h"

#include "windows.h"

#include "dns_utils.h"
#include "phone_debug.h"
#include "util_string.h"

static char ip_address[16];

char* platGetIPAddr (void) {
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
	strcpy(ip_address, inet_ntoa(SocketAddress.sin_addr));

	WSACleanup();
	return ip_address;
}
