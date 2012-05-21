/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#define NS_XREMOTECLIENT_CID                         \
{ 0xcfae5900,                                        \
  0x1dd1,                                            \
  0x11b2,                                            \
  { 0x95, 0xd0, 0xad, 0x45, 0x4c, 0x23, 0x3d, 0xc6 } \
}

/* cfae5900-1dd1-11b2-95d0-ad454c233dc6 */

#include "XRemoteClient.h"

#include "nsXRemoteClientCID.h"
#include "nsIGenericFactory.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(XRemoteClient)

static const nsModuleComponentInfo components[] =
{
  { "XRemote Client",
    NS_XREMOTECLIENT_CID,
    NS_XREMOTECLIENT_CONTRACTID,
    XRemoteClientConstructor }
};

NS_IMPL_NSGETMODULE(XRemoteClientModule, components)


