/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "nsAutoComplete.h"
#include "nsBookmarksService.h"
#include "nsDirectoryViewer.h"
#include "nsGlobalHistory.h"
#include "nsLocalSearchService.h"
#include "nsInternetSearchService.h"
#include "nsRelatedLinksHandlerImpl.h"
#include "nsTimeBomb.h"
#include "nsUrlbarHistory.h"
#if defined(XP_PC) && !defined(XP_OS2)
#include "nsUrlWidget.h"
#include "nsWindowsHooks.h"
#endif // Windows

// Factory constructors
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAutoCompleteItem)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAutoCompleteResults)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsBookmarksService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsHTTPIndex, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDirectoryViewerFactory)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsGlobalHistory, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(LocalSearchDataSource, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(InternetSearchDataSource, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(RelatedLinksHandlerImpl, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTimeBomb)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUrlbarHistory)
#if defined(XP_PC) && !defined(XP_OS2)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsUrlWidget, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWindowsHooks)
#endif // Windows

static nsModuleComponentInfo components[] = {
    { "AutoComplete Search Results", NS_AUTOCOMPLETERESULTS_CID, NS_AUTOCOMPLETERESULTS_PROGID,
      nsAutoCompleteResultsConstructor},
    { "AutoComplete Search Item", NS_AUTOCOMPLETEITEM_CID, NS_AUTOCOMPLETEITEM_PROGID,
      nsAutoCompleteItemConstructor},
    { "Bookmarks", NS_BOOKMARKS_SERVICE_CID, NS_BOOKMARKS_SERVICE_PROGID,
      nsBookmarksServiceConstructor },
    { "Bookmarks", NS_BOOKMARKS_SERVICE_CID, NS_BOOKMARKS_DATASOURCE_PROGID,
      nsBookmarksServiceConstructor },
    { "Directory Viewer", NS_DIRECTORYVIEWERFACTORY_CID,
      NS_DOCUMENT_LOADER_FACTORY_PROGID_PREFIX "view/application/http-index-format",
      nsDirectoryViewerFactoryConstructor },
    { "Directory Viewer", NS_HTTPINDEX_SERVICE_CID, NS_HTTPINDEX_SERVICE_PROGID,
      nsHTTPIndexConstructor },
    { "Directory Viewer", NS_HTTPINDEX_SERVICE_CID, NS_HTTPINDEX_DATASOURCE_PROGID,
      nsHTTPIndexConstructor },
    { "Global History", NS_GLOBALHISTORY_CID, NS_GLOBALHISTORY_PROGID,
      nsGlobalHistoryConstructor },
    { "Global History", NS_GLOBALHISTORY_CID, NS_GLOBALHISTORY_DATASOURCE_PROGID,
      nsGlobalHistoryConstructor },
    { "Local Search", NS_RDFFINDDATASOURCE_CID,
      NS_LOCALSEARCH_SERVICE_PROGID, LocalSearchDataSourceConstructor },
    { "Local Search", NS_RDFFINDDATASOURCE_CID,
      NS_LOCALSEARCH_DATASOURCE_PROGID, LocalSearchDataSourceConstructor },
    { "Internet Search", NS_RDFSEARCHDATASOURCE_CID,
      NS_INTERNETSEARCH_SERVICE_PROGID, InternetSearchDataSourceConstructor },
    { "Internet Search", NS_RDFSEARCHDATASOURCE_CID,
      NS_INTERNETSEARCH_DATASOURCE_PROGID, InternetSearchDataSourceConstructor },
    { "Related Links Handler", NS_RELATEDLINKSHANDLER_CID, NS_RELATEDLINKSHANDLER_PROGID,
	  RelatedLinksHandlerImplConstructor},
    { "Netscape TimeBomb", NS_TIMEBOMB_CID, NS_TIMEBOMB_PROGID, nsTimeBombConstructor},
    { "nsUrlbarHistory", NS_URLBARHISTORY_CID,
      NS_URLBARHISTORY_PROGID, nsUrlbarHistoryConstructor },
    { "nsUrlbarHistory", NS_URLBARHISTORY_CID,
      NS_URLBARAUTOCOMPLETE_PROGID, nsUrlbarHistoryConstructor },
#if defined(XP_PC) && !defined(XP_OS2)
    { NS_IURLWIDGET_CLASSNAME, NS_IURLWIDGET_CID, NS_IURLWIDGET_PROGID, 
      nsUrlWidgetConstructor }, 
    { NS_IWINDOWSHOOKS_CLASSNAME, NS_IWINDOWSHOOKS_CID, NS_IWINDOWSHOOKS_PROGID, 
      nsWindowsHooksConstructor },
#endif // Windows
};

NS_IMPL_NSGETMODULE("application", components)
