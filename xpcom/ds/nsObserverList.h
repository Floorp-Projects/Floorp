/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights
 * Reserved.
 */

#ifndef nsObserverList_h___
#define nsObserverList_h___

#include "nsIObserverList.h"
#include "nsIEnumerator.h"
#include "nsISupportsArray.h"


class nsObserverList : public nsIObserverList {
public:

    NS_IMETHOD AddObserver(nsIObserver** anObserver);
    NS_IMETHOD RemoveObserver(nsIObserver** anObserver);

	NS_IMETHOD EnumerateObserverList(nsIEnumerator** anEnumerator);

    nsObserverList();
    virtual ~nsObserverList(void);

    // This is ObserverList monitor object.
    PRLock* mLock;
     
    NS_DECL_ISUPPORTS

   
private:

 	nsISupportsArray *mObserverList;

};


#endif /* nsObserverList_h___ */
