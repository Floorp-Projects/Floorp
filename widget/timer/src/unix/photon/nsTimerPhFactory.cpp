/* -*- Mode: C++; tab-width: 2; indentT-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsIModule.h"
#include "nsCOMPtr.h"

#include "nsUnixTimerCIID.h"
#include "nsTimerPh.h"

// {48B62AD2-48D3-11d3-B224-000064657374}
#define NS_TIMER_PH_CID \
{ 0x48b62ad2, 0x48d3, 0x11d3, \
  { 0xb2, 0x24, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

NS_GENERIC_FACTORY_CONSTRUCTOR(nsTimerPh)

static nsModuleComponentInfo components[] =
{
  { "Photon timer",
    NS_TIMER_PH_CID,
    "@mozilla.org/timer;1", 
    nsTimerPhConstructor }
};

NS_IMPL_NSGETMODULE("nsPhTimerModule", components)
