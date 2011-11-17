// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
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
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Finkle <mfinkle@mozilla.com>
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

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

/**
 * Delay load some JS modules
 */
XPCOMUtils.defineLazyGetter(this, "PluralForm", function() {
  Cu.import("resource://gre/modules/PluralForm.jsm");
  return PluralForm;
});

XPCOMUtils.defineLazyGetter(this, "PlacesUtils", function() {
  Cu.import("resource://gre/modules/PlacesUtils.jsm");
  return PlacesUtils;
});

/* window.Rect is used by http://www.w3.org/TR/DOM-Level-2-Style/css.html#CSS-Rect
 * so it is not possible to set a lazy getter for Geometry.jsm
 */
Cu.import("resource://gre/modules/Geometry.jsm");

/**
 * Delay load some objects from a common script. We only load the script once,
 * into a namespace, then access each object from the namespace.
 */
XPCOMUtils.defineLazyGetter(this, "CommonUI", function() {
  let CommonUI = {};
  Services.scriptloader.loadSubScript("chrome://browser/content/common-ui.js", CommonUI);
  return CommonUI;
});

[
  ["FullScreenVideo"],
  ["BadgeHandlers"],
  ["ContextHelper"],
  ["FormHelperUI"],
  ["FindHelperUI"],
  ["NewTabPopup"],
  ["BrowserSearch"],
].forEach(function (aObject) {
  XPCOMUtils.defineLazyGetter(window, aObject, function() {
    return CommonUI[aObject];
  });
});

/**
 * Delay load some browser scripts
 */
[
  ["AlertsHelper", "chrome://browser/content/AlertsHelper.js"],
  ["AnimatedZoom", "chrome://browser/content/AnimatedZoom.js"],
  ["AppMenu", "chrome://browser/content/AppMenu.js"],
  ["AwesomePanel", "chrome://browser/content/AwesomePanel.js"],
  ["AwesomeScreen", "chrome://browser/content/AwesomePanel.js"],
  ["BookmarkHelper", "chrome://browser/content/BookmarkHelper.js"],
  ["BookmarkPopup", "chrome://browser/content/BookmarkPopup.js"],
  ["CharsetMenu", "chrome://browser/content/CharsetMenu.js"],
  ["CommandUpdater", "chrome://browser/content/commandUtil.js"],
  ["ContextCommands", "chrome://browser/content/ContextCommands.js"],
  ["ConsoleView", "chrome://browser/content/console.js"],
  ["DownloadsView", "chrome://browser/content/downloads.js"],
  ["ExtensionsView", "chrome://browser/content/extensions.js"],
  ["MenuListHelperUI", "chrome://browser/content/MenuListHelperUI.js"],
  ["OfflineApps", "chrome://browser/content/OfflineApps.js"],
  ["IndexedDB", "chrome://browser/content/IndexedDB.js"],
  ["PageActions", "chrome://browser/content/PageActions.js"],
  ["PreferencesView", "chrome://browser/content/preferences.js"],
  ["Sanitizer", "chrome://browser/content/sanitize.js"],
  ["SelectHelperUI", "chrome://browser/content/SelectHelperUI.js"],
  ["SelectionHelper", "chrome://browser/content/SelectionHelper.js"],
  ["ContentPopupHelper", "chrome://browser/content/ContentPopupHelper.js"],
  ["SharingUI", "chrome://browser/content/SharingUI.js"],
  ["TabletSidebar", "chrome://browser/content/TabletSidebar.js"],
  ["TabsPopup", "chrome://browser/content/TabsPopup.js"],
  ["MasterPasswordUI", "chrome://browser/content/MasterPasswordUI.js"],
#ifdef MOZ_SERVICES_SYNC
  ["WeaveGlue", "chrome://browser/content/sync.js"],
  ["SyncPairDevice", "chrome://browser/content/sync.js"],
#endif
  ["WebappsUI", "chrome://browser/content/WebappsUI.js"],
  ["SSLExceptions", "chrome://browser/content/exceptions.js"],
  ["CapturePickerUI", "chrome://browser/content/CapturePickerUI.js"]
].forEach(function (aScript) {
  let [name, script] = aScript;
  XPCOMUtils.defineLazyGetter(window, name, function() {
    let sandbox = {};
    Services.scriptloader.loadSubScript(script, sandbox);
    return sandbox[name];
  });
});

#ifdef MOZ_SERVICES_SYNC
XPCOMUtils.defineLazyGetter(this, "Weave", function() {
  Components.utils.import("resource://services-sync/main.js");
  return Weave;
});
#endif

/**
 * Delay load some global scripts using a custom namespace
 */
XPCOMUtils.defineLazyGetter(this, "GlobalOverlay", function() {
  let GlobalOverlay = {};
  Services.scriptloader.loadSubScript("chrome://global/content/globalOverlay.js", GlobalOverlay);
  return GlobalOverlay;
});

XPCOMUtils.defineLazyGetter(this, "ContentAreaUtils", function() {
  let ContentAreaUtils = {};
  Services.scriptloader.loadSubScript("chrome://global/content/contentAreaUtils.js", ContentAreaUtils);
  return ContentAreaUtils;
});

XPCOMUtils.defineLazyGetter(this, "ZoomManager", function() {
  let sandbox = {};
  Services.scriptloader.loadSubScript("chrome://global/content/viewZoomOverlay.js", sandbox);
  return sandbox.ZoomManager;
});

XPCOMUtils.defineLazyServiceGetter(window, "gHistSvc", "@mozilla.org/browser/nav-history-service;1", "nsINavHistoryService", "nsIBrowserHistory");
XPCOMUtils.defineLazyServiceGetter(window, "gURIFixup", "@mozilla.org/docshell/urifixup;1", "nsIURIFixup");
XPCOMUtils.defineLazyServiceGetter(window, "gFaviconService", "@mozilla.org/browser/favicon-service;1", "nsIFaviconService");
XPCOMUtils.defineLazyServiceGetter(window, "gFocusManager", "@mozilla.org/focus-manager;1", "nsIFocusManager");
