/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIObserverList_h__
#define nsIObserverList_h__

#include "nsISupports.h"
#include "nsIObserver.h"
#include "nsIEnumerator.h"
#include "nscore.h"


// {E777D482-E6E3-11d2-8ACD-00105A1B8860}
#define NS_IOBSERVERLIST_IID \
{ 0xe777d482, 0xe6e3, 0x11d2, { 0x8a, 0xcd, 0x0, 0x10, 0x5a, 0x1b, 0x88, 0x60 } }

class nsIObserverList : public nsISupports {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IOBSERVERLIST_IID; return iid; }
    
    NS_IMETHOD AddObserver(nsIObserver* anObserver) = 0;
    NS_IMETHOD RemoveObserver(nsIObserver* anObserver) = 0;
	NS_IMETHOD EnumerateObserverList(nsIEnumerator** anEnumerator) = 0;
   
};

extern NS_COM nsresult NS_NewObserverList(nsIObserverList** anObserverList);

#define NS_OBSERVERLIST_CONTRACTID "@mozilla.org/xpcom/observer-list;1"
#define NS_OBSERVERLIST_CLASSNAME "Observer List"

#endif /* nsIObserverList_h__ */
