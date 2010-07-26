/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is Novell code.
 *
 * The Initial Developer of the Original Code is Novell, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Original Author: Robert O'Callahan (rocallahan@novell.com)
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
