/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsIModule.h"

#include "nsXPComFactory.h"

#include "nsIHTTPProtocolHandler.h"
#include "nsHTTPHandler.h"
#include "nsHTTPSHandler.h"

////////////////////////////////////////////////////////////////////////

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsHTTPHandler, Init);
//NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTTPSHandler);

#define NS_HTTPS_HANDLER_FACTORY_CID { 0xd2771480, 0xcac4, 0x11d3, { 0x8c, 0xaf, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

static nsModuleComponentInfo components[] =
{
  { "HTTP Handler",
    NS_IHTTPHANDLER_CID,
    NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http",
    nsHTTPHandlerConstructor },
  { "HTTPS Handler",
    NS_HTTPS_HANDLER_FACTORY_CID,
    NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "https",
    nsHTTPSHandler::Create }
};
  
NS_IMPL_NSGETMODULE("nsHTTPHandlerModule", components)
