/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
 "resource://gre/modules/Services.jsm", "Services");

XPCOMUtils.defineLazyModuleGetter(this, "EventDispatcher",
  "resource://gre/modules/Messaging.jsm");
XPCOMUtils.defineLazyGetter(this, "GlobalEventDispatcher",
  () => EventDispatcher.instance);
XPCOMUtils.defineLazyGetter(this, "WindowEventDispatcher",
  () => EventDispatcher.for(window));

var dump = Cu.import("resource://gre/modules/AndroidLog.jsm", {})
           .AndroidLog.d.bind(null, "View");

// GeckoView module imports.
XPCOMUtils.defineLazyModuleGetter(this, "GeckoViewContent",
  "resource://gre/modules/GeckoViewContent.jsm");

var content;

function startup() {
  dump("zerdatime " + Date.now() + " - geckoview chrome startup finished.");

  content = new GeckoViewContent(window, document.getElementById("content"), WindowEventDispatcher);
}
