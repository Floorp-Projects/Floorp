/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsDnsService_h__
#define nsDnsService_h__

#include "nsIDnsService.h"


class nsIDnsListener;
class nsICancelable;

class nsDnsService : public nsIDnsService
{
public:
     NS_DECL_ISUPPORTS

   // nsDnsService methods:
    nsDnsService();
    virtual ~nsDnsService();
    nsresult Init();
 
 	// nsIDnsService methods:
 	NS_IMETHOD Lookup(const char*     hostname,
                      nsIDnsListener* listener,
                      nsICancelable*  *dnsRequest);
protected:
    // nsDnsLookup cache? - list of nsDnsLookups

#if defined(XP_MAC)
    InetSvcRef  mServiceRef;

#elif defined(XP_UNIX)
    //XXX - to be defined

#elif defined(_WIN32)
    WNDCLASS wc;
    HWND     dnsWindow;
    UINT     msgAsyncSelect;
    UINT     msgFoundDNS;
#endif
};

#endif /* nsDnsService_h__ */
