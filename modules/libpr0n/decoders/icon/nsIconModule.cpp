/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIGenericFactory.h"
#include "nsIModule.h"

#if defined(XP_MACOSX) || defined(XP_OS2)
#include "nsIconDecoder.h"
#endif
#include "nsIconProtocolHandler.h"

// objects that just require generic constructors
/******************************************************************************
 * Protocol CIDs
 */
#define NS_ICONPROTOCOL_CID   { 0xd0f9db12, 0x249c, 0x11d5, { 0x99, 0x5, 0x0, 0x10, 0x83, 0x1, 0xe, 0x9b } } 

#if defined(XP_MACOSX) || defined(XP_OS2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsIconDecoder)
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR(nsIconProtocolHandler)

static const nsModuleComponentInfo components[] =
{
#if defined(XP_MACOSX) || defined(XP_OS2)
  { "icon decoder",
    NS_ICONDECODER_CID,
    "@mozilla.org/image/decoder;2?type=image/icon",
    nsIconDecoderConstructor, },
#endif

   { "Icon Protocol Handler",      
      NS_ICONPROTOCOL_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "moz-icon",
      nsIconProtocolHandlerConstructor
    }
};

NS_IMPL_NSGETMODULE(nsIconDecoderModule, components)
