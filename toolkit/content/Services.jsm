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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gavin Sharp <gavin@gavinsharp.com> (original author)
 *   Justin Dolske <dolske@mozilla.com>
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

let EXPORTED_SYMBOLS = ["Services"];

const Ci = Components.interfaces;
const Cc = Components.classes;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

let Services = {};

XPCOMUtils.defineLazyGetter(Services, "prefs", function () {
  return Cc["@mozilla.org/preferences-service;1"]
           .getService(Ci.nsIPrefService)
           .QueryInterface(Ci.nsIPrefBranch2);
});

XPCOMUtils.defineLazyGetter(Services, "appinfo", function () {
  return Cc["@mozilla.org/xre/app-info;1"]
           .getService(Ci.nsIXULAppInfo)
           .QueryInterface(Ci.nsIXULRuntime);
});

XPCOMUtils.defineLazyGetter(Services, "dirsvc", function () {
  return Cc["@mozilla.org/file/directory_service;1"]
           .getService(Ci.nsIDirectoryService)
           .QueryInterface(Ci.nsIProperties);
});

let initTable = [
  ["console", "@mozilla.org/consoleservice;1", "nsIConsoleService"],
  ["contentPrefs", "@mozilla.org/content-pref/service;1", "nsIContentPrefService"],
  ["cookies", "@mozilla.org/cookiemanager;1", "nsICookieManager2"],
  ["droppedLinkHandler", "@mozilla.org/content/dropped-link-handler;1", "nsIDroppedLinkHandler"],
  ["eTLD", "@mozilla.org/network/effective-tld-service;1", "nsIEffectiveTLDService"],
  ["io", "@mozilla.org/network/io-service;1", "nsIIOService2"],
  ["locale", "@mozilla.org/intl/nslocaleservice;1", "nsILocaleService"],
  ["logins", "@mozilla.org/login-manager;1", "nsILoginManager"],
  ["obs", "@mozilla.org/observer-service;1", "nsIObserverService"],
  ["perms", "@mozilla.org/permissionmanager;1", "nsIPermissionManager"],
  ["prompt", "@mozilla.org/embedcomp/prompt-service;1", "nsIPromptService"],
  ["scriptloader", "@mozilla.org/moz/jssubscript-loader;1", "mozIJSSubScriptLoader"],
#ifdef MOZ_TOOLKIT_SEARCH
  ["search", "@mozilla.org/browser/search-service;1", "nsIBrowserSearchService"],
#endif
  ["storage", "@mozilla.org/storage/service;1", "mozIStorageService"],
  ["strings", "@mozilla.org/intl/stringbundle;1", "nsIStringBundleService"],
  ["telemetry", "@mozilla.org/base/telemetry;1", "nsITelemetry"],
  ["tm", "@mozilla.org/thread-manager;1", "nsIThreadManager"],
  ["urlFormatter", "@mozilla.org/toolkit/URLFormatterService;1", "nsIURLFormatter"],
  ["vc", "@mozilla.org/xpcom/version-comparator;1", "nsIVersionComparator"],
  ["wm", "@mozilla.org/appshell/window-mediator;1", "nsIWindowMediator"],
  ["ww", "@mozilla.org/embedcomp/window-watcher;1", "nsIWindowWatcher"],
  ["startup", "@mozilla.org/toolkit/app-startup;1", "nsIAppStartup"],
  ["sysinfo", "@mozilla.org/system-info;1", "nsIPropertyBag2"],
  ["clipboard", "@mozilla.org/widget/clipboard;1", "nsIClipboard"]
];

initTable.forEach(function ([name, contract, intf])
  XPCOMUtils.defineLazyServiceGetter(Services, name, contract, intf));

initTable = undefined;
