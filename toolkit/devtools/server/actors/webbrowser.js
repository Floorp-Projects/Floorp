/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let { Ci, Cu } = require("chrome");
let Services = require("Services");
let { ActorPool, createExtraActors, appendExtraActors } = require("devtools/server/actors/common");
let { RootActor } = require("devtools/server/actors/root");
let { AddonThreadActor, ThreadActor } = require("devtools/server/actors/script");
let { DebuggerServer } = require("devtools/server/main");
let DevToolsUtils = require("devtools/toolkit/DevToolsUtils");
let { dbg_assert } = DevToolsUtils;

let {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager", "resource://gre/modules/AddonManager.jsm");

// Assumptions on events module:
// events needs to be dispatched synchronously,
// by calling the listeners in the order or registration.
XPCOMUtils.defineLazyGetter(this, "events", () => {
  return require("sdk/event/core");
});

// Also depends on following symbols, shared by common scope with main.js:
// DebuggerServer, CommonCreateExtraActors, CommonAppendExtraActors, ActorPool,
// ThreadActor

/**
 * Browser-specific actors.
 */

/**
 * Yield all windows of type |aWindowType|, from the oldest window to the
 * youngest, using nsIWindowMediator::getEnumerator. We're usually
 * interested in "navigator:browser" windows.
 */
function allAppShellDOMWindows(aWindowType)
{
  let e = Services.wm.getEnumerator(aWindowType);
  while (e.hasMoreElements()) {
    yield e.getNext();
  }
}

exports.allAppShellDOMWindows = allAppShellDOMWindows;

/**
 * Retrieve the window type of the top-level window |aWindow|.
 */
function appShellDOMWindowType(aWindow) {
  /* This is what nsIWindowMediator's enumerator checks. */
  return aWindow.document.documentElement.getAttribute('windowtype');
}

/**
 * Send Debugger:Shutdown events to all "navigator:browser" windows.
 */
function sendShutdownEvent() {
  for (let win of allAppShellDOMWindows(DebuggerServer.chromeWindowType)) {
    let evt = win.document.createEvent("Event");
    evt.initEvent("Debugger:Shutdown", true, false);
    win.document.documentElement.dispatchEvent(evt);
  }
}

exports.sendShutdownEvent = sendShutdownEvent;

/**
 * Construct a root actor appropriate for use in a server running in a
 * browser. The returned root actor:
 * - respects the factories registered with DebuggerServer.addGlobalActor,
 * - uses a BrowserTabList to supply tab actors,
 * - sends all navigator:browser window documents a Debugger:Shutdown event
 *   when it exits.
 *
 * * @param aConnection DebuggerServerConnection
 *        The conection to the client.
 */
function createRootActor(aConnection)
{
  return new RootActor(aConnection,
                       {
                         tabList: new BrowserTabList(aConnection),
                         addonList: new BrowserAddonList(aConnection),
                         globalActorFactories: DebuggerServer.globalActorFactories,
                         onShutdown: sendShutdownEvent
                       });
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
 * Some notes:
 *
 * This constructor is specific to the desktop browser environment; it
 * maintains the tab list by tracking XUL windows and their XUL documents'
 * "tabbrowser", "tab", and "browser" elements. What's entailed in maintaining
 * an accurate list of open tabs in this context?
 *
 * - Opening and closing XUL windows:
 *
 * An nsIWindowMediatorListener is notified when new XUL windows (i.e., desktop
 * windows) are opened and closed. It is not notified of individual content
 * browser tabs coming and going within such a XUL window. That seems
 * reasonable enough; it's concerned with XUL windows, not tab elements in the
 * window's XUL document.
 *
 * However, even if we attach TabOpen and TabClose event listeners to each XUL
 * window as soon as it is created:
 *
 * - we do not receive a TabOpen event for the initial empty tab of a new XUL
 *   window; and
 *
 * - we do not receive TabClose events for the tabs of a XUL window that has
 *   been closed.
 *
 * This means that TabOpen and TabClose events alone are not sufficient to
 * maintain an accurate list of live tabs and mark tab actors as closed
 * promptly. Our nsIWindowMediatorListener onCloseWindow handler must find and
 * exit all actors for tabs that were in the closing window.
 *
 * Since this is a bit hairy, we don't make each individual attached tab actor
 * responsible for noticing when it has been closed; we watch for that, and
 * promise to call each actor's 'exit' method when it's closed, regardless of
 * how we learn the news.
 *
 * - nsIWindowMediator locks
 *
 * nsIWindowMediator holds a lock protecting its list of top-level windows
 * while it calls nsIWindowMediatorListener methods. nsIWindowMediator's
 * GetEnumerator method also tries to acquire that lock. Thus, enumerating
 * windows from within a listener method deadlocks (bug 873589). Rah. One
 * can sometimes work around this by leaving the enumeration for a later
 * tick.
 *
 * - Dragging tabs between windows:
 *
 * When a tab is dragged from one desktop window to another, we receive a
 * TabOpen event for the new tab, and a TabClose event for the old tab; tab XUL
 * elements do not really move from one document to the other (although their
 * linked browser's content window objects do).
 *
 * However, while we could thus assume that each tab stays with the XUL window
 * it belonged to when it was created, I'm not sure this is behavior one should
 * rely upon. When a XUL window is closed, we take the less efficient, more
 * conservative approach of simply searching the entire table for actors that
 * belong to the closing XUL window, rather than trying to somehow track which
 * XUL window each tab belongs to.
 */
function BrowserTabList(aConnection)
{
  this._connection = aConnection;

  /*
   * The XUL document of a tabbed browser window has "tab" elements, whose
   * 'linkedBrowser' JavaScript properties are "browser" elements; those
   * browsers' 'contentWindow' properties are wrappers on the tabs' content
   * window objects.
   *
   * This map's keys are "browser" XUL elements; it maps each browser element
   * to the tab actor we've created for its content window, if we've created
   * one. This map serves several roles:
   *
   * - During iteration, we use it to find actors we've created previously.
   *
   * - On a TabClose event, we use it to find the tab's actor and exit it.
   *
   * - When the onCloseWindow handler is called, we iterate over it to find all
   *   tabs belonging to the closing XUL window, and exit them.
   *
   * - When it's empty, and the onListChanged hook is null, we know we can
   *   stop listening for events and notifications.
   *
   * We listen for TabClose events and onCloseWindow notifications in order to
   * send onListChanged notifications, but also to tell actors when their
   * referent has gone away and remove entries for dead browsers from this map.
   * If that code is working properly, neither this map nor the actors in it
   * should ever hold dead tabs alive.
   */
  this._actorByBrowser = new Map();

  /* The current onListChanged handler, or null. */
  this._onListChanged = null;

  /*
   * True if we've been iterated over since we last called our onListChanged
   * hook.
   */
  this._mustNotify = false;

  /* True if we're testing, and should throw if consistency checks fail. */
  this._testing = false;
}

BrowserTabList.prototype.constructor = BrowserTabList;


/**
 * Get the selected browser for the given navigator:browser window.
 * @private
 * @param aWindow nsIChromeWindow
 *        The navigator:browser window for which you want the selected browser.
 * @return nsIDOMElement|null
 *         The currently selected xul:browser element, if any. Note that the
 *         browser window might not be loaded yet - the function will return
 *         |null| in such cases.
 */
BrowserTabList.prototype._getSelectedBrowser = function(aWindow) {
  return aWindow.gBrowser ? aWindow.gBrowser.selectedBrowser : null;
};

BrowserTabList.prototype._getChildren = function(aWindow) {
  return aWindow.gBrowser.browsers;
};

BrowserTabList.prototype.getList = function() {
  let topXULWindow = Services.wm.getMostRecentWindow(DebuggerServer.chromeWindowType);

  // As a sanity check, make sure all the actors presently in our map get
  // picked up when we iterate over all windows' tabs.
  let initialMapSize = this._actorByBrowser.size;
  let foundCount = 0;

  // To avoid mysterious behavior if tabs are closed or opened mid-iteration,
  // we update the map first, and then make a second pass over it to yield
  // the actors. Thus, the sequence yielded is always a snapshot of the
  // actors that were live when we began the iteration.

  let actorPromises = [];

  // Iterate over all navigator:browser XUL windows.
  for (let win of allAppShellDOMWindows(DebuggerServer.chromeWindowType)) {
    let selectedBrowser = this._getSelectedBrowser(win);
    if (!selectedBrowser) {
      continue;
    }

    // For each tab in this XUL window, ensure that we have an actor for
    // it, reusing existing actors where possible. We actually iterate
    // over 'browser' XUL elements, and BrowserTabActor uses
    // browser.contentWindow as the debuggee global.
    for (let browser of this._getChildren(win)) {
      // Do we have an existing actor for this browser? If not, create one.
      let actor = this._actorByBrowser.get(browser);
      if (actor) {
        actorPromises.push(promise.resolve(actor));
        foundCount++;
      } else if (browser.isRemoteBrowser) {
        actor = new RemoteBrowserTabActor(this._connection, browser);
        this._actorByBrowser.set(browser, actor);
        let promise = actor.connect().then((form) => {
          actor._form = form;
          return actor;
        });
        actorPromises.push(promise);
      } else {
        actor = new BrowserTabActor(this._connection, browser, win.gBrowser);
        this._actorByBrowser.set(browser, actor);
        actorPromises.push(promise.resolve(actor));
      }

      // Set the 'selected' properties on all actors correctly.
      actor.selected = (win === topXULWindow && browser === selectedBrowser);
    }
  }

  if (this._testing && initialMapSize !== foundCount)
    throw Error("_actorByBrowser map contained actors for dead tabs");

  this._mustNotify = true;
  this._checkListening();

  return promise.all(actorPromises);
};

Object.defineProperty(BrowserTabList.prototype, 'onListChanged', {
  enumerable: true, configurable:true,
  get: function() { return this._onListChanged; },
  set: function(v) {
    if (v !== null && typeof v !== 'function') {
      throw Error("onListChanged property may only be set to 'null' or a function");
    }
    this._onListChanged = v;
    this._checkListening();
  }
});

/**
 * The set of tabs has changed somehow. Call our onListChanged handler, if
 * one is set, and if we haven't already called it since the last iteration.
 */
BrowserTabList.prototype._notifyListChanged = function() {
  if (!this._onListChanged)
    return;
  if (this._mustNotify) {
    this._onListChanged();
    this._mustNotify = false;
  }
};

/**
 * Exit |aActor|, belonging to |aBrowser|, and notify the onListChanged
 * handle if needed.
 */
BrowserTabList.prototype._handleActorClose = function(aActor, aBrowser) {
  if (this._testing) {
    if (this._actorByBrowser.get(aBrowser) !== aActor) {
      throw Error("BrowserTabActor not stored in map under given browser");
    }
    if (aActor.browser !== aBrowser) {
      throw Error("actor's browser and map key don't match");
    }
  }

  this._actorByBrowser.delete(aBrowser);
  aActor.exit();

  this._notifyListChanged();
  this._checkListening();
};

/**
 * Make sure we are listening or not listening for activity elsewhere in
 * the browser, as appropriate. Other than setting up newly created XUL
 * windows, all listener / observer connection and disconnection should
 * happen here.
 */
BrowserTabList.prototype._checkListening = function() {
  /*
   * If we have an onListChanged handler that we haven't sent an announcement
   * to since the last iteration, we need to watch for tab creation.
   *
   * Oddly, we don't need to watch for 'close' events here. If our actor list
   * is empty, then either it was empty the last time we iterated, and no
   * close events are possible, or it was not empty the last time we
   * iterated, but all the actors have since been closed, and we must have
   * sent a notification already when they closed.
   */
  this._listenForEventsIf(this._onListChanged && this._mustNotify,
                          "_listeningForTabOpen", ["TabOpen", "TabSelect"]);

  /* If we have live actors, we need to be ready to mark them dead. */
  this._listenForEventsIf(this._actorByBrowser.size > 0,
                          "_listeningForTabClose", ["TabClose"]);

  /*
   * We must listen to the window mediator in either case, since that's the
   * only way to find out about tabs that come and go when top-level windows
   * are opened and closed.
   */
  this._listenToMediatorIf((this._onListChanged && this._mustNotify) ||
                           (this._actorByBrowser.size > 0));
};

/*
 * Add or remove event listeners for all XUL windows.
 *
 * @param aShouldListen boolean
 *    True if we should add event handlers; false if we should remove them.
 * @param aGuard string
 *    The name of a guard property of 'this', indicating whether we're
 *    already listening for those events.
 * @param aEventNames array of strings
 *    An array of event names.
 */
BrowserTabList.prototype._listenForEventsIf = function(aShouldListen, aGuard, aEventNames) {
  if (!aShouldListen !== !this[aGuard]) {
    let op = aShouldListen ? "addEventListener" : "removeEventListener";
    for (let win of allAppShellDOMWindows(DebuggerServer.chromeWindowType)) {
      for (let name of aEventNames) {
        win[op](name, this, false);
      }
    }
    this[aGuard] = aShouldListen;
  }
};

/**
 * Implement nsIDOMEventListener.
 */
BrowserTabList.prototype.handleEvent = DevToolsUtils.makeInfallible(function(aEvent) {
  switch (aEvent.type) {
  case "TabOpen":
  case "TabSelect":
    /* Don't create a new actor; iterate will take care of that. Just notify. */
    this._notifyListChanged();
    this._checkListening();
    break;
  case "TabClose":
    let browser = aEvent.target.linkedBrowser;
    let actor = this._actorByBrowser.get(browser);
    if (actor) {
      this._handleActorClose(actor, browser);
    }
    break;
  }
}, "BrowserTabList.prototype.handleEvent");

/*
 * If |aShouldListen| is true, ensure we've registered a listener with the
 * window mediator. Otherwise, ensure we haven't registered a listener.
 */
BrowserTabList.prototype._listenToMediatorIf = function(aShouldListen) {
  if (!aShouldListen !== !this._listeningToMediator) {
    let op = aShouldListen ? "addListener" : "removeListener";
    Services.wm[op](this);
    this._listeningToMediator = aShouldListen;
  }
};

/**
 * nsIWindowMediatorListener implementation.
 *
 * See _onTabClosed for explanation of why we needn't actually tweak any
 * actors or tables here.
 *
 * An nsIWindowMediatorListener's methods get passed all sorts of windows; we
 * only care about the tab containers. Those have 'getBrowser' methods.
 */
BrowserTabList.prototype.onWindowTitleChange = () => { };

BrowserTabList.prototype.onOpenWindow = DevToolsUtils.makeInfallible(function(aWindow) {
  let handleLoad = DevToolsUtils.makeInfallible(() => {
    /* We don't want any further load events from this window. */
    aWindow.removeEventListener("load", handleLoad, false);

    if (appShellDOMWindowType(aWindow) !== DebuggerServer.chromeWindowType)
      return;

    // Listen for future tab activity.
    if (this._listeningForTabOpen) {
      aWindow.addEventListener("TabOpen", this, false);
      aWindow.addEventListener("TabSelect", this, false);
    }
    if (this._listeningForTabClose) {
      aWindow.addEventListener("TabClose", this, false);
    }

    // As explained above, we will not receive a TabOpen event for this
    // document's initial tab, so we must notify our client of the new tab
    // this will have.
    this._notifyListChanged();
  });

  /*
   * You can hardly do anything at all with a XUL window at this point; it
   * doesn't even have its document yet. Wait until its document has
   * loaded, and then see what we've got. This also avoids
   * nsIWindowMediator enumeration from within listeners (bug 873589).
   */
  aWindow = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindow);

  aWindow.addEventListener("load", handleLoad, false);
}, "BrowserTabList.prototype.onOpenWindow");

BrowserTabList.prototype.onCloseWindow = DevToolsUtils.makeInfallible(function(aWindow) {
  aWindow = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindow);

  if (appShellDOMWindowType(aWindow) !== DebuggerServer.chromeWindowType)
    return;

  /*
   * nsIWindowMediator deadlocks if you call its GetEnumerator method from
   * a nsIWindowMediatorListener's onCloseWindow hook (bug 873589), so
   * handle the close in a different tick.
   */
  Services.tm.currentThread.dispatch(DevToolsUtils.makeInfallible(() => {
    /*
     * Scan the entire map for actors representing tabs that were in this
     * top-level window, and exit them.
     */
    for (let [browser, actor] of this._actorByBrowser) {
      /* The browser document of a closed window has no default view. */
      if (!browser.ownerDocument.defaultView) {
        this._handleActorClose(actor, browser);
      }
    }
  }, "BrowserTabList.prototype.onCloseWindow's delayed body"), 0);
}, "BrowserTabList.prototype.onCloseWindow");

exports.BrowserTabList = BrowserTabList;

/**
 * Creates a tab actor for handling requests to a browser tab, like
 * attaching and detaching. TabActor respects the actor factories
 * registered with DebuggerServer.addTabActor.
 *
 * This class is subclassed by BrowserTabActor and
 * ContentActor. Subclasses are expected to implement a getter
 * the docShell properties.
 *
 * @param aConnection DebuggerServerConnection
 *        The conection to the client.
 * @param aChromeEventHandler
 *        An object on which listen for DOMWindowCreated and pageshow events.
 */
function TabActor(aConnection)
{
  this.conn = aConnection;
  this._tabActorPool = null;
  // A map of actor names to actor instances provided by extensions.
  this._extraActors = {};
  this._exited = false;

  this.traits = { reconfigure: true };
}

// XXX (bug 710213): TabActor attach/detach/exit/disconnect is a
// *complete* mess, needs to be rethought asap.

TabActor.prototype = {
  traits: null,

  get exited() { return this._exited; },
  get attached() { return !!this._attached; },

  _tabPool: null,
  get tabActorPool() { return this._tabPool; },

  _contextPool: null,
  get contextActorPool() { return this._contextPool; },

  _pendingNavigation: null,

  // A constant prefix that will be used to form the actor ID by the server.
  actorPrefix: "tab",

  /**
   * An object on which listen for DOMWindowCreated and pageshow events.
   */
  get chromeEventHandler() {
    // TODO: bug 992778, fix docShell.chromeEventHandler in child processes
    return this.docShell.chromeEventHandler ||
           this.docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIContentFrameMessageManager);
  },

  /**
   * Getter for the nsIMessageManager associated to the tab.
   */
  get messageManager() {
    return this.docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIContentFrameMessageManager);
  },

  /**
   * Getter for the tab's doc shell.
   */
  get docShell() {
    throw "The docShell getter should be implemented by a subclass of TabActor";
  },

  /**
   * Getter for the list of all docshell in this tabActor
   * @return {Array}
   */
  get docShells() {
    let docShellsEnum = this.docShell.getDocShellEnumerator(
      Ci.nsIDocShellTreeItem.typeAll,
      Ci.nsIDocShell.ENUMERATE_FORWARDS
    );

    let docShells = [];
    while (docShellsEnum.hasMoreElements()) {
      docShells.push(docShellsEnum.getNext());
    }

    return docShells;
  },

  /**
   * Getter for the tab content's DOM window.
   */
  get window() {
    return this.docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindow);
  },

  /**
   * Getter for the list of all content DOM windows in this tabActor
   * @return {Array}
   */
  get windows() {
    return this.docShells.map(docShell => {
      return docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDOMWindow);
    });
  },

  /**
   * Getter for the nsIWebProgress for watching this window.
   */
  get webProgress() {
    return this.docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
  },

  /**
   * Getter for the nsIWebNavigation for the tab.
   */
  get webNavigation() {
    return this.docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebNavigation);
  },

  /**
   * Getter for the tab's document.
   */
  get contentDocument() {
    return this.webNavigation.document;
  },

  /**
   * Getter for the tab title.
   * @return string
   *         Tab title.
   */
  get title() {
    return this.contentDocument.contentTitle;
  },

  /**
   * Getter for the tab URL.
   * @return string
   *         Tab URL.
   */
  get url() {
    if (this.webNavigation.currentURI) {
      return this.webNavigation.currentURI.spec;
    }
    // Abrupt closing of the browser window may leave callbacks without a
    // currentURI.
    return null;
  },

  form: function BTA_form() {
    dbg_assert(!this.exited,
               "grip() shouldn't be called on exited browser actor.");
    dbg_assert(this.actorID,
               "tab should have an actorID.");

    let windowUtils = this.window
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils);

    let response = {
      actor: this.actorID,
      title: this.title,
      url: this.url,
      outerWindowID: windowUtils.outerWindowID
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
    this._exited = true;
  },

  /**
   * Called by the root actor when the underlying tab is closed.
   */
  exit: function BTA_exit() {
    if (this.exited) {
      return;
    }

    // Tell the thread actor that the tab is closed, so that it may terminate
    // instead of resuming the debuggee script.
    if (this._attached) {
      this.threadActor._tabClosed = true;
    }

    if (this._detach()) {
      this.conn.send({ from: this.actorID,
                       type: "tabDetached" });
    }

    this._exited = true;
  },

  /* Support for DebuggerServer.addTabActor. */
  _createExtraActors: createExtraActors,
  _appendExtraActors: appendExtraActors,

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

    this._progressListener = new DebuggerProgressListener(this);
    this._progressListener.watch(this.docShell);

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

    this.threadActor = new ThreadActor(this, this.window);
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
   *
   * @returns false if the tab wasn't attached or true of detaching succeeds.
   */
  _detach: function BTA_detach() {
    if (!this.attached) {
      return false;
    }

    // Check for docShell availability, as it can be already gone
    // during Firefox shutdown.
    if (this.docShell) {
      this._progressListener.unwatch(this.docShell);
    }
    this._progressListener.destroy();
    this._progressListener = null;

    this._popContext();

    // Shut down actors that belong to this tab's pool.
    this.conn.removeActorPool(this._tabPool);
    this._tabPool = null;
    if (this._tabActorPool) {
      this.conn.removeActorPool(this._tabActorPool);
      this._tabActorPool = null;
    }

    this._attached = false;
    return true;
  },

  // Protocol Request Handlers

  onAttach: function BTA_onAttach(aRequest) {
    if (this.exited) {
      return { type: "exited" };
    }

    this._attach();

    return {
      type: "tabAttached",
      threadActor: this.threadActor.actorID,
      cacheEnabled: this._getCacheEnabled(),
      javascriptEnabled: this._getJavascriptEnabled(),
      traits: this.traits,
    };
  },

  onDetach: function BTA_onDetach(aRequest) {
    if (!this._detach()) {
      return { error: "wrongState" };
    }

    return { type: "detached" };
  },

  /**
   * Reload the page in this tab.
   */
  onReload: function(aRequest) {
    let force = aRequest && aRequest.options && aRequest.options.force;
    // Wait a tick so that the response packet can be dispatched before the
    // subsequent navigation event packet.
    Services.tm.currentThread.dispatch(DevToolsUtils.makeInfallible(() => {
      this.webNavigation.reload(force ? Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE
                                      : Ci.nsIWebNavigation.LOAD_FLAGS_NONE);
    }, "TabActor.prototype.onReload's delayed body"), 0);
    return {};
  },

  /**
   * Navigate this tab to a new location
   */
  onNavigateTo: function(aRequest) {
    // Wait a tick so that the response packet can be dispatched before the
    // subsequent navigation event packet.
    Services.tm.currentThread.dispatch(DevToolsUtils.makeInfallible(() => {
      this.window.location = aRequest.url;
    }, "TabActor.prototype.onNavigateTo's delayed body"), 0);
    return {};
  },

  /**
   * Reconfigure options.
   */
  onReconfigure: function (aRequest) {
    let options = aRequest.options || {};

    this._toggleJsOrCache(options);
    return {};
  },

  /**
   * Handle logic to enable/disable JS/cache.
   */
  _toggleJsOrCache: function(options) {
    // Wait a tick so that the response packet can be dispatched before the
    // subsequent navigation event packet.
    let reload = false;

    if (typeof options.javascriptEnabled !== "undefined" &&
        options.javascriptEnabled !== this._getJavascriptEnabled()) {
      this._setJavascriptEnabled(options.javascriptEnabled);
      reload = true;
    }
    if (typeof options.cacheEnabled !== "undefined" &&
        options.cacheEnabled !== this._getCacheEnabled()) {
      this._setCacheEnabled(options.cacheEnabled);
      reload = true;
    }

    // Reload if:
    //  - there's an explicit `performReload` flag and it's true
    //  - there's no `performReload` flag, but it makes sense to do so
    let hasExplicitReloadFlag = "performReload" in options;
    if ((hasExplicitReloadFlag && options.performReload) ||
       (!hasExplicitReloadFlag && reload)) {
      this.onReload();
    }
  },

  /**
   * Disable or enable the cache via docShell.
   */
  _setCacheEnabled: function(allow) {
    let enable =  Ci.nsIRequest.LOAD_NORMAL;
    let disable = Ci.nsIRequest.LOAD_BYPASS_CACHE |
                  Ci.nsIRequest.INHIBIT_CACHING;
    if (this.docShell) {
      this.docShell.defaultLoadFlags = allow ? enable : disable;
    }
  },

  /**
   * Disable or enable JS via docShell.
   */
  _setJavascriptEnabled: function(allow) {
    if (this.docShell) {
      this.docShell.allowJavascript = allow;
    }
  },

  /**
   * Return cache allowed status.
   */
  _getCacheEnabled: function() {
    if (!this.docShell) {
      // The tab is already closed.
      return null;
    }

    let disable = Ci.nsIRequest.LOAD_BYPASS_CACHE |
                  Ci.nsIRequest.INHIBIT_CACHING;
    return this.docShell.defaultLoadFlags !== disable;
  },

  /**
   * Return JS allowed status.
   */
  _getJavascriptEnabled: function() {
    if (!this.docShell) {
      // The tab is already closed.
      return null;
    }

    return this.docShell.allowJavascript;
  },

  /**
   * Prepare to enter a nested event loop by disabling debuggee events.
   */
  preNest: function BTA_preNest() {
    if (!this.window) {
      // The tab is already closed.
      return;
    }
    let windowUtils = this.window
                          .QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindowUtils);
    windowUtils.suppressEventHandling(true);
    windowUtils.suspendTimeouts();
  },

  /**
   * Prepare to exit a nested event loop by enabling debuggee events.
   */
  postNest: function BTA_postNest(aNestData) {
    if (!this.window) {
      // The tab is already closed.
      return;
    }
    let windowUtils = this.window
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
   * Handle location changes, by clearing the previous debuggees and enabling
   * debugging, which may have been disabled temporarily by the
   * DebuggerProgressListener.
   */
  _windowReady: function (window) {
    let isTopLevel = window == this.window;

    events.emit(this, "window-ready", {
      window: window,
      isTopLevel: isTopLevel
    });

    // TODO bug 997119: move that code to ThreadActor by listening to window-ready
    let threadActor = this.threadActor;
    if (isTopLevel) {
      threadActor.clearDebuggees();
      if (threadActor.dbg) {
        threadActor.dbg.enabled = true;
        threadActor.global = window;
        threadActor.maybePauseOnExceptions();
      }
    }

    // Refresh the debuggee list when a new window object appears (top window or
    // iframe).
    if (threadActor.attached) {
      threadActor.findGlobals();
    }
  },

  _windowDestroyed: function (window) {
    events.emit(this, "window-destroyed", {
      window: window,
      isTopLevel: window == this.window
    });
  },

  /**
   * Start notifying server codebase and client about a new document
   * being loaded in the currently targeted context.
   */
  _willNavigate: function (window, newURI, request) {
    let isTopLevel = window == this.window;

    // will-navigate event needs to be dispatched synchronously,
    // by calling the listeners in the order or registration.
    // This event fires once navigation starts,
    // (all pending user prompts are dealt with),
    // but before the first request starts.
    events.emit(this, "will-navigate", {
      window: window,
      isTopLevel: isTopLevel,
      newURI: newURI,
      request: request
    });

    // We don't do anything for inner frames in TabActor.
    // (we will only update thread actor on window-ready)
    if (!isTopLevel) {
      return;
    }

    // Proceed normally only if the debuggee is not paused.
    // TODO bug 997119: move that code to ThreadActor by listening to will-navigate
    let threadActor = this.threadActor;
    if (request && threadActor.state == "paused") {
      request.suspend();
      threadActor.onResume();
      threadActor.dbg.enabled = false;
      this._pendingNavigation = request;
    }
    threadActor.disableAllBreakpoints();

    this.conn.send({
      from: this.actorID,
      type: "tabNavigated",
      url: newURI,
      nativeConsoleAPI: true,
      state: "start"
    });
  },

  /**
   * Notify server and client about a new document done loading in the current
   * targeted context.
   */
  _navigate: function (window) {
    let isTopLevel = window == this.window;

    // navigate event needs to be dispatched synchronously,
    // by calling the listeners in the order or registration.
    // This event is fired once the document is loaded,
    // after the load event, it's document ready-state is 'complete'.
    events.emit(this, "navigate", {
      window: window,
      isTopLevel: isTopLevel
    });

    // We don't do anything for inner frames in TabActor.
    // (we will only update thread actor on window-ready)
    if (!isTopLevel) {
      return;
    }

    // TODO bug 997119: move that code to ThreadActor by listening to navigate
    let threadActor = this.threadActor;
    if (threadActor.state == "running") {
      threadActor.dbg.enabled = true;
    }

    this.conn.send({
      from: this.actorID,
      type: "tabNavigated",
      url: this.url,
      title: this.title,
      nativeConsoleAPI: this.hasNativeConsoleAPI(this.window),
      state: "stop"
    });
  },

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
      // We are very explicitly examining the "console" property of
      // the non-Xrayed object here.
      let console = aWindow.wrappedJSObject.console;
      isNative = console instanceof aWindow.Console;
    }
    catch (ex) { }
    return isNative;
  }
};

