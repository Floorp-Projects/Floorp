#include "nsIGenericFactory.h"
#include "nsIClassInfoImpl.h"

#include "nsAnnoProtocolHandler.h"
#include "nsAnnotationService.h"
#include "nsNavHistory.h"
#include "nsNavBookmarks.h"
#include "nsFaviconService.h"
#include "nsDocShellCID.h"

#define NS_NAVHISTORY_CLASSINFO \
  nsnull, nsnull, nsnull, \
  NS_CI_INTERFACE_GETTER_NAME(nsNavHistory), \
  nsnull, \
  &NS_CLASSINFO_NAME(nsNavHistory), \
  nsIClassInfo::SINGLETON

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsNavHistory,
                                         nsNavHistory::GetSingleton)
NS_DECL_CLASSINFO(nsNavHistory)

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsAnnotationService,
                                         nsAnnotationService::GetSingleton)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsNavBookmarks,
                                         nsNavBookmarks::GetSingleton)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsFaviconService,
                                         nsFaviconService::GetSingleton)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsAnnoProtocolHandler)

static const nsModuleComponentInfo components[] =
{
  { "Browser Navigation History",
    NS_NAVHISTORYSERVICE_CID,
    NS_NAVHISTORYSERVICE_CONTRACTID,
    nsNavHistoryConstructor,
    NS_NAVHISTORY_CLASSINFO },

  { "Browser Navigation History",
    NS_NAVHISTORYSERVICE_CID,
    NS_GLOBALHISTORY2_CONTRACTID,
    nsNavHistoryConstructor,
    NS_NAVHISTORY_CLASSINFO },

  { "Download Navigation History",
    NS_NAVHISTORYSERVICE_CID,
    NS_DOWNLOADHISTORY_CONTRACTID,
    nsNavHistoryConstructor,
    NS_NAVHISTORY_CLASSINFO },

  { "Page Annotation Service",
    NS_ANNOTATIONSERVICE_CID,
    NS_ANNOTATIONSERVICE_CONTRACTID,
    nsAnnotationServiceConstructor },

  { "Annotation Protocol Handler",
    NS_ANNOPROTOCOLHANDLER_CID,
    NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "moz-anno",
    nsAnnoProtocolHandlerConstructor },

  { "Browser Bookmarks Service",
    NS_NAVBOOKMARKSSERVICE_CID,
    NS_NAVBOOKMARKSSERVICE_CONTRACTID,
    nsNavBookmarksConstructor },

  { "Favicon Service",
    NS_FAVICONSERVICE_CID,
    NS_FAVICONSERVICE_CONTRACTID,
    nsFaviconServiceConstructor },

  { "Browser History Charset Resolver",
    NS_NAVHISTORYSERVICE_CID,
    "@mozilla.org/embeddor.implemented/bookmark-charset-resolver;1",
    nsNavHistoryConstructor,
    NS_NAVHISTORY_CLASSINFO },

};

NS_IMPL_NSGETMODULE(nsPlacesModule, components)
