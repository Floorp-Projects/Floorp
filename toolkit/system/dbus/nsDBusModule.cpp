/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsServiceManagerUtils.h"
#include "nsICategoryManager.h"
#include "mozilla/ModuleUtils.h"
#include "nsIAppStartupNotifier.h"
#include "nsNetworkManagerListener.h"
#include "nsNetCID.h"

#define NS_DBUS_NETWORK_LINK_SERVICE_CLASSNAME "DBus Network Link Status"
#define NS_DBUS_NETWORK_LINK_SERVICE_CID    \
  { 0x75a500a2,                                        \
    0x0030,                                            \
    0x40f7,                                            \
    { 0x86, 0xf8, 0x63, 0xf2, 0x25, 0xb9, 0x40, 0xae } \
  }
  
/* ===== XPCOM registration stuff ======== */

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsNetworkManagerListener, Init)
NS_DEFINE_NAMED_CID(NS_DBUS_NETWORK_LINK_SERVICE_CID);

static const mozilla::Module::CIDEntry kDBUSCIDs[] = {
    { &kNS_DBUS_NETWORK_LINK_SERVICE_CID, false, NULL, nsNetworkManagerListenerConstructor },
    { NULL }
};

static const mozilla::Module::ContractIDEntry kDBUSContracts[] = {
    { NS_NETWORK_LINK_SERVICE_CONTRACTID, &kNS_DBUS_NETWORK_LINK_SERVICE_CID },
    { NULL }
};

static const mozilla::Module kDBUSModule = {
    mozilla::Module::kVersion,
    kDBUSCIDs,
    kDBUSContracts
};

NSMODULE_DEFN(nsDBusModule) = &kDBUSModule;
