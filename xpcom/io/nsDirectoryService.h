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

#ifndef nsDirectoryService_h___
#define nsDirectoryService_h___

#include "nsIDirectoryService.h"
#include "nsHashtable.h"
#include "nsIFile.h"
#include "nsISupportsArray.h"
class nsDirectoryService : public nsIDirectoryService, public nsIProperties, public nsIDirectoryServiceProvider
{
  public:

  NS_DEFINE_STATIC_CID_ACCESSOR(NS_DIRECTORY_SERVICE_CID);

  // nsISupports interface
  NS_DECL_ISUPPORTS

  NS_DECL_NSIPROPERTIES  

  NS_DECL_NSIDIRECTORYSERVICE

  NS_DECL_NSIDIRECTORYSERVICEPROVIDER

  nsDirectoryService();
  virtual ~nsDirectoryService();

  static NS_METHOD
  Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

private:
    static nsDirectoryService* mService;
    static PRBool ReleaseValues(nsHashKey* key, void* data, void* closure);
    nsHashtable* mHashtable;
    nsCOMPtr<nsISupportsArray> mProviders;
};


#endif

