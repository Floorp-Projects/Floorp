/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsObserverList_h___
#define nsObserverList_h___

#include "nsIObserverList.h"
#include "nsIEnumerator.h"
#include "nsISupportsArray.h"

// {E777D484-E6E3-11d2-8ACD-00105A1B8860}
#define NS_OBSERVERLIST_CID \
{ 0xe777d484, 0xe6e3, 0x11d2, { 0x8a, 0xcd, 0x0, 0x10, 0x5a, 0x1b, 0x88, 0x60 } }

class nsObserverList : public nsIObserverList
{
public:

               nsObserverList();
    virtual    ~nsObserverList();

 		NS_DEFINE_STATIC_CID_ACCESSOR(NS_OBSERVERLIST_CID);

    NS_DECL_ISUPPORTS

    NS_IMETHOD AddObserver(nsIObserver* anObserver);
    NS_IMETHOD RemoveObserver(nsIObserver* anObserver);

	NS_IMETHOD EnumerateObserverList(nsIEnumerator** anEnumerator);

public:

    static NS_METHOD Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);
     
protected:

    // This is ObserverList monitor object.
    PRLock* mLock;
   
private:

 	  nsCOMPtr<nsISupportsArray>  mObserverList;

};


#endif /* nsObserverList_h___ */
