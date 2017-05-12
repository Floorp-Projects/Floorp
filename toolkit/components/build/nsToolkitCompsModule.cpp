/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsAppStartup.h"
#include "nsNetCID.h"
#include "nsUserInfo.h"
#include "nsToolkitCompsCID.h"
#include "nsFindService.h"
#if defined(MOZ_UPDATER) && !defined(MOZ_WIDGET_ANDROID)
#include "nsUpdateDriver.h"
#endif

#if !defined(MOZ_DISABLE_PARENTAL_CONTROLS)
#include "nsParentalControlsService.h"
#endif

#include "mozilla/AlertNotification.h"
#include "nsAlertsService.h"

#include "nsDownloadManager.h"
#include "DownloadPlatform.h"
#include "nsDownloadProxy.h"
#include "rdf.h"

#include "nsTypeAheadFind.h"

#include "ApplicationReputation.h"
#include "nsUrlClassifierDBService.h"
#include "nsUrlClassifierStreamUpdater.h"
#include "nsUrlClassifierUtils.h"
#include "nsUrlClassifierPrefixSet.h"

#include "nsBrowserStatusFilter.h"
#include "mozilla/FinalizationWitnessService.h"
#include "mozilla/NativeOSFileInternals.h"
#include "mozilla/AddonContentPolicy.h"
#include "mozilla/AddonManagerStartup.h"
#include "mozilla/AddonPathService.h"

#if defined(XP_WIN)
#include "NativeFileWatcherWin.h"
#else
#include "NativeFileWatcherNotSupported.h"
#endif // (XP_WIN)

#include "nsWebRequestListener.h"

#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
#define MOZ_HAS_TERMINATOR
#endif

#if defined(MOZ_HAS_TERMINATOR)
#include "nsTerminator.h"
#endif

#define MOZ_HAS_PERFSTATS

#if defined(MOZ_HAS_PERFSTATS)
#include "nsPerformanceStats.h"
#endif // defined (MOZ_HAS_PERFSTATS)

using namespace mozilla;

/////////////////////////////////////////////////////////////////////////////

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsAppStartup, Init)

#if defined(MOZ_HAS_PERFSTATS)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPerformanceStatsService, Init)
#endif // defined (MOZ_HAS_PERFSTATS)

#if defined(MOZ_HAS_TERMINATOR)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTerminator)
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(nsUserInfo)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFindService)

#if !defined(MOZ_DISABLE_PARENTAL_CONTROLS)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsParentalControlsService)
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(AlertNotification)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAlertsService)

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsDownloadManager,
                                         nsDownloadManager::GetSingleton)
NS_GENERIC_FACTORY_CONSTRUCTOR(DownloadPlatform)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDownloadProxy)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsTypeAheadFind)

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(ApplicationReputationService,
                                         ApplicationReputationService::GetSingleton)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUrlClassifierPrefixSet)
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
    if (nullptr == inst) {
        return rv;
    }
    /* NS_ADDREF(inst); */
    rv = inst->QueryInterface(aIID, aResult);
    NS_RELEASE(inst);

    return rv;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsBrowserStatusFilter)
#if defined(MOZ_UPDATER) && !defined(MOZ_WIDGET_ANDROID)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUpdateProcessor)
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(FinalizationWitnessService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(NativeOSFileInternalsService)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(NativeFileWatcherService, Init)

NS_GENERIC_FACTORY_CONSTRUCTOR(AddonContentPolicy)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(AddonPathService, AddonPathService::GetInstance)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(AddonManagerStartup, AddonManagerStartup::GetInstance)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsWebRequestListener)

NS_DEFINE_NAMED_CID(NS_TOOLKIT_APPSTARTUP_CID);
#if defined(MOZ_HAS_PERFSTATS)
NS_DEFINE_NAMED_CID(NS_TOOLKIT_PERFORMANCESTATSSERVICE_CID);
#endif // defined (MOZ_HAS_PERFSTATS)

