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
#ifndef __nsInternetConfigService_h
#define __nsInternetConfigService_h

#include "nsIInternetConfigService.h"
#include "nsInternetConfig.h"

#define NS_INTERNETCONFIGSERVICE_CID \
  {0x9b8b9d81, 0x5f4f, 0x11d4, \
    { 0x96, 0x96, 0x00, 0x60, 0x08, 0x3a, 0x0b, 0xcf }}

class nsInternetConfigService : public nsIInternetConfigService
{
public:

  NS_DECL_ISUPPORTS
  NS_DECL_NSIINTERNETCONFIGSERVICE

  OSStatus GetMappingForMIMEType(const char *mimetype, const char *fileextension, ICMapEntry *entry);

  nsInternetConfigService();
  virtual ~nsInternetConfigService();
};

#endif
