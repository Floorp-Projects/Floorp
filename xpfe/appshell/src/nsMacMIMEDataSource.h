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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsIMIMEDataSource.h"
#include "nsInternetConfig.h"

#define NS_NATIVEMIMEDATASOURCE_CID                           \
{ /* 95df6581-0001-11d4-a12b-a66ef662f0bc */         \
    0x95df6581,                                      \
    0x0001,                                          \
    0x11d4,                                          \
    {0xa1, 0x2b, 0xa6, 0x6e, 0xf6, 0x62, 0xf0, 0xbc} \
}

class nsMacMIMEDataSource: public nsIMIMEDataSource
{
public:
					nsMacMIMEDataSource();
  virtual ~nsMacMIMEDataSource();
	NS_DECL_ISUPPORTS
  NS_DECL_NSIMIMEDATASOURCE
  
private:
	nsInternetConfig mIC;
};