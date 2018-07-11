/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals require, exports */

"use strict";

const DebuggerServer = require("devtools/server/main");
const { RootActor } = require("devtools/server/actors/root");
const { BrowserTabList } = require("devtools/server/actors/webbrowser");
const Services = require("Services");

/**
 * xpcshell-test (XPCST) specific actors.
 *
 */

/**
 * Construct a root actor appropriate for use in a server running xpcshell
 * tests. <snip boilerplate> :)
 */
function createRootActor(connection) {
  let parameters = {
    tabList: new XPCSTTabList(connection),
    globalActorFactories: DebuggerServer.globalActorFactories,
    onShutdown() {
      // If the user never switches to the "debugger" tab we might get a
      // shutdown before we've attached.
      Services.obs.notifyObservers(null, "xpcshell-test-devtools-shutdown");
    }
  };
  return new RootActor(connection, parameters);
}
exports.createRootActor = createRootActor;

/**
 * A "stub" TabList implementation that provides no tabs.
 */

function XPCSTTabList(connection) {
  BrowserTabList.call(this, connection);
}

XPCSTTabList.prototype = Object.create(BrowserTabList.prototype);

XPCSTTabList.prototype.constructor = XPCSTTabList;

XPCSTTabList.prototype.getList = function() {
  return Promise.resolve([]);
};
