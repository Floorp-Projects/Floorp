/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* The nsIInterfaceInfoManager xpcom public declaration. */

#ifndef nsIInterfaceInfoManager_h___
#define nsIInterfaceInfoManager_h___

#include "nsIInterfaceInfo.h"
#include "nsIEnumerator.h"

// This should be implemented as a Service

// {8B161900-BE2B-11d2-9831-006008962422}
#define NS_IINTERFACEINFO_MANAGER_IID   \
{ 0x8b161900, 0xbe2b, 0x11d2,           \
  { 0x98, 0x31, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } }

class nsIInterfaceInfoManager : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IINTERFACEINFO_MANAGER_IID)

    // nsIInformationInfo management services
    NS_IMETHOD GetInfoForIID(const nsIID* iid, nsIInterfaceInfo** info) = 0;
    NS_IMETHOD GetInfoForName(const char* name, nsIInterfaceInfo** info) = 0;

    // name <-> IID mapping services
    // NOTE: these return IAllocatator alloc'd copies of the data
    NS_IMETHOD GetIIDForName(const char* name, nsIID** iid) = 0;
    NS_IMETHOD GetNameForIID(const nsIID* iid, char** name) = 0;

    // Get an enumeration of all the interfaces
    NS_IMETHOD GetInterfaceEnumerator(nsIEnumerator** emumerator) = 0;

    // XXX other methods?

};

#endif /* nsIInterfaceInfoManager_h___ */


