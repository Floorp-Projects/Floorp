/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "EventDispatcher",
  "resource://gre/modules/Messaging.jsm");
ChromeUtils.defineModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyGetter(this, "WindowEventDispatcher",
  () => EventDispatcher.for(window));

XPCOMUtils.defineLazyGetter(this, "dump", () =>
    ChromeUtils.import("resource://gre/modules/AndroidLog.jsm",
                       {}).AndroidLog.d.bind(null, "View"));

// Creates and manages GeckoView modules.
// A module must extend GeckoViewModule.
// Instantiate a module by calling
//   add(<resource path>, <type name>)
// and remove by calling
//   remove(<type name>)
var ModuleManager = {
  init: function(aBrowser) {
    this.browser = aBrowser;
    this.modules = {};
  },

  add: function(aResource, aType, ...aArgs) {
    this.remove(aType);
    let scope = {};
    ChromeUtils.import(aResource, scope);
    this.modules[aType] = new scope[aType](
      aType, window, this.browser, WindowEventDispatcher, ...aArgs
    );
  },

  remove: function(aType) {
    if (!(aType in this.modules)) {
      return;
    }
    delete this.modules[aType];
  }
};

function createBrowser() {
  const browser = window.browser = document.createElement("browser");
  browser.setAttribute("type", "content");
  browser.setAttribute("primary", "true");
  browser.setAttribute("flex", "1");

  // There may be a GeckoViewNavigation module in another window waiting for us to
  // create a browser so it can call presetOpenerWindow(), so allow them to do that now.
  Services.obs.notifyObservers(window, "geckoview-window-created");
  window.document.getElementById("main-window").appendChild(browser);

  browser.stop();
  return browser;
}

function startup() {
  const browser = createBrowser();
  ModuleManager.init(browser);

  // GeckoViewNavigation needs to go first because nsIDOMBrowserWindow must set up
  // before the first remote browser. Bug 1365364.
  ModuleManager.add("resource://gre/modules/GeckoViewNavigation.jsm",
                    "GeckoViewNavigation");
  ModuleManager.add("resource://gre/modules/GeckoViewSettings.jsm",
                    "GeckoViewSettings");
  ModuleManager.add("resource://gre/modules/GeckoViewContent.jsm",
                    "GeckoViewContent");
  ModuleManager.add("resource://gre/modules/GeckoViewProgress.jsm",
                    "GeckoViewProgress");
  ModuleManager.add("resource://gre/modules/GeckoViewScroll.jsm",
                    "GeckoViewScroll");
  ModuleManager.add("resource://gre/modules/GeckoViewTab.jsm",
                    "GeckoViewTab");
  ModuleManager.add("resource://gre/modules/GeckoViewRemoteDebugger.jsm",
                    "GeckoViewRemoteDebugger");
  ModuleManager.add("resource://gre/modules/GeckoViewTrackingProtection.jsm",
                    "GeckoViewTrackingProtection");
  ModuleManager.add("resource://gre/modules/GeckoViewSelectionAction.jsm",
                    "GeckoViewSelectionAction");
  ModuleManager.add("resource://gre/modules/GeckoViewAccessibility.jsm",
                    "GeckoViewAccessibility");

  // Move focus to the content window at the end of startup,
  // so things like text selection can work properly.
  browser.focus();
}
