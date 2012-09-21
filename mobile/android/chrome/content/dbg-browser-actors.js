/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/**
 * Fennec-specific root actor that extends BrowserRootActor and overrides some
 * of its methods.
 */

/**
 * The function that creates the root actor. DebuggerServer expects to find this
 * function in the loaded actors in order to initialize properly.
 */
function createRootActor(aConnection) {
  return new DeviceRootActor(aConnection);
}

/**
 * Creates the root actor that client-server communications always start with.
 * The root actor is responsible for the initial 'hello' packet and for
 * responding to a 'listTabs' request that produces the list of currently open
 * tabs.
 *
 * @param aConnection DebuggerServerConnection
 *        The conection to the client.
 */
function DeviceRootActor(aConnection) {
  BrowserRootActor.call(this, aConnection);
}

DeviceRootActor.prototype = new BrowserRootActor();

/**
 * Handles the listTabs request.  Builds a list of actors
 * for the tabs running in the process.  The actors will survive
 * until at least the next listTabs request.
 */
DeviceRootActor.prototype.onListTabs = function DRA_onListTabs() {
  // Get actors for all the currently-running tabs (reusing
  // existing actors where applicable), and store them in
  // an ActorPool.

  let actorPool = new ActorPool(this.conn);
  let tabActorList = [];

  let win = windowMediator.getMostRecentWindow("navigator:browser");
  this.browser = win.BrowserApp.selectedBrowser;

  // Watch the window for tab closes so we can invalidate
  // actors as needed.
  this.watchWindow(win);

  let tabs = win.BrowserApp.tabs;
  let selected;

  for each (let tab in tabs) {
    let browser = tab.browser;

    if (browser == this.browser) {
      selected = tabActorList.length;
    }

    let actor = this._tabActors.get(browser);
    if (!actor) {
      actor = new BrowserTabActor(this.conn, browser);
      actor.parentID = this.actorID;
      this._tabActors.set(browser, actor);
    }

    actorPool.addActor(actor);
    tabActorList.push(actor);
  }

  this._createExtraActors(DebuggerServer.globalActorFactories, actorPool);

  // Now drop the old actorID -> actor map.  Actors that still
  // mattered were added to the new map, others will go
  // away.
  if (this._tabActorPool) {
    this.conn.removeActorPool(this._tabActorPool);
  }

  this._tabActorPool = actorPool;
  this.conn.addActorPool(this._tabActorPool);

  let response = {
    "from": "root",
    "selected": selected,
    "tabs": [actor.grip() for (actor of tabActorList)]
  };
  this._appendExtraActors(response);
  return response;
};

/**
 * Return the tab container for the specified window.
 */
DeviceRootActor.prototype.getTabContainer = function DRA_getTabContainer(aWindow) {
  return aWindow.document.getElementById("browsers");
};

/**
 * When a tab is closed, exit its tab actor.  The actor
 * will be dropped at the next listTabs request.
 */
DeviceRootActor.prototype.onTabClosed = function DRA_onTabClosed(aEvent) {
  this.exitTabActor(aEvent.target.browser);
};

// nsIWindowMediatorListener
DeviceRootActor.prototype.onCloseWindow = function DRA_onCloseWindow(aWindow) {
  if (aWindow.BrowserApp) {
    this.unwatchWindow(aWindow);
  }
};

/**
 * The request types this actor can handle.
 */
DeviceRootActor.prototype.requestTypes = {
  "listTabs": DeviceRootActor.prototype.onListTabs
};
