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
  // A map of actor names to actor instances provided by extensions.
  this._extraActors = {};

  this.onTabClosed = this.onTabClosed.bind(this);
  windowMediator.addListener(this);
}

BrowserRootActor.prototype = {

  /**
   * Return a 'hello' packet as specified by the Remote Debugging Protocol.
   */
  sayHello: function BRA_sayHello() {
    return {
      from: "root",
      applicationType: "browser",
      traits: {
        sources: true
      }
    };
  },

  /**
   * Disconnects the actor from the browser window.
   */
  disconnect: function BRA_disconnect() {
    windowMediator.removeListener(this);
    this._extraActors = null;

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
   * Handles the listTabs request.  Builds a list of actors for the tabs running
   * in the process.  The actors will survive until at least the next listTabs
   * request.
   */
  onListTabs: function BRA_onListTabs() {
    // Get actors for all the currently-running tabs (reusing existing actors
    // where applicable), and store them in an ActorPool.

    let actorPool = new ActorPool(this.conn);
    let tabActorList = [];

    // Get the chrome debugger actor.
    let actor = this._chromeDebugger;
    if (!actor) {
      actor = new ChromeDebuggerActor(this);
      actor.parentID = this.actorID;
      this._chromeDebugger = actor;
      actorPool.addActor(actor);
    }

    // Walk over open browser windows.
    let e = windowMediator.getEnumerator("navigator:browser");
    let top = windowMediator.getMostRecentWindow("navigator:browser");
    let selected;
    while (e.hasMoreElements()) {
      let win = e.getNext();

      // Watch the window for tab closes so we can invalidate actors as needed.
      this.watchWindow(win);

      // List the tabs in this browser.
      let selectedBrowser = win.getBrowser().selectedBrowser;

      let browsers = win.getBrowser().browsers;
      for each (let browser in browsers) {
        if (browser == selectedBrowser && win == top) {
          selected = tabActorList.length;
        }
        let actor = this._tabActors.get(browser);
        if (!actor) {
          actor = new BrowserTabActor(this.conn, browser, win.gBrowser);
          actor.parentID = this.actorID;
          this._tabActors.set(browser, actor);
        }
        actorPool.addActor(actor);
        tabActorList.push(actor);
      }
    }

    this._createExtraActors(DebuggerServer.globalActorFactories, actorPool);

    // Now drop the old actorID -> actor map.  Actors that still mattered were
    // added to the new map, others will go away.
    if (this._tabActorPool) {
      this.conn.removeActorPool(this._tabActorPool);
    }
    this._tabActorPool = actorPool;
    this.conn.addActorPool(this._tabActorPool);

    let response = {
      "from": "root",
      "selected": selected,
      "tabs": [actor.grip() for (actor of tabActorList)],
      "chromeDebugger": this._chromeDebugger.actorID
    };
    this._appendExtraActors(response);
    return response;
  },

  /**
   * Adds dynamically-added actors from add-ons to the provided pool.
   */
  _createExtraActors: function BRA_createExtraActors(aFactories, aPool) {
    // Walk over global actors added by extensions.
    for (let name in aFactories) {
      let actor = this._extraActors[name];
      if (!actor) {
        actor = aFactories[name].bind(null, this.conn, this);
        actor.prototype = aFactories[name].prototype;
        actor.parentID = this.actorID;
        this._extraActors[name] = actor;
      }
      aPool.addActor(actor);
    }
  },

  /**
   * Appends the extra actors to the specified object.
   */
  _appendExtraActors: function BRA_appendExtraActors(aObject) {
    for (let name in this._extraActors) {
      let actor = this._extraActors[name];
      aObject[name] = actor.actorID;
    }
  },

  /**
   * Watch a window that was visited during onListTabs for
   * tab closures.
   */
  watchWindow: function BRA_watchWindow(aWindow) {
    this.getTabContainer(aWindow).addEventListener("TabClose",
                                                   this.onTabClosed,
                                                   false);
  },

  /**
   * Stop watching a window for tab closes.
   */
  unwatchWindow: function BRA_unwatchWindow(aWindow) {
    this.getTabContainer(aWindow).removeEventListener("TabClose",
                                                      this.onTabClosed);
    this.exitTabActor(aWindow);
  },

  /**
   * Return the tab container for the specified window.
   */
  getTabContainer: function BRA_getTabContainer(aWindow) {
    return aWindow.getBrowser().tabContainer;
  },

  /**
   * When a tab is closed, exit its tab actor.  The actor
   * will be dropped at the next listTabs request.
   */
  onTabClosed:
  makeInfallible(function BRA_onTabClosed(aEvent) {
    this.exitTabActor(aEvent.target.linkedBrowser);
  }, "BrowserRootActor.prototype.onTabClosed"),

  /**
   * Exit the tab actor of the specified tab.
   */
  exitTabActor: function BRA_exitTabActor(aWindow) {
    let actor = this._tabActors.get(aWindow);
    if (actor) {
      this._tabActors.delete(actor.browser);
      actor.exit();
    }
  },

  // ChromeDebuggerActor hooks.

  /**
   * Add the specified actor to the default actor pool connection, in order to
   * keep it alive as long as the server is. This is used by breakpoints in the
   * thread and chrome debugger actors.
   *
   * @param actor aActor
   *        The actor object.
   */
  addToParentPool: function BRA_addToParentPool(aActor) {
    this.conn.addActor(aActor);
  },

  /**
   * Remove the specified actor from the default actor pool.
   *
   * @param BreakpointActor aActor
   *        The actor object.
   */
  removeFromParentPool: function BRA_removeFromParentPool(aActor) {
    this.conn.removeActor(aActor);
  },

  /**
   * Prepare to enter a nested event loop by disabling debuggee events.
   */
  preNest: function BRA_preNest() {
    // Disable events in all open windows.
    let e = windowMediator.getEnumerator(null);
    while (e.hasMoreElements()) {
      let win = e.getNext();
      let windowUtils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
      windowUtils.suppressEventHandling(true);
      windowUtils.suspendTimeouts();
    }
  },

  /**
   * Prepare to exit a nested event loop by enabling debuggee events.
   */
  postNest: function BRA_postNest(aNestData) {
    // Enable events in all open windows.
    let e = windowMediator.getEnumerator(null);
    while (e.hasMoreElements()) {
      let win = e.getNext();
      let windowUtils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
      windowUtils.resumeTimeouts();
      windowUtils.suppressEventHandling(false);
    }
  },

  // nsIWindowMediatorListener.

  onWindowTitleChange: function BRA_onWindowTitleChange(aWindow, aTitle) { },
  onOpenWindow: function BRA_onOpenWindow(aWindow) { },
  onCloseWindow:
  makeInfallible(function BRA_onCloseWindow(aWindow) {
    // An nsIWindowMediatorListener's onCloseWindow method gets passed all
    // sorts of windows; we only care about the tab containers. Those have
    // 'getBrowser' methods.
    if (aWindow.getBrowser) {
      this.unwatchWindow(aWindow);
    }
  }, "BrowserRootActor.prototype.onCloseWindow"),
};

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
 * @param aTabBrowser tabbrowser
 *        The tabbrowser that can receive nsIWebProgressListener events.
 */
function BrowserTabActor(aConnection, aBrowser, aTabBrowser)
{
  this.conn = aConnection;
  this._browser = aBrowser;
  this._tabbrowser = aTabBrowser;
  this._tabActorPool = null;
  // A map of actor names to actor instances provided by extensions.
  this._extraActors = {};

  this._createExtraActors = BrowserRootActor.prototype._createExtraActors.bind(this);
  this._appendExtraActors = BrowserRootActor.prototype._appendExtraActors.bind(this);
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

  _pendingNavigation: null,

  /**
   * Add the specified actor to the default actor pool connection, in order to
   * keep it alive as long as the server is. This is used by breakpoints in the
   * thread actor.
   *
   * @param actor aActor
   *        The actor object.
   */
  addToParentPool: function BTA_addToParentPool(aActor) {
    this.conn.addActor(aActor);
  },

  /**
   * Remove the specified actor from the default actor pool.
   *
   * @param BreakpointActor aActor
   *        The actor object.
   */
  removeFromParentPool: function BTA_removeFromParentPool(aActor) {
    this.conn.removeActor(aActor);
  },

  // A constant prefix that will be used to form the actor ID by the server.
  actorPrefix: "tab",

  /**
   * Getter for the tab title.
   * @return string
   *         Tab title.
   */
  get title() {
    let title = this.browser.contentTitle;
    // If contentTitle is empty (e.g. on a not-yet-restored tab), but there is a
    // tabbrowser (i.e. desktop Firefox, but not Fennec), we can use the label
    // as the title.
    if (!title && this._tabbrowser) {
      title = this._tabbrowser
                  ._getTabForContentWindow(this.contentWindow).label;
    }
    return title;
  },

  /**
   * Getter for the tab URL.
   * @return string
   *         Tab URL.
   */
  get url() {
    return this.browser.currentURI.spec;
  },

  /**
   * Getter for the tab content window.
   * @return nsIDOMWindow
   *         Tab content window.
   */
  get contentWindow() {
    return this.browser.contentWindow;
  },

  grip: function BTA_grip() {
    dbg_assert(!this.exited,
               "grip() shouldn't be called on exited browser actor.");
    dbg_assert(this.actorID,
               "tab should have an actorID.");

    let response = {
      actor: this.actorID,
      title: this.title,
      url: this.url,
    };

    // Walk over tab actors added by extensions and add them to a new ActorPool.
    let actorPool = new ActorPool(this.conn);
    this._createExtraActors(DebuggerServer.tabActorFactories, actorPool);
    if (!actorPool.isEmpty()) {
      this._tabActorPool = actorPool;
      this.conn.addActorPool(this._tabActorPool);
    }

    this._appendExtraActors(response);
    return response;
  },

  /**
   * Called when the actor is removed from the connection.
   */
  disconnect: function BTA_disconnect() {
    this._detach();
    this._extraActors = null;
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
    this._tabbrowser = null;
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
    this.browser.addEventListener("pageshow", this._onWindowCreated, true);
    if (this._tabbrowser) {
      this._progressListener = new DebuggerProgressListener(this);
    }

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

    this.threadActor = new ThreadActor(this, this.contentWindow.wrappedJSObject);
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

    if (this._progressListener) {
      this._progressListener.destroy();
    }

    this.browser.removeEventListener("DOMWindowCreated", this._onWindowCreated, true);
    this.browser.removeEventListener("pageshow", this._onWindowCreated, true);

    this._popContext();

    // Shut down actors that belong to this tab's pool.
    this.conn.removeActorPool(this._tabPool);
    this._tabPool = null;
    if (this._tabActorPool) {
      this.conn.removeActorPool(this._tabActorPool, true);
      this._tabActorPool = null;
    }

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
    let windowUtils = this.contentWindow
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
    let windowUtils = this.contentWindow
                          .QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindowUtils);
    windowUtils.resumeTimeouts();
    windowUtils.suppressEventHandling(false);
    if (this._pendingNavigation) {
      this._pendingNavigation.resume();
      this._pendingNavigation = null;
    }
  },

  /**
   * Handle location changes, by sending a tabNavigated notification to the
   * client.
   */
  onWindowCreated:
  makeInfallible(function BTA_onWindowCreated(evt) {
    if (evt.target === this.browser.contentDocument) {
      // pageshow events for non-persisted pages have already been handled by a
      // prior DOMWindowCreated event.
      if (evt.type == "pageshow" && !evt.persisted) {
        return;
      }
      if (this._attached) {
        this.threadActor.clearDebuggees();
        if (this.threadActor.dbg) {
          this.threadActor.dbg.enabled = true;
        }
      }
    }

    if (this._attached) {
      this.threadActor.global = evt.target.defaultView.wrappedJSObject;
      if (this.threadActor.attached) {
        this.threadActor.findGlobals();
      }
    }
  }, "BrowserTabActor.prototype.onWindowCreated"),

  /**
   * Tells if the window.console object is native or overwritten by script in
   * the page.
   *
   * @param nsIDOMWindow aWindow
   *        The window object you want to check.
   * @return boolean
   *         True if the window.console object is native, or false otherwise.
   */
  hasNativeConsoleAPI: function BTA_hasNativeConsoleAPI(aWindow) {
    let isNative = false;
    try {
      let console = aWindow.wrappedJSObject.console;
      isNative = "__mozillaConsole__" in console;
    }
    catch (ex) { }
    return isNative;
  },
};

