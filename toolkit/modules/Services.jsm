/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint mozilla/use-services:off */

var EXPORTED_SYMBOLS = ["Services"];

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

var Services = {};

/**
 * WARNING: If you add a getter that isn't in the initTable, please update the
 * eslint rule in /tools/lint/eslint/eslint-plugin-mozilla/lib/rules/use-services.js
 */

XPCOMUtils.defineLazyGetter(Services, "prefs", function() {
  return Cc["@mozilla.org/preferences-service;1"]
           .getService(Ci.nsIPrefService)
           .QueryInterface(Ci.nsIPrefBranch);
});

XPCOMUtils.defineLazyGetter(Services, "appinfo", function() {
  let appinfo = Cc["@mozilla.org/xre/app-info;1"]
                  .getService(Ci.nsIXULRuntime);
  try {
    appinfo.QueryInterface(Ci.nsIXULAppInfo);
  } catch (ex) {
    // Not all applications implement nsIXULAppInfo (e.g. xpcshell doesn't).
    if (!(ex instanceof Components.Exception) || ex.result != Cr.NS_NOINTERFACE) {
      throw ex;
    }
  }
  return appinfo;
});

XPCOMUtils.defineLazyGetter(Services, "dirsvc", function() {
  return Cc["@mozilla.org/file/directory_service;1"]
           .getService(Ci.nsIDirectoryService)
           .QueryInterface(Ci.nsIProperties);
});

if (AppConstants.MOZ_CRASHREPORTER) {
  XPCOMUtils.defineLazyGetter(Services, "crashmanager", () => {
    let ns = {};
    ChromeUtils.import("resource://gre/modules/CrashManager.jsm", ns);

    return ns.CrashManager.Singleton;
  });
}

XPCOMUtils.defineLazyGetter(Services, "io", () => {
  return Cc["@mozilla.org/network/io-service;1"]
           .getService(Ci.nsIIOService)
           .QueryInterface(Ci.nsISpeculativeConnect);
});

var initTable = {
  appShell: ["@mozilla.org/appshell/appShellService;1", "nsIAppShellService"],
  cache2: ["@mozilla.org/netwerk/cache-storage-service;1", "nsICacheStorageService"],
  clearData: ["@mozilla.org/clear-data-service;1", "nsIClearDataService"],
  cpmm: ["@mozilla.org/childprocessmessagemanager;1", "nsIMessageSender"],
  console: ["@mozilla.org/consoleservice;1", "nsIConsoleService"],
  cookies: ["@mozilla.org/cookiemanager;1", "nsICookieManager"],
  droppedLinkHandler: ["@mozilla.org/content/dropped-link-handler;1", "nsIDroppedLinkHandler"],
  els: ["@mozilla.org/eventlistenerservice;1", "nsIEventListenerService"],
  eTLD: ["@mozilla.org/network/effective-tld-service;1", "nsIEffectiveTLDService"],
  intl: ["@mozilla.org/mozintl;1", "mozIMozIntl"],
  locale: ["@mozilla.org/intl/localeservice;1", "mozILocaleService"],
  logins: ["@mozilla.org/login-manager;1", "nsILoginManager"],
  mm: ["@mozilla.org/globalmessagemanager;1", "nsISupports"],
  obs: ["@mozilla.org/observer-service;1", "nsIObserverService"],
  perms: ["@mozilla.org/permissionmanager;1", "nsIPermissionManager"],
  ppmm: ["@mozilla.org/parentprocessmessagemanager;1", "nsISupports"],
  prompt: ["@mozilla.org/embedcomp/prompt-service;1", "nsIPromptService"],
  scriptloader: ["@mozilla.org/moz/jssubscript-loader;1", "mozIJSSubScriptLoader"],
  scriptSecurityManager: ["@mozilla.org/scriptsecuritymanager;1", "nsIScriptSecurityManager"],
  storage: ["@mozilla.org/storage/service;1", "mozIStorageService"],
  domStorageManager: ["@mozilla.org/dom/localStorage-manager;1", "nsIDOMStorageManager"],
  strings: ["@mozilla.org/intl/stringbundle;1", "nsIStringBundleService"],
  telemetry: ["@mozilla.org/base/telemetry;1", "nsITelemetry"],
  textToSubURI: ["@mozilla.org/intl/texttosuburi;1", "nsITextToSubURI"],
  tm: ["@mozilla.org/thread-manager;1", "nsIThreadManager"],
  urlFormatter: ["@mozilla.org/toolkit/URLFormatterService;1", "nsIURLFormatter"],
  vc: ["@mozilla.org/xpcom/version-comparator;1", "nsIVersionComparator"],
  wm: ["@mozilla.org/appshell/window-mediator;1", "nsIWindowMediator"],
  ww: ["@mozilla.org/embedcomp/window-watcher;1", "nsIWindowWatcher"],
  startup: ["@mozilla.org/toolkit/app-startup;1", "nsIAppStartup"],
  sysinfo: ["@mozilla.org/system-info;1", "nsIPropertyBag2"],
  clipboard: ["@mozilla.org/widget/clipboard;1", "nsIClipboard"],
  DOMRequest: ["@mozilla.org/dom/dom-request-service;1", "nsIDOMRequestService"],
  focus: ["@mozilla.org/focus-manager;1", "nsIFocusManager"],
  uriFixup: ["@mozilla.org/docshell/urifixup;1", "nsIURIFixup"],
  blocklist: ["@mozilla.org/extensions/blocklist;1"],
  netUtils: ["@mozilla.org/network/util;1", "nsINetUtil"],
  loadContextInfo: ["@mozilla.org/load-context-info-factory;1", "nsILoadContextInfoFactory"],
  qms: ["@mozilla.org/dom/quota-manager-service;1", "nsIQuotaManagerService"],
  xulStore: ["@mozilla.org/xul/xulstore;1", "nsIXULStore"],
};

if (AppConstants.platform == "android") {
  initTable.androidBridge = ["@mozilla.org/android/bridge;1", "nsIAndroidBridge"];
}
if (AppConstants.MOZ_GECKO_PROFILER) {
  initTable.profiler = ["@mozilla.org/tools/profiler;1", "nsIProfiler"];
}
if (AppConstants.MOZ_TOOLKIT_SEARCH) {
  initTable.search = ["@mozilla.org/browser/search-service;1", "nsIBrowserSearchService"];
}
if ("@mozilla.org/browser/enterprisepolicies;1" in Cc) {
  initTable.policies = ["@mozilla.org/browser/enterprisepolicies;1", "nsIEnterprisePolicies"];
}

XPCOMUtils.defineLazyServiceGetters(Services, initTable);

initTable = undefined;
