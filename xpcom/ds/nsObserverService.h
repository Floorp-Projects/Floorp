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
#include "nsIObserverList.h"

class nsObjectHashtable;
class nsString;

// {D07F5195-E3D1-11d2-8ACD-00105A1B8860}
#define NS_OBSERVERSERVICE_CID \
    { 0xd07f5195, 0xe3d1, 0x11d2, { 0x8a, 0xcd, 0x0, 0x10, 0x5a, 0x1b, 0x88, 0x60 } }

class nsObserverService : public nsIObserverService {
public:
    NS_DEFINE_STATIC_CID_ACCESSOR( NS_OBSERVERSERVICE_CID )

	static nsresult GetObserverService(nsIObserverService** anObserverService);
    
    NS_DECL_NSIOBSERVERSERVICE
   
  nsObserverService();
  virtual ~nsObserverService(void);
     
  NS_DECL_ISUPPORTS

  static NS_METHOD
  Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

private:
	
  NS_IMETHOD GetObserverList(const nsString& aTopic, nsIObserverList** anObserverList);

  nsObjectHashtable* mObserverTopicTable;

};

#endif /* nsObserverService_h___ */
