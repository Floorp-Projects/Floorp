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

#include "nsIProperties.h"
#include "nsHashtable.h"
#include "nsAgg.h"


#define NS_DIRECTORY_SERVICE_CID                     \
{ /* f00152d0-b40b-11d3-8c9c-000064657374 */         \
    0xf00152d0,                                      \
    0xb40b,                                          \
    0x11d3,                                          \
    {0x8c, 0x9c, 0x00, 0x00, 0x64, 0x65, 0x73, 0x74} \
}

#define NS_DIRECTORY_SERVICE_PROGID    "component://netscape/file/directory_service"
#define NS_DIRECTORY_SERVICE_CLASSNAME "nsIFile Directory Service"


class nsDirectoryService : public nsIProperties, public nsHashtable {
public:

  NS_DEFINE_STATIC_CID_ACCESSOR(NS_DIRECTORY_SERVICE_CID)

  NS_DECL_AGGREGATED

  // nsIProperties methods:
  NS_IMETHOD DefineProperty(const char* prop, nsISupports* initialValue);
  NS_IMETHOD UndefineProperty(const char* prop);
  NS_IMETHOD GetProperty(const char* prop, nsISupports* *result);
  NS_IMETHOD SetProperty(const char* prop, nsISupports* value);
  NS_IMETHOD HasProperty(const char* prop, nsISupports* value); 

  // nsProperties methods:
  nsDirectoryService(nsISupports* outer);
  virtual ~nsDirectoryService();

  static NS_METHOD
  Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  static PRBool ReleaseValues(nsHashKey* key, void* data, void* closure);

};


#endif