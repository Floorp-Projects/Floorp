/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/**
 * Fennec-specific actors.
 */

const { RootActor } = require("devtools/server/actors/root");
const { DebuggerServer } = require("devtools/server/main");
const { BrowserTabList, BrowserAddonList, sendShutdownEvent } =
  require("devtools/server/actors/webbrowser");

/**
 * Construct a root actor appropriate for use in a server running in a
 * browser on Android. The returned root actor:
 * - respects the factories registered with DebuggerServer.addGlobalActor,
 * - uses a MobileTabList to supply tab actors,
 * - sends all navigator:browser window documents a Debugger:Shutdown event
 *   when it exits.
 *
 * * @param aConnection DebuggerServerConnection
 *        The conection to the client.
 */
function createRootActor(aConnection)
{
  let parameters = {
    tabList: new MobileTabList(aConnection),
    addonList: new BrowserAddonList(aConnection),
    globalActorFactories: DebuggerServer.globalActorFactories,
    onShutdown: sendShutdownEvent
  };
  return new RootActor(aConnection, parameters);
}

/**
 * A live list of BrowserTabActors representing the current browser tabs,
 * to be provided to the root actor to answer 'listTabs' requests.
 *
 * This object also takes care of listening for TabClose events and
 * onCloseWindow notifications, and exiting the BrowserTabActors concerned.
 *
 * (See the documentation for RootActor for the definition of the "live
 * list" interface.)
 *
 * @param aConnection DebuggerServerConnection
 *     The connection in which this list's tab actors may participate.
 *
 * @see BrowserTabList for more a extensive description of how tab list objects
 *      work.
 */
function MobileTabList(aConnection)
{
  BrowserTabList.call(this, aConnection);
}

MobileTabList.prototype = Object.create(BrowserTabList.prototype);

MobileTabList.prototype.constructor = MobileTabList;

MobileTabList.prototype._getSelectedBrowser = function(aWindow) {
  return aWindow.BrowserApp.selectedBrowser;
};

MobileTabList.prototype._getChildren = function(aWindow) {
  return aWindow.BrowserApp.tabs.map(tab => tab.browser);
};

exports.register = function(handle) {
  handle.setRootActor(createRootActor);
};

exports.unregister = function(handle) {
  handle.setRootActor(null);
};
