/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim:set tw=80 expandtab softtabstop=4 ts=4 sw=4:
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
 *   Chris Saari <saari@netscape.com>
 *   Christian Biesinger <cbiesinger@web.de>
 *   David Hyatt <hyatt@netscape.com>
 */

#include "nsXBMDecoder.h"
#include "nsIComponentManager.h"
#include "nsIGenericFactory.h"
#include "nsISupports.h"
#include "nsCOMPtr.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsXBMDecoder)

static nsModuleComponentInfo components[] =
{
  { "XBM Decoder",
     NS_XBMDECODER_CID,
     "@mozilla.org/image/decoder;2?type=image/x-xbitmap",
     nsXBMDecoderConstructor, },
  { "XBM Decoder",
     NS_XBMDECODER_CID,
     "@mozilla.org/image/decoder;2?type=image/x-xbm",
     nsXBMDecoderConstructor, },
  { "XBM Decoder",
     NS_XBMDECODER_CID,
     "@mozilla.org/image/decoder;2?type=image/xbm",
     nsXBMDecoderConstructor, },
};

NS_IMPL_NSGETMODULE(nsXBMModule, components)


