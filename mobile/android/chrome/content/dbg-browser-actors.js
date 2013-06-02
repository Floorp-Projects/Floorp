/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/**
 * Fennec-specific actors.
 */

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

MobileTabList.prototype.iterator = function() {
  // As a sanity check, make sure all the actors presently in our map get
  // picked up when we iterate over all windows' tabs.
  let initialMapSize = this._actorByBrowser.size;
  let foundCount = 0;

  // To avoid mysterious behavior if tabs are closed or opened mid-iteration,
  // we update the map first, and then make a second pass over it to yield
  // the actors. Thus, the sequence yielded is always a snapshot of the
  // actors that were live when we began the iteration.

  // Iterate over all navigator:browser XUL windows.
  for (let win of allAppShellDOMWindows("navigator:browser")) {
    let selectedTab = win.BrowserApp.selectedBrowser;

    // For each tab in this XUL window, ensure that we have an actor for
    // it, reusing existing actors where possible. We actually iterate
    // over 'browser' XUL elements, and BrowserTabActor uses
    // browser.contentWindow.wrappedJSObject as the debuggee global.
    for (let tab of win.BrowserApp.tabs) {
      let browser = tab.browser;
      // Do we have an existing actor for this browser? If not, create one.
      let actor = this._actorByBrowser.get(browser);
      if (actor) {
        foundCount++;
      } else {
        actor = new BrowserTabActor(this._connection, browser);
        this._actorByBrowser.set(browser, actor);
      }

      // Set the 'selected' properties on all actors correctly.
      actor.selected = (browser === selectedTab);
    }
  }

  if (this._testing && initialMapSize !== foundCount)
    throw Error("_actorByBrowser map contained actors for dead tabs");

  this._mustNotify = true;
  this._checkListening();

  /* Yield the values. */
  for (let [browser, actor] of this._actorByBrowser) {
    yield actor;
  }
};
