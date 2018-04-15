/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  GeckoViewUtils: "resource://gre/modules/GeckoViewUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

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
    this.modules = new Map();
  },

  add: function(aResource, aType, ...aArgs) {
    this.remove(aType);

    const scope = {};
    const global = ChromeUtils.import(aResource, scope);
    const tag = aType.replace("GeckoView", "GeckoView.");
    GeckoViewUtils.initLogging(tag, global);

    this.modules.set(aType, new scope[aType](
      aType, window, this.browser, WindowEventDispatcher, ...aArgs
    ));
  },

  remove: function(aType) {
    this.modules.delete(aType);
  },

  forEach: function(aCallback) {
    this.modules.forEach(aCallback, this);
  }
};

function createBrowser() {
  const browser = window.browser = document.createElement("browser");
  browser.setAttribute("type", "content");
  browser.setAttribute("primary", "true");
  browser.setAttribute("flex", "1");
  return browser;
}

function startup() {
  GeckoViewUtils.initLogging("GeckoView.XUL", window);

  const browser = createBrowser();
  ModuleManager.init(browser);

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

  window.document.documentElement.appendChild(browser);

  ModuleManager.forEach(module => {
    module.onInit();
    module.onSettingsUpdate();
  });

  // Move focus to the content window at the end of startup,
  // so things like text selection can work properly.
  browser.focus();
}