/**
 * The request types this actor can handle.
 */
BrowserTabActor.prototype.requestTypes = {
  "attach": BrowserTabActor.prototype.onAttach,
  "detach": BrowserTabActor.prototype.onDetach
};

/**
 * The DebuggerProgressListener object is an nsIWebProgressListener which
 * handles onStateChange events for the inspected browser. If the user tries to
 * navigate away from a paused page, the listener makes sure that the debuggee
 * is resumed before the navigation begins.
 *
 * @param BrowserTabActor aBrowserTabActor
 *        The tab actor associated with this listener.
 */
function DebuggerProgressListener(aBrowserTabActor) {
  this._tabActor = aBrowserTabActor;
  this._tabActor._tabbrowser.addProgressListener(this);
}

DebuggerProgressListener.prototype = {
  onStateChange:
  makeInfallible(function DPL_onStateChange(aProgress, aRequest, aFlag, aStatus) {
    let isStart = aFlag & Ci.nsIWebProgressListener.STATE_START;
    let isStop = aFlag & Ci.nsIWebProgressListener.STATE_STOP;
    let isDocument = aFlag & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT;
    let isNetwork = aFlag & Ci.nsIWebProgressListener.STATE_IS_NETWORK;
    let isRequest = aFlag & Ci.nsIWebProgressListener.STATE_IS_REQUEST;
    let isWindow = aFlag & Ci.nsIWebProgressListener.STATE_IS_WINDOW;

    // Skip non-interesting states.
    if (!isWindow || !isNetwork ||
        aProgress.DOMWindow != this._tabActor.contentWindow) {
      return;
    }

    if (isStart && aRequest instanceof Ci.nsIChannel) {
      // Proceed normally only if the debuggee is not paused.
      if (this._tabActor.threadActor.state == "paused") {
        aRequest.suspend();
        this._tabActor.threadActor.onResume();
        this._tabActor.threadActor.dbg.enabled = false;
        this._tabActor._pendingNavigation = aRequest;
      }

      this._tabActor.threadActor.disableAllBreakpoints();
      this._tabActor.conn.send({
        from: this._tabActor.actorID,
        type: "tabNavigated",
        url: aRequest.URI.spec,
        nativeConsoleAPI: true,
        state: "start",
      });
    } else if (isStop) {
      if (this._tabActor.threadActor.state == "running") {
        this._tabActor.threadActor.dbg.enabled = true;
      }

      let window = this._tabActor.contentWindow;
      this._tabActor.conn.send({
        from: this._tabActor.actorID,
        type: "tabNavigated",
        url: this._tabActor.url,
        title: this._tabActor.title,
        nativeConsoleAPI: this._tabActor.hasNativeConsoleAPI(window),
        state: "stop",
      });
    }
  }, "DebuggerProgressListener.prototype.onStateChange"),

  /**
   * Destroy the progress listener instance.
   */
  destroy: function DPL_destroy() {
    if (this._tabActor._tabbrowser.removeProgressListener) {
      try {
        this._tabActor._tabbrowser.removeProgressListener(this);
      } catch (ex) {
        // This can throw during browser shutdown.
      }
    }
    this._tabActor._progressListener = null;
    this._tabActor = null;
  }
};
