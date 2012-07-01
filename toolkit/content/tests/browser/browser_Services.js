/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Tests for Services.jsm (Bug 512784) **/
  ok(Services, "Services object exists");
  checkServices();
}

function checkService(service, interface) {
  ok(service in Services, "Services." + service + " exists");
  ok(Services[service] instanceof interface, "Services." + service + " is an " + interface);
}

function checkServices() {
  checkService("prefs", Ci.nsIPrefBranch);
  checkService("prefs", Ci.nsIPrefService);
  checkService("contentPrefs", Ci.nsIContentPrefService);
  checkService("wm", Ci.nsIWindowMediator);
  checkService("obs", Ci.nsIObserverService);
  checkService("perms", Ci.nsIPermissionManager);
  checkService("io", Ci.nsIIOService);
  checkService("io", Ci.nsIIOService2);
  checkService("appinfo", Ci.nsIXULAppInfo);
  checkService("appinfo", Ci.nsIXULRuntime);
  checkService("dirsvc", Ci.nsIDirectoryService);
  checkService("dirsvc", Ci.nsIProperties);
  checkService("prompt", Ci.nsIPromptService);
  if ("nsIBrowserSearchService" in Ci)
    checkService("search", Ci.nsIBrowserSearchService);
  checkService("storage", Ci.mozIStorageService);
  checkService("vc", Ci.nsIVersionComparator);
  checkService("locale", Ci.nsILocaleService);
  checkService("scriptloader", Ci.mozIJSSubScriptLoader);
  checkService("ww", Ci.nsIWindowWatcher);
  checkService("tm", Ci.nsIThreadManager);
  checkService("droppedLinkHandler", Ci.nsIDroppedLinkHandler);
  checkService("strings", Ci.nsIStringBundleService);
  checkService("urlFormatter", Ci.nsIURLFormatter);
  checkService("eTLD", Ci.nsIEffectiveTLDService);
  checkService("cookies", Ci.nsICookieManager2);
  checkService("logins", Ci.nsILoginManager);
  checkService("telemetry", Ci.nsITelemetry);
  checkService("sysinfo", Ci.nsIPropertyBag2);
  checkService("clipboard", Ci.nsIClipboard);
  checkService("console", Ci.nsIConsoleService);
  checkService("startup", Ci.nsIAppStartup);
  checkService("appShell", Ci.nsIAppShellService);
  checkService("cache", Ci.nsICacheService);
  checkService("scriptSecurityManager", Ci.nsIScriptSecurityManager);
  checkService("domStorageManager", Ci.nsIDOMStorageManager);
  checkService("DOMRequest", Ci.nsIDOMRequestService);
  checkService("downloads", Ci.nsIDownloadManager);
  checkService("focus", Ci.nsIFocusManager);
}
