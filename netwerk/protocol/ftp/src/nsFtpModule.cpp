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
#include "nsFtpProtocolHandler.h"


NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsFtpProtocolHandler, Init);

static nsModuleComponentInfo gResComponents[] = {
    { "The FTP Protocol Handler", 
      NS_FTPPROTOCOLHANDLER_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "ftp",
      nsFtpProtocolHandlerConstructor
    }
};

NS_IMPL_NSGETMODULE("ftp", gResComponents)