#if defined(MOZ_HAS_TERMINATOR)
NS_DEFINE_NAMED_CID(NS_TOOLKIT_TERMINATOR_CID);
#endif
NS_DEFINE_NAMED_CID(NS_USERINFO_CID);
NS_DEFINE_NAMED_CID(ALERT_NOTIFICATION_CID);
NS_DEFINE_NAMED_CID(NS_ALERTSSERVICE_CID);
#if !defined(MOZ_DISABLE_PARENTAL_CONTROLS)
NS_DEFINE_NAMED_CID(NS_PARENTALCONTROLSSERVICE_CID);
#endif
NS_DEFINE_NAMED_CID(NS_DOWNLOADMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_DOWNLOADPLATFORM_CID);
NS_DEFINE_NAMED_CID(NS_DOWNLOAD_CID);
NS_DEFINE_NAMED_CID(NS_FIND_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_TYPEAHEADFIND_CID);
NS_DEFINE_NAMED_CID(NS_APPLICATION_REPUTATION_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_URLCLASSIFIERPREFIXSET_CID);
NS_DEFINE_NAMED_CID(NS_URLCLASSIFIERDBSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_URLCLASSIFIERSTREAMUPDATER_CID);
NS_DEFINE_NAMED_CID(NS_URLCLASSIFIERUTILS_CID);
NS_DEFINE_NAMED_CID(NS_BROWSERSTATUSFILTER_CID);
#if defined(MOZ_UPDATER) && !defined(MOZ_WIDGET_ANDROID)
NS_DEFINE_NAMED_CID(NS_UPDATEPROCESSOR_CID);
#endif
NS_DEFINE_NAMED_CID(FINALIZATIONWITNESSSERVICE_CID);
NS_DEFINE_NAMED_CID(NATIVE_OSFILE_INTERNALS_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_ADDONCONTENTPOLICY_CID);
NS_DEFINE_NAMED_CID(NS_ADDON_PATH_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_ADDON_MANAGER_STARTUP_CID);
NS_DEFINE_NAMED_CID(NATIVE_FILEWATCHER_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_WEBREQUESTLISTENER_CID);

static const Module::CIDEntry kToolkitCIDs[] = {
  { &kNS_TOOLKIT_APPSTARTUP_CID, false, nullptr, nsAppStartupConstructor },
#if defined(MOZ_HAS_TERMINATOR)
  { &kNS_TOOLKIT_TERMINATOR_CID, false, nullptr, nsTerminatorConstructor },
#endif
#if defined(MOZ_HAS_PERFSTATS)
  { &kNS_TOOLKIT_PERFORMANCESTATSSERVICE_CID, false, nullptr, nsPerformanceStatsServiceConstructor },
#endif // defined (MOZ_HAS_PERFSTATS)
  { &kNS_USERINFO_CID, false, nullptr, nsUserInfoConstructor },
  { &kALERT_NOTIFICATION_CID, false, nullptr, AlertNotificationConstructor },
  { &kNS_ALERTSSERVICE_CID, false, nullptr, nsAlertsServiceConstructor },
#if !defined(MOZ_DISABLE_PARENTAL_CONTROLS)
  { &kNS_PARENTALCONTROLSSERVICE_CID, false, nullptr, nsParentalControlsServiceConstructor },
#endif
  { &kNS_DOWNLOADMANAGER_CID, false, nullptr, nsDownloadManagerConstructor },
  { &kNS_DOWNLOADPLATFORM_CID, false, nullptr, DownloadPlatformConstructor },
  { &kNS_DOWNLOAD_CID, false, nullptr, nsDownloadProxyConstructor },
  { &kNS_FIND_SERVICE_CID, false, nullptr, nsFindServiceConstructor },
  { &kNS_TYPEAHEADFIND_CID, false, nullptr, nsTypeAheadFindConstructor },
  { &kNS_APPLICATION_REPUTATION_SERVICE_CID, false, nullptr, ApplicationReputationServiceConstructor },
  { &kNS_URLCLASSIFIERPREFIXSET_CID, false, nullptr, nsUrlClassifierPrefixSetConstructor },
  { &kNS_URLCLASSIFIERDBSERVICE_CID, false, nullptr, nsUrlClassifierDBServiceConstructor },
  { &kNS_URLCLASSIFIERSTREAMUPDATER_CID, false, nullptr, nsUrlClassifierStreamUpdaterConstructor },
  { &kNS_URLCLASSIFIERUTILS_CID, false, nullptr, nsUrlClassifierUtilsConstructor },
  { &kNS_BROWSERSTATUSFILTER_CID, false, nullptr, nsBrowserStatusFilterConstructor },
#if defined(MOZ_UPDATER) && !defined(MOZ_WIDGET_ANDROID)
  { &kNS_UPDATEPROCESSOR_CID, false, nullptr, nsUpdateProcessorConstructor },
#endif
  { &kFINALIZATIONWITNESSSERVICE_CID, false, nullptr, FinalizationWitnessServiceConstructor },
  { &kNATIVE_OSFILE_INTERNALS_SERVICE_CID, false, nullptr, NativeOSFileInternalsServiceConstructor },
  { &kNS_ADDONCONTENTPOLICY_CID, false, nullptr, AddonContentPolicyConstructor },
  { &kNS_ADDON_PATH_SERVICE_CID, false, nullptr, AddonPathServiceConstructor },
  { &kNS_ADDON_MANAGER_STARTUP_CID, false, nullptr, AddonManagerStartupConstructor },
  { &kNATIVE_FILEWATCHER_SERVICE_CID, false, nullptr, NativeFileWatcherServiceConstructor },
  { &kNS_WEBREQUESTLISTENER_CID, false, nullptr, nsWebRequestListenerConstructor },
  { nullptr }
};

