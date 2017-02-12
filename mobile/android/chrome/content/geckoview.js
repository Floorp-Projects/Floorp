/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "EventDispatcher",
  "resource://gre/modules/Messaging.jsm");
XPCOMUtils.defineLazyGetter(this, "WindowEventDispatcher",
  () => EventDispatcher.for(window));

var dump = Cu.import("resource://gre/modules/AndroidLog.jsm", {})
           .AndroidLog.d.bind(null, "View");

// Creates and manages GeckoView modules.
// A module must extend GeckoViewModule.
// Instantiate a module by calling
//   add(<resource path>, <type name>)
// and remove by calling
//   remove(<type name>)
var ModuleManager = {
  init: function() {
    this.browser = document.getElementById("content");
    this.modules = {};
  },

  add: function(resource, type, ...args) {
    this.remove(type);
    let scope = {};
    Cu.import(resource, scope);
    this.modules[type] = new scope[type](
      window, this.browser, WindowEventDispatcher, ...args);
  },

  remove: function(type) {
    if (!(type in this.modules)) {
      return;
    }
    delete this.modules[type];
  }
};

function startup() {
  ModuleManager.init();
  ModuleManager.add("resource://gre/modules/GeckoViewContent.jsm",
                    "GeckoViewContent");
  ModuleManager.add("resource://gre/modules/GeckoViewNavigation.jsm",
                    "GeckoViewNavigation");
  ModuleManager.add("resource://gre/modules/GeckoViewProgress.jsm",
                    "GeckoViewProgress");

  dump("zerdatime " + Date.now() + " - geckoview chrome startup finished.");
}
