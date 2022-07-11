/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests the Services global variable.
 */

// Globals

function checkService(service, interface) {
  info("Checking that Services." + service + " is an " + interface);
  Assert.ok(service in Services);
  Assert.ok(Services[service] instanceof interface);
}

// Tests

function run_test() {
  do_get_profile();

  checkService("appShell", Ci.nsIAppShellService);
  checkService("appinfo", Ci.nsIXULRuntime);
  checkService("cache2", Ci.nsICacheStorageService);
  checkService("clipboard", Ci.nsIClipboard);
  checkService("console", Ci.nsIConsoleService);
  checkService("cookies", Ci.nsICookieManager);
  checkService("dirsvc", Ci.nsIDirectoryService);
  checkService("dirsvc", Ci.nsIProperties);
  checkService("DOMRequest", Ci.nsIDOMRequestService);
  checkService("domStorageManager", Ci.nsIDOMStorageManager);
  checkService("droppedLinkHandler", Ci.nsIDroppedLinkHandler);
  checkService("eTLD", Ci.nsIEffectiveTLDService);
  checkService("focus", Ci.nsIFocusManager);
  checkService("io", Ci.nsIIOService);
  checkService("intl", Ci.mozIMozIntl);
  checkService("locale", Ci.mozILocaleService);
  checkService("logins", Ci.nsILoginManager);
  checkService("obs", Ci.nsIObserverService);
  checkService("perms", Ci.nsIPermissionManager);
  checkService("prefs", Ci.nsIPrefBranch);
  checkService("prefs", Ci.nsIPrefService);
  checkService("prompt", Ci.nsIPromptService);
  checkService("scriptSecurityManager", Ci.nsIScriptSecurityManager);
  checkService("scriptloader", Ci.mozIJSSubScriptLoader);
  checkService("startup", Ci.nsIAppStartup);
  checkService("storage", Ci.mozIStorageService);
  checkService("strings", Ci.nsIStringBundleService);
  checkService("sysinfo", Ci.nsIPropertyBag2);
  checkService("telemetry", Ci.nsITelemetry);
  checkService("tm", Ci.nsIThreadManager);
  checkService("urlFormatter", Ci.nsIURLFormatter);
  checkService("vc", Ci.nsIVersionComparator);
  checkService("wm", Ci.nsIWindowMediator);
  checkService("ww", Ci.nsIWindowWatcher);
  if ("nsISearchService" in Ci) {
    checkService("search", Ci.nsISearchService);
  }
  if ("nsIAndroidBridge" in Ci) {
    checkService("androidBridge", Ci.nsIAndroidBridge);
  }
  if ("@mozilla.org/enterprisepolicies;1" in Cc) {
    checkService("policies", Ci.nsIEnterprisePolicies);
  }
}
