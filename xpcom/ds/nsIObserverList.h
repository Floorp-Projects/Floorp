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

#ifndef nsIObserverList_h__
#define nsIObserverList_h__

#include "nsISupports.h"
#include "nsIObserver.h"
#include "nsIEnumerator.h"
#include "nscore.h"


// {E777D482-E6E3-11d2-8ACD-00105A1B8860}
#define NS_IOBSERVERLIST_IID \
{ 0xe777d482, 0xe6e3, 0x11d2, { 0x8a, 0xcd, 0x0, 0x10, 0x5a, 0x1b, 0x88, 0x60 } };

// {E777D484-E6E3-11d2-8ACD-00105A1B8860}
#define NS_OBSERVERLIST_CID \
{ 0xe777d484, 0xe6e3, 0x11d2, { 0x8a, 0xcd, 0x0, 0x10, 0x5a, 0x1b, 0x88, 0x60 } };

class nsIObserverList : public nsISupports {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IOBSERVERLIST_IID; return iid; }
    
    NS_IMETHOD AddObserver(nsIObserver** anObserver) = 0;
    NS_IMETHOD RemoveObserver(nsIObserver** anObserver) = 0;
	NS_IMETHOD EnumerateObserverList(nsIEnumerator** anEnumerator) = 0;
   
};

extern NS_BASE nsresult NS_NewObserverList(nsIObserverList** anObserverList);

#endif /* nsIObserverList_h__ */
