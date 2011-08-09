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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (Original Author)
 *   The Mozilla Foundation <http://www.mozilla.org>
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

#include "mozilla/ModuleUtils.h"
#include "nsAppStartup.h"
#include "nsUserInfo.h"
#include "nsToolkitCompsCID.h"
#include "nsFindService.h"

#if defined(XP_WIN) && !defined(MOZ_DISABLE_PARENTAL_CONTROLS)
#include "nsParentalControlsServiceWin.h"
#endif

#ifdef ALERTS_SERVICE
#include "nsAlertsService.h"
#endif

#ifdef MOZ_RDF
#include "nsDownloadManager.h"
#include "nsDownloadProxy.h"
#include "nsCharsetMenu.h"
#include "rdf.h"
#endif

#include "nsTypeAheadFind.h"

#ifdef MOZ_URL_CLASSIFIER
#include "nsUrlClassifierDBService.h"
#include "nsUrlClassifierStreamUpdater.h"
#include "nsUrlClassifierUtils.h"
#endif

#ifdef MOZ_FEEDS
#include "nsScriptableUnescapeHTML.h"
#endif

#include "nsBrowserStatusFilter.h"

/////////////////////////////////////////////////////////////////////////////

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsAppStartup, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUserInfo)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFindService)

#if defined(XP_WIN) && !defined(MOZ_DISABLE_PARENTAL_CONTROLS)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsParentalControlsServiceWin)
#endif

#ifdef ALERTS_SERVICE
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAlertsService)
#endif

#ifdef MOZ_RDF
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsDownloadManager,
                                         nsDownloadManager::GetSingleton) 
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDownloadProxy)
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(nsTypeAheadFind)

#ifdef MOZ_URL_CLASSIFIER
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUrlClassifierStreamUpdater)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsUrlClassifierUtils, Init)

static nsresult
nsUrlClassifierDBServiceConstructor(nsISupports *aOuter, REFNSIID aIID,
                                    void **aResult)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_NO_AGGREGATION(aOuter);

    nsUrlClassifierDBService *inst = nsUrlClassifierDBService::GetInstance(&rv);
    if (NULL == inst) {
        return rv;
    }
    /* NS_ADDREF(inst); */
    rv = inst->QueryInterface(aIID, aResult);
    NS_RELEASE(inst);

    return rv;
}
#endif

#ifdef MOZ_FEEDS
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScriptableUnescapeHTML)
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(nsBrowserStatusFilter)

NS_DEFINE_NAMED_CID(NS_TOOLKIT_APPSTARTUP_CID);
NS_DEFINE_NAMED_CID(NS_USERINFO_CID);
#ifdef ALERTS_SERVICE
NS_DEFINE_NAMED_CID(NS_ALERTSSERVICE_CID);
#endif
#if defined(XP_WIN) && !defined(MOZ_DISABLE_PARENTAL_CONTROLS)
NS_DEFINE_NAMED_CID(NS_PARENTALCONTROLSSERVICE_CID);
#endif
#ifdef MOZ_RDF
NS_DEFINE_NAMED_CID(NS_DOWNLOADMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_DOWNLOAD_CID);
#endif
NS_DEFINE_NAMED_CID(NS_FIND_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_TYPEAHEADFIND_CID);
#ifdef MOZ_URL_CLASSIFIER
NS_DEFINE_NAMED_CID(NS_URLCLASSIFIERDBSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_URLCLASSIFIERSTREAMUPDATER_CID);
NS_DEFINE_NAMED_CID(NS_URLCLASSIFIERUTILS_CID);
#endif
#ifdef MOZ_FEEDS
NS_DEFINE_NAMED_CID(NS_SCRIPTABLEUNESCAPEHTML_CID);
#endif
NS_DEFINE_NAMED_CID(NS_BROWSERSTATUSFILTER_CID);
NS_DEFINE_NAMED_CID(NS_CHARSETMENU_CID);