/**
 * The request types this actor can handle.
 */
TabActor.prototype.requestTypes = {
  "attach": TabActor.prototype.onAttach,
  "detach": TabActor.prototype.onDetach,
  "reload": TabActor.prototype.onReload,
  "navigateTo": TabActor.prototype.onNavigateTo,
  "reconfigure": TabActor.prototype.onReconfigure
};

exports.TabActor = TabActor;

/**
 * Creates a tab actor for handling requests to a single in-process
 * <browser> tab. Most of the implementation comes from TabActor.
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
  TabActor.call(this, aConnection, aBrowser);
  this._browser = aBrowser;
  this._tabbrowser = aTabBrowser;
}

BrowserTabActor.prototype = Object.create(TabActor.prototype);

BrowserTabActor.prototype.constructor = BrowserTabActor;

Object.defineProperty(BrowserTabActor.prototype, "docShell", {
  get: function() {
    return this._browser.docShell;
  },
  enumerable: true,
  configurable: false
});

Object.defineProperty(BrowserTabActor.prototype, "title", {
  get: function() {
    let title = this.contentDocument.title || this._browser.contentTitle;
    // If contentTitle is empty (e.g. on a not-yet-restored tab), but there is a
    // tabbrowser (i.e. desktop Firefox, but not Fennec), we can use the label
    // as the title.
    if (!title && this._tabbrowser) {
      let tab = this._tabbrowser._getTabForContentWindow(this.window);
      if (tab) {
        title = tab.label;
      }
    }
    return title;
  },
  enumerable: true,
  configurable: false
});

Object.defineProperty(BrowserTabActor.prototype, "browser", {
  get: function() {
    return this._browser;
  },
  enumerable: true,
  configurable: false
});

BrowserTabActor.prototype.disconnect = function() {
  TabActor.prototype.disconnect.call(this);
  this._browser = null;
  this._tabbrowser = null;
};

BrowserTabActor.prototype.exit = function() {
  TabActor.prototype.exit.call(this);
  this._browser = null;
  this._tabbrowser = null;
};

exports.BrowserTabActor = BrowserTabActor;

/**
 * This actor is a shim that connects to a ContentActor in a remote
 * browser process. All RDP packets get forwarded using the message
 * manager.
 *
 * @param aConnection The main RDP connection.
 * @param aBrowser XUL <browser> element to connect to.
 */
