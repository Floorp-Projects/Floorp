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

#ifndef nsObserverService_h___
#define nsObserverService_h___

#include "nsIObserverService.h"

class nsHashtable;

class nsObserverService : public nsIObserverService {
public:

	static nsresult GetObserverService(nsIObserverService** anObserverService);
    
	NS_IMETHOD AddObserver(nsIObserver** anObserver, nsString* aTopic);
  NS_IMETHOD RemoveObserver(nsIObserver** anObserver, nsString* aTopic);
	NS_IMETHOD EnumerateObserverList(nsIEnumerator** anEnumerator, nsString* aTopic);

   
  nsObserverService();
  virtual ~nsObserverService(void);
     
  NS_DECL_ISUPPORTS

  static NS_METHOD
  Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

private:
	
  NS_IMETHOD GetObserverList(nsIObserverList** anObserverList, nsString* aTopic);

  nsHashtable* mObserverTopicTable;

};

#endif /* nsObserverService_h___ */
