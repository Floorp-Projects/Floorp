/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is JS Debugger Server code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

"use strict";
/**
 * Browser-specific actors.
 */

var windowMediator = Cc["@mozilla.org/appshell/window-mediator;1"]
  .getService(Ci.nsIWindowMediator);

function createRootActor(aConnection)
{
  return new BrowserRootActor(aConnection);
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
function BrowserRootActor(aConnection)
{
  this.conn = aConnection;
  this._tabActors = new WeakMap();
  this._tabActorPool = null;
  this._actorFactories = null;

  this.onTabClosed = this.onTabClosed.bind(this);
  this._onWindowCreated = this.onWindowCreated.bind(this);
  windowMediator.addListener(this);
}

BrowserRootActor.prototype = {
  /**
   * Return a 'hello' packet as specified by the Remote Debugging Protocol.
   */
  sayHello: function BRA_sayHello() {
    // Create the tab actor for the selected tab right away so that it gets a
    // chance to listen to onNewScript notifications.
    this._preInitTabActor();

    return { from: "root",
             applicationType: "browser",
             traits: [] };
  },

  /**
   * Disconnects the actor from the browser window.
   */
  disconnect: function BRA_disconnect() {
    windowMediator.removeListener(this);

    // We may have registered event listeners on browser windows to
    // watch for tab closes, remove those.
    let e = windowMediator.getEnumerator("navigator:browser");
    while (e.hasMoreElements()) {
      let win = e.getNext();
      this.unwatchWindow(win);
      // Signal our imminent shutdown.
      let evt = win.document.createEvent("Event");
      evt.initEvent("Debugger:Shutdown", true, false);
      win.document.documentElement.dispatchEvent(evt);
    }
  },

  /**
   * Handles the listTabs request.  Builds a list of actors
   * for the tabs running in the process.  The actors will survive
   * until at least the next listTabs request.
   */
  onListTabs: function BRA_onListTabs() {
    // Get actors for all the currently-running tabs (reusing
    // existing actors where applicable), and store them in
    // an ActorPool.

    let actorPool = new ActorPool(this.conn);
    let actorList = [];

    // Walk over open browser windows.
    let e = windowMediator.getEnumerator("navigator:browser");
    let selected;
    while (e.hasMoreElements()) {
      let win = e.getNext();

      // List the tabs in this browser.
      let selectedBrowser = win.getBrowser().selectedBrowser;
      let browsers = win.getBrowser().browsers;
      for each (let browser in browsers) {
        if (browser == selectedBrowser) {
          selected = actorList.length;
        }
        let actor = this._tabActors.get(browser);
        if (!actor) {
          actor = new BrowserTabActor(this.conn, browser);
          actor.parentID = this.actorID;
          this._tabActors.set(browser, actor);
        }
        actorPool.addActor(actor);
        actorList.push(actor);
      }
    }

    // Now drop the old actorID -> actor map.  Actors that still
    // mattered were added to the new map, others will go
    // away.
    if (this._tabActorPool) {
      this.conn.removeActorPool(this._tabActorPool);
    }
    this._tabActorPool = actorPool;
    this.conn.addActorPool(this._tabActorPool);

    return { "from": "root",
             "selected": selected,
             "tabs": [actor.grip()
                      for each (actor in actorList)] };
  },

  /**
   * Watch a window that was visited during onListTabs for
   * tab closures.
   */
  watchWindow: function BRA_watchWindow(aWindow) {
    aWindow.getBrowser().tabContainer.addEventListener("TabClose",
                                                       this.onTabClosed,
                                                       false);
  },

  /**
   * Stop watching a window for tab closes.
   */
  unwatchWindow: function BRA_unwatchWindow(aWindow) {
    aWindow.getBrowser().tabContainer.removeEventListener("TabClose",
                                                          this.onTabClosed);
    this.exitTabActor(aWindow);
  },

  /**
   * When a tab is closed, exit its tab actor.  The actor
   * will be dropped at the next listTabs request.
   */
  onTabClosed: function BRA_onTabClosed(aEvent) {
    this.exitTabActor(aEvent.target.linkedBrowser);
  },

  /**
   * Handle location changes, by preinitializing a tab actor.
   */
  onWindowCreated: function BRA_onWindowCreated(evt) {
    if (evt.target === this.browser.contentDocument) {
      this._preInitTabActor();
    }
  },

  /**
   * Exit the tab actor of the specified tab.
   */
  exitTabActor: function BRA_exitTabActor(aWindow) {
    this.browser.removeEventListener("DOMWindowCreated", this._onWindowCreated, true);
    let actor = this._tabActors.get(aWindow);
    if (actor) {
      actor.exit();
    }
  },

  /**
   * Create the tab actor in the selected tab right away so that it gets a
   * chance to listen to onNewScript notifications.
   */
  _preInitTabActor: function BRA__preInitTabActor() {
    let actorPool = new ActorPool(this.conn);

    // Walk over open browser windows.
    let e = windowMediator.getEnumerator("navigator:browser");
    while (e.hasMoreElements()) {
      let win = e.getNext();

      // Watch the window for tab closes so we can invalidate
      // actors as needed.
      this.watchWindow(win);

      this.browser = win.getBrowser().selectedBrowser;
      let actor = this._tabActors.get(this.browser);
      if (actor) {
        actor._detach();
      }
      actor = new BrowserTabActor(this.conn, this.browser);
      actor.parentID = this.actorID;
      this._tabActors.set(this.browser, actor);

      actorPool.addActor(actor);
    }

    this._tabActorPool = actorPool;
    this.conn.addActorPool(this._tabActorPool);

    // Watch for globals being created in this tab.
    this.browser.addEventListener("DOMWindowCreated", this._onWindowCreated, true);
  },

  // nsIWindowMediatorListener
  onWindowTitleChange: function BRA_onWindowTitleChange(aWindow, aTitle) { },
  onOpenWindow: function BRA_onOpenWindow(aWindow) { },
  onCloseWindow: function BRA_onCloseWindow(aWindow) {
    if (aWindow.getBrowser) {
      this.unwatchWindow(aWindow);
    }
  }
}

/**
 * The request types this actor can handle.
 */
BrowserRootActor.prototype.requestTypes = {
  "listTabs": BrowserRootActor.prototype.onListTabs
};

/**
 * Creates a tab actor for handling requests to a browser tab, like attaching
 * and detaching.
 *
 * @param aConnection DebuggerServerConnection
 *        The conection to the client.
 * @param aBrowser browser
 *        The browser instance that contains this tab.
 */
function BrowserTabActor(aConnection, aBrowser)
{
  this.conn = aConnection;
  this._browser = aBrowser;

  this._onWindowCreated = this.onWindowCreated.bind(this);
  this._attach();
}

// XXX (bug 710213): BrowserTabActor attach/detach/exit/disconnect is a
// *complete* mess, needs to be rethought asap.

BrowserTabActor.prototype = {
  get browser() { return this._browser; },

  get exited() { return !this.browser; },
  get attached() { return !!this._attached },

  _tabPool: null,
  get tabActorPool() { return this._tabPool; },

  _contextPool: null,
  get contextActorPool() { return this._contextPool; },

  actorPrefix: "tab",

  grip: function BTA_grip() {
    dbg_assert(!this.exited,
               "grip() shouldn't be called on exited browser actor.");
    dbg_assert(this.actorID,
               "tab should have an actorID.");
    return { actor: this.actorID,
             title: this.browser.contentTitle,
             url: this.browser.currentURI.spec }
  },

  /**
   * Called when the actor is removed from the connection.
   */
  disconnect: function BTA_disconnect() {
    this._detach();
  },

  /**
   * Called by the root actor when the underlying tab is closed.
   */
  exit: function BTA_exit() {
    if (this.exited) {
      return;
    }

    if (this.attached) {
      this._detach();
      this.conn.send({ from: this.actorID,
                       type: "tabDetached" });
    }

    this._browser = null;
  },

  /**
   * Does the actual work of attching to a tab.
   */
  _attach: function BTA_attach() {
    if (this._attached) {
      return;
    }

    // Create a pool for tab-lifetime actors.
    dbg_assert(!this._tabPool, "Shouldn't have a tab pool if we weren't attached.");
    this._tabPool = new ActorPool(this.conn);
    this.conn.addActorPool(this._tabPool);

    // ... and a pool for context-lifetime actors.
    this._pushContext();

    // Watch for globals being created in this tab.
    this.browser.addEventListener("DOMWindowCreated", this._onWindowCreated, false);

    this._attached = true;
  },

  /**
   * Creates a thread actor and a pool for context-lifetime actors. It then sets
   * up the content window for debugging.
   */
  _pushContext: function BTA_pushContext() {
    dbg_assert(!this._contextPool, "Can't push multiple contexts");

    this._contextPool = new ActorPool(this.conn);
    this.conn.addActorPool(this._contextPool);

    this.threadActor = new ThreadActor(this);
    this.threadActor.addDebuggee(this.browser.contentWindow.wrappedJSObject);
    this._contextPool.addActor(this.threadActor);
  },

  /**
   * Exits the current thread actor and removes the context-lifetime actor pool.
   * The content window is no longer being debugged after this call.
   */
  _popContext: function BTA_popContext() {
    dbg_assert(!!this._contextPool, "No context to pop.");

    this.conn.removeActorPool(this._contextPool);
    this._contextPool = null;
    this.threadActor.exit();
    this.threadActor = null;
  },

  /**
   * Does the actual work of detaching from a tab.
   */
  _detach: function BTA_detach() {
    if (!this.attached) {
      return;
    }

    this.browser.removeEventListener("DOMWindowCreated", this._onWindowCreated, false);

    this._popContext();

    // Shut down actors that belong to this tab's pool.
    this.conn.removeActorPool(this._tabPool);
    this._tabPool = null;

    this._attached = false;
  },

  // Protocol Request Handlers

  onAttach: function BTA_onAttach(aRequest) {
    if (this.exited) {
      return { type: "exited" };
    }

    this._attach();

    return { type: "tabAttached", threadActor: this.threadActor.actorID };
  },

  onDetach: function BTA_onDetach(aRequest) {
    if (!this.attached) {
      return { error: "wrongState" };
    }

    this._detach();

    return { type: "detached" };
  },

  /**
   * Prepare to enter a nested event loop by disabling debuggee events.
   */
  preNest: function BTA_preNest() {
    if (!this.browser) {
      // The tab is already closed.
      return;
    }
    let windowUtils = this.browser.contentWindow
                          .QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindowUtils);
    windowUtils.suppressEventHandling(true);
    windowUtils.suspendTimeouts();
  },

  /**
   * Prepare to exit a nested event loop by enabling debuggee events.
   */
  postNest: function BTA_postNest(aNestData) {
    if (!this.browser) {
      // The tab is already closed.
      return;
    }
    let windowUtils = this.browser.contentWindow
                          .QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindowUtils);
    windowUtils.resumeTimeouts();
    windowUtils.suppressEventHandling(false);
  },

  /**
   * Handle location changes, by sending a tabNavigated notification to the
   * client.
   */
  onWindowCreated: function BTA_onWindowCreated(evt) {
    if (evt.target === this.browser.contentDocument) {
      if (this._attached) {
        this.conn.send({ from: this.actorID, type: "tabNavigated",
                         url: this.browser.contentDocument.URL });
      }
    }
  }
};

/**
 * The request types this actor can handle.
 */
BrowserTabActor.prototype.requestTypes = {
  "attach": BrowserTabActor.prototype.onAttach,
  "detach": BrowserTabActor.prototype.onDetach
};

/**
 * Registers handlers for new request types defined dynamically. This is used
 * for example by add-ons to augment the functionality of the tab actor.
 *
 * @param aName string
 *        The name of the new request type.
 * @param aFunction function
 *        The handler for this request type.
 */
DebuggerServer.addTabRequest = function DS_addTabRequest(aName, aFunction) {
  BrowserTabActor.prototype.requestTypes[aName] = function(aRequest) {
    if (!this.attached) {
      return { error: "wrongState" };
    }
    return aFunction(this, aRequest);
  }
};
