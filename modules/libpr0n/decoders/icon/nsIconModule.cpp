/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Scott MacGregor <mscott@netscape.com>
 */

#include "nsIGenericFactory.h"
#include "nsIModule.h"

#include "nsIconDecoder.h"
#include "nsIconProtocolHandler.h"

// objects that just require generic constructors
/******************************************************************************
 * Protocol CIDs
 */
#define NS_ICONPROTOCOL_CID   { 0xd0f9db12, 0x249c, 0x11d5, { 0x99, 0x5, 0x0, 0x10, 0x83, 0x1, 0xe, 0x9b } } 

NS_GENERIC_FACTORY_CONSTRUCTOR(nsIconDecoder)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsIconProtocolHandler)

static const nsModuleComponentInfo components[] =
{
  { "icon decoder",
    NS_ICONDECODER_CID,
    "@mozilla.org/image/decoder;2?type=image/icon",
    nsIconDecoderConstructor, },

   { "Icon Protocol Handler",      
      NS_ICONPROTOCOL_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "moz-icon",
      nsIconProtocolHandlerConstructor
    }
};

NS_IMPL_NSGETMODULE(nsIconDecoderModule, components)