static const mozilla::Module::CIDEntry kToolkitCIDs[] = {
  { &kNS_TOOLKIT_APPSTARTUP_CID, false, NULL, nsAppStartupConstructor },
  { &kNS_USERINFO_CID, false, NULL, nsUserInfoConstructor },
#ifdef ALERTS_SERVICE
  { &kNS_ALERTSSERVICE_CID, false, NULL, nsAlertsServiceConstructor },
#endif
#if defined(XP_WIN) && !defined(MOZ_DISABLE_PARENTAL_CONTROLS)
  { &kNS_PARENTALCONTROLSSERVICE_CID, false, NULL, nsParentalControlsServiceWinConstructor },
#endif
#ifdef MOZ_RDF
  { &kNS_DOWNLOADMANAGER_CID, false, NULL, nsDownloadManagerConstructor },
  { &kNS_DOWNLOAD_CID, false, NULL, nsDownloadProxyConstructor },
#endif
  { &kNS_FIND_SERVICE_CID, false, NULL, nsFindServiceConstructor },
  { &kNS_TYPEAHEADFIND_CID, false, NULL, nsTypeAheadFindConstructor },
#ifdef MOZ_URL_CLASSIFIER
  { &kNS_URLCLASSIFIERDBSERVICE_CID, false, NULL, nsUrlClassifierDBServiceConstructor },
  { &kNS_URLCLASSIFIERSTREAMUPDATER_CID, false, NULL, nsUrlClassifierStreamUpdaterConstructor },
  { &kNS_URLCLASSIFIERUTILS_CID, false, NULL, nsUrlClassifierUtilsConstructor },
#endif
#ifdef MOZ_FEEDS
  { &kNS_SCRIPTABLEUNESCAPEHTML_CID, false, NULL, nsScriptableUnescapeHTMLConstructor },
#endif
  { &kNS_BROWSERSTATUSFILTER_CID, false, NULL, nsBrowserStatusFilterConstructor },
  { &kNS_CHARSETMENU_CID, false, NULL, NS_NewCharsetMenu },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kToolkitContracts[] = {
  { NS_APPSTARTUP_CONTRACTID, &kNS_TOOLKIT_APPSTARTUP_CID },
  { NS_USERINFO_CONTRACTID, &kNS_USERINFO_CID },
#ifdef ALERTS_SERVICE
  { NS_ALERTSERVICE_CONTRACTID, &kNS_ALERTSSERVICE_CID },
#endif
#if defined(XP_WIN) && !defined(MOZ_DISABLE_PARENTAL_CONTROLS)
  { NS_PARENTALCONTROLSSERVICE_CONTRACTID, &kNS_PARENTALCONTROLSSERVICE_CID },
#endif
#ifdef MOZ_RDF
  { NS_DOWNLOADMANAGER_CONTRACTID, &kNS_DOWNLOADMANAGER_CID },
  { NS_TRANSFER_CONTRACTID, &kNS_DOWNLOAD_CID },
#endif
  { NS_FIND_SERVICE_CONTRACTID, &kNS_FIND_SERVICE_CID },
  { NS_TYPEAHEADFIND_CONTRACTID, &kNS_TYPEAHEADFIND_CID },
#ifdef MOZ_URL_CLASSIFIER
  { NS_URLCLASSIFIERDBSERVICE_CONTRACTID, &kNS_URLCLASSIFIERDBSERVICE_CID },
  { NS_URICLASSIFIERSERVICE_CONTRACTID, &kNS_URLCLASSIFIERDBSERVICE_CID },
  { NS_URLCLASSIFIERSTREAMUPDATER_CONTRACTID, &kNS_URLCLASSIFIERSTREAMUPDATER_CID },
  { NS_URLCLASSIFIERUTILS_CONTRACTID, &kNS_URLCLASSIFIERUTILS_CID },
#endif
#ifdef MOZ_FEEDS
  { NS_SCRIPTABLEUNESCAPEHTML_CONTRACTID, &kNS_SCRIPTABLEUNESCAPEHTML_CID },
#endif
  { NS_BROWSERSTATUSFILTER_CONTRACTID, &kNS_BROWSERSTATUSFILTER_CID },
  { NS_RDF_DATASOURCE_CONTRACTID_PREFIX NS_CHARSETMENU_PID, &kNS_CHARSETMENU_CID },
  { NULL }
};

static const mozilla::Module kToolkitModule = {
  mozilla::Module::kVersion,
  kToolkitCIDs,
  kToolkitContracts
};

NSMODULE_DEFN(nsToolkitCompsModule) = &kToolkitModule;
