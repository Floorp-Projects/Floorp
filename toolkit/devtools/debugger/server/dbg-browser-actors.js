/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  windowMediator.addListener(this);
}

BrowserRootActor.prototype = {
  /**
   * Return a 'hello' packet as specified by the Remote Debugging Protocol.
   */
  sayHello: function BRA_sayHello() {
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
    let top = windowMediator.getMostRecentWindow("navigator:browser");
    let selected;
    while (e.hasMoreElements()) {
      let win = e.getNext();

      // Watch the window for tab closes so we can invalidate
      // actors as needed.
      this.watchWindow(win);

      // List the tabs in this browser.
      let selectedBrowser = win.getBrowser().selectedBrowser;
      let browsers = win.getBrowser().browsers;
      for each (let browser in browsers) {
        if (browser == selectedBrowser && win == top) {
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
   * Exit the tab actor of the specified tab.
   */
  exitTabActor: function BRA_exitTabActor(aWindow) {
    let actor = this._tabActors.get(aWindow);
    if (actor) {
      actor.exit();
    }
  },

  // nsIWindowMediatorListener
  onWindowTitleChange: function BRA_onWindowTitleChange(aWindow, aTitle) { },
  onOpenWindow: function BRA_onOpenWindow(aWindow) { },
  onCloseWindow: function BRA_onCloseWindow(aWindow) {
    // An nsIWindowMediatorListener's onCloseWindow method gets passed all
    // sorts of windows; we only care about the tab containers. Those have
    // 'getBrowser' methods.
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

  /**
   * Add the specified breakpoint to the default actor pool connection, in order
   * to be alive as long as the server is.
   *
   * @param BreakpointActor aActor
   *        The actor object.
   */
  addToBreakpointPool: function BTA_addToBreakpointPool(aActor) {
    this.conn.addActor(aActor);
  },

  /**
   * Remove the specified breakpint from the default actor pool.
   *
   * @param string aActor
   *        The actor ID.
   */
  removeFromBreakpointPool: function BTA_removeFromBreakpointPool(aActor) {
    this.conn.removeActor(aActor);
  },

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
    this.browser.addEventListener("DOMWindowCreated", this._onWindowCreated, true);

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
    this._addDebuggees(this.browser.contentWindow.wrappedJSObject);
    this._contextPool.addActor(this.threadActor);
  },

  /**
   * Add the provided window and all windows in its frame tree as debuggees.
   */
  _addDebuggees: function BTA__addDebuggees(aWindow) {
    this.threadActor.addDebuggee(aWindow);
    let frames = aWindow.frames;
    for (let i = 0; i < frames.length; i++) {
      this._addDebuggees(frames[i]);
    }
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

    this.browser.removeEventListener("DOMWindowCreated", this._onWindowCreated, true);

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
