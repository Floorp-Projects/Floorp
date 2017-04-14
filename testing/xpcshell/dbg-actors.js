/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { Promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
var { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
const { devtools } = Cu.import("resource://devtools/shared/Loader.jsm", {});
const { RootActor } = devtools.require("devtools/server/actors/root");
const { BrowserTabList } = devtools.require("devtools/server/actors/webbrowser");

/**
 * xpcshell-test (XPCST) specific actors.
 *
 */

/**
 * Construct a root actor appropriate for use in a server running xpcshell
 * tests. <snip boilerplate> :)
 */
function createRootActor(connection)
{
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

/**
 * A "stub" TabList implementation that provides no tabs.
 */

function XPCSTTabList(connection)
{
  BrowserTabList.call(this, connection);
}

XPCSTTabList.prototype = Object.create(BrowserTabList.prototype);

XPCSTTabList.prototype.constructor = XPCSTTabList;

XPCSTTabList.prototype.getList = function() {
  return Promise.resolve([]);
};
