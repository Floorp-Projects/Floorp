/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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

#ifndef nsIObserverService_h__
#define nsIObserverService_h__

#include "nsISupports.h"
#include "nsIObserverList.h"
#include "nsString.h"


// {D07F5192-E3D1-11d2-8ACD-00105A1B8860}
#define NS_IOBSERVERSERVICE_IID \
{ 0xd07f5192, 0xe3d1, 0x11d2, { 0x8a, 0xcd, 0x0, 0x10, 0x5a, 0x1b, 0x88, 0x60 } }


// {D07F5195-E3D1-11d2-8ACD-00105A1B8860}
#define NS_OBSERVERSERVICE_CID \
{ 0xd07f5195, 0xe3d1, 0x11d2, { 0x8a, 0xcd, 0x0, 0x10, 0x5a, 0x1b, 0x88, 0x60 } }


class nsIObserverService : public nsISupports {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IOBSERVERSERVICE_IID; return iid; }

    NS_IMETHOD  GetObserverList(nsString* aTopic, nsIObserverList** anObserverList) = 0;    
};

extern NS_BASE nsresult NS_NewObserverService(nsIObserverService** anObserverService);

#define NS_OBSERVERSERVICE_PROGID             "component:||netscape|ObserverService"


#endif /* nsIObserverService_h__ */
