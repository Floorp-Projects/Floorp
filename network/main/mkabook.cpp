/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
// 
// mkabook.cpp -- Handles "addbook:" " URLs for core Navigator, without
//               requiring libmsg. mkabook is intended for adding 
//               to the address-book.
//
//

#include "mkutils.h" 

#include "xp.h"
#include "xp_str.h"

#include "mkgeturl.h"
#include "mkabook.h"
#include "addrbook.h"

//
// Callbacks from NET_GetURL 
//

extern "C" int32 net_AddressBookLoad (ActiveEntry *ce)
{
	char * url = ce->URL_s->address;
	char * path = NET_ParseURL(url, GET_PATH_PART);
	char * search = NET_ParseURL(url, GET_SEARCH_PART);
	if (!XP_STRNCASECMP(path,"add",3)) {
		if (!XP_STRNCASECMP (search, "?vcard=", 7)) {
			ABook* addressbook = FE_GetAddressBook(NULL);
			if (addressbook)
				AB_ImportFromVcardURL(addressbook, ce->window_id, NET_UnEscape(search+7));
		}
	}

	return -1;
}


extern "C" int32 net_ProcessAddressBook (ActiveEntry *ce)
{
	XP_ASSERT(0);
	return -1;
}


extern "C" int32 net_InterruptAddressBook (ActiveEntry * ce)
{
	XP_ASSERT(0);
	return -1;
}

extern "C" void
net_CleanupAddressBook(void)
{
}

MODULE_PRIVATE void
NET_InitAddressBookProtocol(void)
{
    static NET_ProtoImpl abook_proto_impl;

    abook_proto_impl.init = net_AddressBookLoad;
    abook_proto_impl.process = net_ProcessAddressBook;
    abook_proto_impl.interrupt = net_InterruptAddressBook;
    abook_proto_impl.cleanup = net_CleanupAddressBook;

    NET_RegisterProtocolImplementation(&abook_proto_impl, ADDRESS_BOOK_TYPE_URL);
}