function RemoteBrowserTabActor(aConnection, aBrowser)
{
  this._conn = aConnection;
  this._browser = aBrowser;
  this._form = null;
}

RemoteBrowserTabActor.prototype = {
  connect: function() {
    return DebuggerServer.connectToChild(this._conn, this._browser);
  },

  form: function() {
    return this._form;
  },

  exit: function() {
    this._browser = null;
  },
};

function BrowserAddonList(aConnection)
{
  this._connection = aConnection;
  this._actorByAddonId = new Map();
  this._onListChanged = null;
}

BrowserAddonList.prototype.getList = function() {
  var deferred = promise.defer();
  AddonManager.getAllAddons((addons) => {
    for (let addon of addons) {
      let actor = this._actorByAddonId.get(addon.id);
      if (!actor) {
        actor = new BrowserAddonActor(this._connection, addon);
        this._actorByAddonId.set(addon.id, actor);
      }
    }
    deferred.resolve([actor for ([_, actor] of this._actorByAddonId)]);
  });
  return deferred.promise;
}

Object.defineProperty(BrowserAddonList.prototype, "onListChanged", {
  enumerable: true, configurable: true,
  get: function() { return this._onListChanged; },
  set: function(v) {
    if (v !== null && typeof v != "function") {
      throw Error("onListChanged property may only be set to 'null' or a function");
    }
    this._onListChanged = v;
    if (this._onListChanged) {
      AddonManager.addAddonListener(this);
    } else {
      AddonManager.removeAddonListener(this);
    }
  }
});

