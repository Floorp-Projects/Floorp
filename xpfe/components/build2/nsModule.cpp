/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
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

// minotaur specific stuff that will get smaller and smaller until it shrinks to zero. 

#include "nsIGenericFactory.h"
#include "nsICategoryManager.h"

#include "rdf.h"
#include "nsXPIDLString.h"
#include "nsCharsetMenu.h"
#include "nsFontPackageHandler.h"
#include "nsWindowDataSource.h"
#include "nsRDFCID.h"
#include "nsAutoComplete.h"
#include "nsDownloadManager.h"
#include "nsDownloadProxy.h"

#if defined(MOZ_LDAP_XPCOM)
#include "nsLDAPAutoCompleteSession.h"
#endif

#if defined(XP_WIN)
#include "nsAlertsService.h" 
#include "nsWindowsHooks.h"
#endif // Windows

#include "nsCURILoader.h"

// Factory constructors
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsWindowDataSource, Init)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsAutoCompleteItem)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAutoCompleteResults)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFontPackageHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsDownloadManager, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDownloadProxy)

#if defined(MOZ_LDAP_XPCOM)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLDAPAutoCompleteSession)
#endif

#if defined(XP_WIN)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAlertsService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWindowsHooks)
#endif // Windows

static const nsModuleComponentInfo components[] = {

    { "Download Manager", NS_DOWNLOADMANAGER_CID, NS_DOWNLOADMANAGER_CONTRACTID,
      nsDownloadManagerConstructor },
    { "Download", NS_DOWNLOAD_CID, NS_DOWNLOAD_CONTRACTID,
      nsDownloadProxyConstructor },
    { "AutoComplete Search Results", NS_AUTOCOMPLETERESULTS_CID, NS_AUTOCOMPLETERESULTS_CONTRACTID,
      nsAutoCompleteResultsConstructor},
    { "AutoComplete Search Item", NS_AUTOCOMPLETEITEM_CID, NS_AUTOCOMPLETEITEM_CONTRACTID,
      nsAutoCompleteItemConstructor},  

#if defined(MOZ_LDAP_XPCOM)
    { "LDAP Autocomplete Session", NS_LDAPAUTOCOMPLETESESSION_CID,
	  "@mozilla.org/autocompleteSession;1?type=ldap",
	  nsLDAPAutoCompleteSessionConstructor },
#endif 

#if defined(XP_WIN)
    { "nsAlertsService", NS_ALERTSSERVICE_CID, NS_ALERTSERVICE_CONTRACTID, nsAlertsServiceConstructor},
    { NS_IWINDOWSHOOKS_CLASSNAME, NS_IWINDOWSHOOKS_CID, NS_IWINDOWSHOOKS_CONTRACTID, nsWindowsHooksConstructor },
#endif // Windows

    { "nsCharsetMenu", NS_CHARSETMENU_CID,
      NS_RDF_DATASOURCE_CONTRACTID_PREFIX NS_CHARSETMENU_PID,
      NS_NewCharsetMenu },
    { "nsFontPackageHandler", NS_FONTPACKAGEHANDLER_CID,
      "@mozilla.org/locale/default-font-package-handler;1",
      nsFontPackageHandlerConstructor },
    { "nsWindowDataSource",
      NS_WINDOWDATASOURCE_CID,
      NS_RDF_DATASOURCE_CONTRACTID_PREFIX "window-mediator",
      nsWindowDataSourceConstructor },
};

NS_IMPL_NSGETMODULE(application, components)

