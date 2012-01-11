/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Veditz <dveditz@netscape.com>
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

#include <string.h>

#include "nscore.h"
#include "pratom.h"
#include "prmem.h"
#include "prio.h"
#include "plstr.h"
#include "prlog.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "mozilla/ModuleUtils.h"
#include "nsIJARFactory.h"
#include "nsJARProtocolHandler.h"
#include "nsJARURI.h"
#include "nsJAR.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsJAR)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsZipReaderCache)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsJARProtocolHandler,
                                         nsJARProtocolHandler::GetSingleton)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsJARURI)

NS_DEFINE_NAMED_CID(NS_ZIPREADER_CID);
NS_DEFINE_NAMED_CID(NS_ZIPREADERCACHE_CID);
NS_DEFINE_NAMED_CID(NS_JARPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_JARURI_CID);

static const mozilla::Module::CIDEntry kJARCIDs[] = {
    { &kNS_ZIPREADER_CID, false, NULL, nsJARConstructor },
    { &kNS_ZIPREADERCACHE_CID, false, NULL, nsZipReaderCacheConstructor },
    { &kNS_JARPROTOCOLHANDLER_CID, false, NULL, nsJARProtocolHandlerConstructor },
    { &kNS_JARURI_CID, false, NULL, nsJARURIConstructor },
    { NULL }
};

static const mozilla::Module::ContractIDEntry kJARContracts[] = {
    { "@mozilla.org/libjar/zip-reader;1", &kNS_ZIPREADER_CID },
    { "@mozilla.org/libjar/zip-reader-cache;1", &kNS_ZIPREADERCACHE_CID },
    { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "jar", &kNS_JARPROTOCOLHANDLER_CID },
    { NULL }
};

// Jar module shutdown hook
static void nsJarShutdown()
{
    NS_IF_RELEASE(gJarHandler);
}

static const mozilla::Module kJARModule = {
    mozilla::Module::kVersion,
    kJARCIDs,
    kJARContracts,
    NULL,
    NULL,
    NULL,
    nsJarShutdown
};

NSMODULE_DEFN(nsJarModule) = &kJARModule;