BrowserAddonList.prototype.onInstalled = function (aAddon) {
  this._onListChanged();
};

BrowserAddonList.prototype.onUninstalled = function (aAddon) {
  this._actorByAddonId.delete(aAddon.id);
  this._onListChanged();
};

exports.BrowserAddonList = BrowserAddonList;

function BrowserAddonActor(aConnection, aAddon) {
  this.conn = aConnection;
  this._addon = aAddon;
  this._contextPool = new ActorPool(this.conn);
  this.conn.addActorPool(this._contextPool);
  this._threadActor = null;
  this._global = null;
  AddonManager.addAddonListener(this);
}

BrowserAddonActor.prototype = {
  actorPrefix: "addon",

  get exited() {
    return !this._addon;
  },

  get id() {
    return this._addon.id;
  },

  get url() {
    return this._addon.sourceURI ? this._addon.sourceURI.spec : undefined;
  },

  get attached() {
    return this._threadActor;
  },

  get global() {
    return this._global;
  },

  form: function BAA_form() {
    dbg_assert(this.actorID, "addon should have an actorID.");
    if (!this._consoleActor) {
      let {AddonConsoleActor} = require("devtools/server/actors/webconsole");
      this._consoleActor = new AddonConsoleActor(this._addon, this.conn, this);
      this._contextPool.addActor(this._consoleActor);
    }

    return {
      actor: this.actorID,
      id: this.id,
      name: this._addon.name,
      url: this.url,
      debuggable: this._addon.isDebuggable,
      consoleActor: this._consoleActor.actorID,
    };
  },

  disconnect: function BAA_disconnect() {
    this.conn.removeActorPool(this._contextPool);
    this._contextPool = null;
    this._consoleActor = null;
    this._addon = null;
    this._global = null;
    AddonManager.removeAddonListener(this);
  },

  setOptions: function BAA_setOptions(aOptions) {
    if ("global" in aOptions) {
      this._global = aOptions.global;
    }
  },

  onDisabled: function BAA_onDisabled(aAddon) {
    if (aAddon != this._addon) {
      return;
    }

    this._global = null;
  },

  onUninstalled: function BAA_onUninstalled(aAddon) {
    if (aAddon != this._addon) {
      return;
    }

    if (this.attached) {
      this.onDetach();
      this.conn.send({ from: this.actorID, type: "tabDetached" });
    }

    this.disconnect();
  },

  onAttach: function BAA_onAttach() {
    if (this.exited) {
      return { type: "exited" };
    }

    if (!this.attached) {
      this._threadActor = new AddonThreadActor(this.conn, this,
                                               this._addon.id);
      this._contextPool.addActor(this._threadActor);
    }

    return { type: "tabAttached", threadActor: this._threadActor.actorID };
  },

  onDetach: function BAA_onDetach() {
    if (!this.attached) {
      return { error: "wrongState" };
    }

    this._contextPool.remoteActor(this._threadActor);

    this._threadActor = null;

    return { type: "detached" };
  },

  preNest: function() {
    let e = Services.wm.getEnumerator(null);
    while (e.hasMoreElements()) {
      let win = e.getNext();
      let windowUtils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
      windowUtils.suppressEventHandling(true);
      windowUtils.suspendTimeouts();
    }
  },

  postNest: function() {
    let e = Services.wm.getEnumerator(null);
    while (e.hasMoreElements()) {
      let win = e.getNext();
      let windowUtils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
      windowUtils.resumeTimeouts();
      windowUtils.suppressEventHandling(false);
    }
  }
};

