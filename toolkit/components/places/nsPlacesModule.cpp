/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsIClassInfoImpl.h"

#include "nsAnnoProtocolHandler.h"
#include "nsAnnotationService.h"
#include "nsNavHistory.h"
#include "nsNavBookmarks.h"
#include "nsFaviconService.h"
#include "History.h"
#include "nsDocShellCID.h"

using namespace mozilla::places;

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsNavHistory,
                                         nsNavHistory::GetSingleton)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsAnnotationService,
                                         nsAnnotationService::GetSingleton)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsNavBookmarks,
                                         nsNavBookmarks::GetSingleton)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsFaviconService,
                                         nsFaviconService::GetSingleton)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(History, History::GetSingleton)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsAnnoProtocolHandler)
NS_DEFINE_NAMED_CID(NS_NAVHISTORYSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_ANNOTATIONSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_ANNOPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_NAVBOOKMARKSSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_FAVICONSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_HISTORYSERVICE_CID);

const mozilla::Module::CIDEntry kPlacesCIDs[] = {
  { &kNS_NAVHISTORYSERVICE_CID, false, NULL, nsNavHistoryConstructor },
  { &kNS_ANNOTATIONSERVICE_CID, false, NULL, nsAnnotationServiceConstructor },
  { &kNS_ANNOPROTOCOLHANDLER_CID, false, NULL, nsAnnoProtocolHandlerConstructor },
  { &kNS_NAVBOOKMARKSSERVICE_CID, false, NULL, nsNavBookmarksConstructor },
  { &kNS_FAVICONSERVICE_CID, false, NULL, nsFaviconServiceConstructor },
  { &kNS_HISTORYSERVICE_CID, false, NULL, HistoryConstructor },
  { NULL }
};

const mozilla::Module::ContractIDEntry kPlacesContracts[] = {
  { NS_NAVHISTORYSERVICE_CONTRACTID, &kNS_NAVHISTORYSERVICE_CID },
  { NS_GLOBALHISTORY2_CONTRACTID, &kNS_NAVHISTORYSERVICE_CID },
  { NS_ANNOTATIONSERVICE_CONTRACTID, &kNS_ANNOTATIONSERVICE_CID },
  { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "moz-anno", &kNS_ANNOPROTOCOLHANDLER_CID },
  { NS_NAVBOOKMARKSSERVICE_CONTRACTID, &kNS_NAVBOOKMARKSSERVICE_CID },
  { NS_FAVICONSERVICE_CONTRACTID, &kNS_FAVICONSERVICE_CID },
  { "@mozilla.org/embeddor.implemented/bookmark-charset-resolver;1", &kNS_NAVHISTORYSERVICE_CID },
  { NS_IHISTORY_CONTRACTID, &kNS_HISTORYSERVICE_CID },
  { NS_DOWNLOADHISTORY_CONTRACTID, &kNS_HISTORYSERVICE_CID },
  { NULL }
};

const mozilla::Module::CategoryEntry kPlacesCategories[] = {
  { "vacuum-participant", "Places", NS_NAVHISTORYSERVICE_CONTRACTID },
  { NULL }
};

const mozilla::Module kPlacesModule = {
  mozilla::Module::kVersion,
  kPlacesCIDs,
  kPlacesContracts,
  kPlacesCategories
};

NSMODULE_DEFN(nsPlacesModule) = &kPlacesModule;
