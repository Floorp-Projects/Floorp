/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["RemoteDebugger"];

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
var { require } = Cu.import("resource://gre/modules/devtools/shared/Loader.jsm", {});
var { DebuggerServer } = require("devtools/server/main");

this.RemoteDebugger = {
  init: function(port) {
    if (!DebuggerServer.initialized) {
      DebuggerServer.init();
      DebuggerServer.addBrowserActors("webapprt:webapp");
      DebuggerServer.addActors("chrome://webapprt/content/dbg-webapp-actors.js");
    }
    let listener = DebuggerServer.createListener();
    listener.portOrPath = port;
    listener.open();
  }
}