BrowserAddonActor.prototype.requestTypes = {
  "attach": BrowserAddonActor.prototype.onAttach,
  "detach": BrowserAddonActor.prototype.onDetach
};

/**
 * The DebuggerProgressListener object is an nsIWebProgressListener which
 * handles onStateChange events for the inspected browser. If the user tries to
 * navigate away from a paused page, the listener makes sure that the debuggee
 * is resumed before the navigation begins.
 *
 * @param TabActor aTabActor
 *        The tab actor associated with this listener.
 */
function DebuggerProgressListener(aTabActor) {
  this._tabActor = aTabActor;
  this._onWindowCreated = this.onWindowCreated.bind(this);
  this._onWindowHidden = this.onWindowHidden.bind(this);

  // Watch for windows destroyed (global observer that will need filtering)
  Services.obs.addObserver(this, "inner-window-destroyed", false);

  // XXX: for now we maintain the list of windows we know about in this instance
  // so that we can discriminate windows we care about when observing
  // inner-window-destroyed events. Bug 1016952 would remove the need for this.
  this._knownWindowIDs = new Map();
}

DebuggerProgressListener.prototype = {
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIWebProgressListener,
    Ci.nsISupportsWeakReference,
    Ci.nsISupports,
  ]),

  destroy: function() {
    Services.obs.removeObserver(this, "inner-window-destroyed", false);
    this._knownWindowIDs.clear();
    this._knownWindowIDs = null;
  },

  watch: function(docShell) {
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_STATUS |
                                          Ci.nsIWebProgress.NOTIFY_STATE_WINDOW |
                                          Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT);

    // TODO: fix docShell.chromeEventHandler in child processes!
    let handler = docShell.chromeEventHandler ||
                  docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIContentFrameMessageManager);

    handler.addEventListener("DOMWindowCreated", this._onWindowCreated, true);
    handler.addEventListener("pageshow", this._onWindowCreated, true);
    handler.addEventListener("pagehide", this._onWindowHidden, true);

    // Dispatch the _windowReady event on the tabActor for pre-existing windows
    for (let win of this._getWindowsInDocShell(docShell)) {
      this._tabActor._windowReady(win);
      this._knownWindowIDs.set(this._getWindowID(win), win);
    }
  },

  unwatch: function(docShell) {
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.removeProgressListener(this);

    // TODO: fix docShell.chromeEventHandler in child processes!
    let handler = docShell.chromeEventHandler ||
                  docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIContentFrameMessageManager);

    handler.removeEventListener("DOMWindowCreated", this._onWindowCreated, true);
    handler.removeEventListener("pageshow", this._onWindowCreated, true);
    handler.removeEventListener("pagehide", this._onWindowHidden, true);

    for (let win of this._getWindowsInDocShell(docShell)) {
      this._knownWindowIDs.delete(this._getWindowID(win));
    }
  },

  _getWindowsInDocShell: function(docShell) {
    let docShellsEnum = docShell.getDocShellEnumerator(
      Ci.nsIDocShellTreeItem.typeAll,
      Ci.nsIDocShell.ENUMERATE_FORWARDS
    );

    let windows = [];
    while (docShellsEnum.hasMoreElements()) {
      let w = docShellsEnum.getNext().QueryInterface(Ci.nsIInterfaceRequestor)
                                     .getInterface(Ci.nsIDOMWindow);
      windows.push(w);
    }
    return windows;
  },

  _getWindowID: function(window) {
    return window.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIDOMWindowUtils)
                 .currentInnerWindowID;
  },

  onWindowCreated: DevToolsUtils.makeInfallible(function(evt) {
    if (!this._tabActor.attached) {
      return;
    }

    // pageshow events for non-persisted pages have already been handled by a
    // prior DOMWindowCreated event. For persisted pages, act as if the window
    // had just been created since it's been unfrozen from bfcache.
    if (evt.type == "pageshow" && !evt.persisted) {
      return;
    }

    let window = evt.target.defaultView;
    this._tabActor._windowReady(window);

    if (evt.type !== "pageshow") {
      this._knownWindowIDs.set(this._getWindowID(window), window);
    }
  }, "DebuggerProgressListener.prototype.onWindowCreated"),

  onWindowHidden: DevToolsUtils.makeInfallible(function(evt) {
    if (!this._tabActor.attached) {
      return;
    }

    // Only act as if the window has been destroyed if the 'pagehide' event
    // was sent for a persisted window (persisted is set when the page is put
    // and frozen in the bfcache). If the page isn't persisted, the observer's
    // inner-window-destroyed event will handle it.
    if (!evt.persisted) {
      return;
    }

    let window = evt.target.defaultView;
    this._tabActor._windowDestroyed(window);
  }, "DebuggerProgressListener.prototype.onWindowHidden"),

  observe: DevToolsUtils.makeInfallible(function(subject, topic) {
    if (!this._tabActor.attached) {
      return;
    }

    // Because this observer will be called for all inner-window-destroyed in
    // the application, we need to filter out events for windows we are not
    // watching
    let innerID = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
    let window = this._knownWindowIDs.get(innerID);
    if (window) {
      this._knownWindowIDs.delete(innerID);
      this._tabActor._windowDestroyed(window);
    }
  }, "DebuggerProgressListener.prototype.observe"),

  onStateChange:
  DevToolsUtils.makeInfallible(function(aProgress, aRequest, aFlag, aStatus) {
    if (!this._tabActor.attached) {
      return;
    }

    let isStart = aFlag & Ci.nsIWebProgressListener.STATE_START;
    let isStop = aFlag & Ci.nsIWebProgressListener.STATE_STOP;
    let isDocument = aFlag & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT;
    let isWindow = aFlag & Ci.nsIWebProgressListener.STATE_IS_WINDOW;

    let window = aProgress.DOMWindow;
    if (isDocument && isStart) {
      let newURI = aRequest instanceof Ci.nsIChannel ? aRequest.URI.spec : null;
      this._tabActor._willNavigate(window, newURI, aRequest);
    }
    if (isWindow && isStop) {
      this._tabActor._navigate(window);
    }
  }, "DebuggerProgressListener.prototype.onStateChange")
};

exports.register = function(handle) {
  handle.setRootActor(createRootActor);
};

exports.unregister = function(handle) {
  handle.setRootActor(null);
};