static const Module::ContractIDEntry kToolkitContracts[] = {
  { NS_APPSTARTUP_CONTRACTID, &kNS_TOOLKIT_APPSTARTUP_CID },
#if defined(MOZ_HAS_TERMINATOR)
  { NS_TOOLKIT_TERMINATOR_CONTRACTID, &kNS_TOOLKIT_TERMINATOR_CID },
#endif
#if defined(MOZ_HAS_PERFSTATS)
  { NS_TOOLKIT_PERFORMANCESTATSSERVICE_CONTRACTID, &kNS_TOOLKIT_PERFORMANCESTATSSERVICE_CID },
#endif // defined (MOZ_HAS_PERFSTATS)
  { NS_USERINFO_CONTRACTID, &kNS_USERINFO_CID },
  { ALERT_NOTIFICATION_CONTRACTID, &kALERT_NOTIFICATION_CID },
  { NS_ALERTSERVICE_CONTRACTID, &kNS_ALERTSSERVICE_CID },
#if !defined(MOZ_DISABLE_PARENTAL_CONTROLS)
  { NS_PARENTALCONTROLSSERVICE_CONTRACTID, &kNS_PARENTALCONTROLSSERVICE_CID },
#endif
  { NS_DOWNLOADMANAGER_CONTRACTID, &kNS_DOWNLOADMANAGER_CID },
  { NS_DOWNLOADPLATFORM_CONTRACTID, &kNS_DOWNLOADPLATFORM_CID },
  { NS_FIND_SERVICE_CONTRACTID, &kNS_FIND_SERVICE_CID },
  { NS_TYPEAHEADFIND_CONTRACTID, &kNS_TYPEAHEADFIND_CID },
  { NS_APPLICATION_REPUTATION_SERVICE_CONTRACTID, &kNS_APPLICATION_REPUTATION_SERVICE_CID },
  { NS_URLCLASSIFIERPREFIXSET_CONTRACTID, &kNS_URLCLASSIFIERPREFIXSET_CID },
  { NS_URLCLASSIFIERDBSERVICE_CONTRACTID, &kNS_URLCLASSIFIERDBSERVICE_CID },
  { NS_URICLASSIFIERSERVICE_CONTRACTID, &kNS_URLCLASSIFIERDBSERVICE_CID },
  { NS_URLCLASSIFIERSTREAMUPDATER_CONTRACTID, &kNS_URLCLASSIFIERSTREAMUPDATER_CID },
  { NS_URLCLASSIFIERUTILS_CONTRACTID, &kNS_URLCLASSIFIERUTILS_CID },
  { NS_BROWSERSTATUSFILTER_CONTRACTID, &kNS_BROWSERSTATUSFILTER_CID },
#if defined(MOZ_UPDATER) && !defined(MOZ_WIDGET_ANDROID)
  { NS_UPDATEPROCESSOR_CONTRACTID, &kNS_UPDATEPROCESSOR_CID },
#endif
  { FINALIZATIONWITNESSSERVICE_CONTRACTID, &kFINALIZATIONWITNESSSERVICE_CID },
  { NATIVE_OSFILE_INTERNALS_SERVICE_CONTRACTID, &kNATIVE_OSFILE_INTERNALS_SERVICE_CID },
  { NS_ADDONCONTENTPOLICY_CONTRACTID, &kNS_ADDONCONTENTPOLICY_CID },
  { NS_ADDONPATHSERVICE_CONTRACTID, &kNS_ADDON_PATH_SERVICE_CID },
  { NS_ADDONMANAGERSTARTUP_CONTRACTID, &kNS_ADDON_MANAGER_STARTUP_CID },
  { NATIVE_FILEWATCHER_SERVICE_CONTRACTID, &kNATIVE_FILEWATCHER_SERVICE_CID },
  { NS_WEBREQUESTLISTENER_CONTRACTID, &kNS_WEBREQUESTLISTENER_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kToolkitCategories[] = {
  { "content-policy", NS_ADDONCONTENTPOLICY_CONTRACTID, NS_ADDONCONTENTPOLICY_CONTRACTID },
  { nullptr }
};

static const Module kToolkitModule = {
  Module::kVersion,
  kToolkitCIDs,
  kToolkitContracts,
  kToolkitCategories
};

NSMODULE_DEFN(nsToolkitCompsModule) = &kToolkitModule;
