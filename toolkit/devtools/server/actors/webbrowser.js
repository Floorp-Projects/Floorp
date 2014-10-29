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
let makeDebugger = require("./utils/make-debugger");
let mapURIToAddonID = require("./utils/map-uri-to-addon-id");

let {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager", "resource://gre/modules/AddonManager.jsm");

// Assumptions on events module:
// events needs to be dispatched synchronously,
// by calling the listeners in the order or registration.
XPCOMUtils.defineLazyGetter(this, "events", () => {
  return require("sdk/event/core");
});

XPCOMUtils.defineLazyGetter(this, "StyleSheetActor", () => {
  return require("devtools/server/actors/stylesheets").StyleSheetActor;
});

function getWindowID(window) {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIDOMWindowUtils)
               .currentInnerWindowID;
}

/**
 * Browser-specific actors.
 */

function getInnerId(window) {
  return window.QueryInterface(Ci.nsIInterfaceRequestor).
                getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;
};

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
 * Unwrap a global that is wrapped in a |Debugger.Object|, or if the global has
 * become a dead object, return |undefined|.
 *
 * @param Debugger.Object wrappedGlobal
 *        The |Debugger.Object| which wraps a global.
 *
 * @returns {Object|undefined}
 *          Returns the unwrapped global object or |undefined| if unwrapping
 *          failed.
 */
const unwrapDebuggerObjectGlobal = wrappedGlobal => {
  try {
    // Because of bug 991399 we sometimes get nuked window references here. We
    // just bail out in that case.
    //
    // Note that addon sandboxes have a DOMWindow as their prototype. So make
    // sure that we can touch the prototype too (whatever it is), in case _it_
    // is it a nuked window reference. We force stringification to make sure
    // that any dead object proxies make themselves known.
    let global = wrappedGlobal.unsafeDereference();
    Object.getPrototypeOf(global) + "";
    return global;
  }
  catch (e) {
    return undefined;
  }
};

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

/**
 * Produces an iterable (in this case a generator) to enumerate all available
 * browser tabs.
 */
BrowserTabList.prototype._getBrowsers = function*() {
  // Iterate over all navigator:browser XUL windows.
  for (let win of allAppShellDOMWindows(DebuggerServer.chromeWindowType)) {
    // For each tab in this XUL window, ensure that we have an actor for
    // it, reusing existing actors where possible. We actually iterate
    // over 'browser' XUL elements, and BrowserTabActor uses
    // browser.contentWindow as the debuggee global.
    for (let browser of this._getChildren(win)) {
      yield browser;
    }
  }
};

BrowserTabList.prototype._getChildren = function(aWindow) {
  return aWindow.gBrowser ? aWindow.gBrowser.browsers : [];
};

BrowserTabList.prototype._isRemoteBrowser = function(browser) {
  return browser.getAttribute("remote");
};

BrowserTabList.prototype.getList = function() {
  let topXULWindow = Services.wm.getMostRecentWindow(DebuggerServer.chromeWindowType);
  let selectedBrowser = null;
  if (topXULWindow) {
    selectedBrowser = this._getSelectedBrowser(topXULWindow);
  }

  // As a sanity check, make sure all the actors presently in our map get
  // picked up when we iterate over all windows' tabs.
  let initialMapSize = this._actorByBrowser.size;
  let foundCount = 0;

  // To avoid mysterious behavior if tabs are closed or opened mid-iteration,
  // we update the map first, and then make a second pass over it to yield
  // the actors. Thus, the sequence yielded is always a snapshot of the
  // actors that were live when we began the iteration.

  let actorPromises = [];

  for (let browser of this._getBrowsers()) {
    // Do we have an existing actor for this browser? If not, create one.
    let actor = this._actorByBrowser.get(browser);
    if (actor) {
      actorPromises.push(actor.update());
      foundCount++;
    } else if (this._isRemoteBrowser(browser)) {
      actor = new RemoteBrowserTabActor(this._connection, browser);
      this._actorByBrowser.set(browser, actor);
      actorPromises.push(actor.connect());
    } else {
      actor = new BrowserTabActor(this._connection, browser,
                                  browser.getTabBrowser());
      this._actorByBrowser.set(browser, actor);
      actorPromises.push(promise.resolve(actor));
    }

    // Set the 'selected' properties on all actors correctly.
    actor.selected = browser === selectedBrowser;
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

  // Map of DOM stylesheets to StyleSheetActors
  this._styleSheetActors = new Map();

  this._shouldAddNewGlobalAsDebuggee = this._shouldAddNewGlobalAsDebuggee.bind(this);

  this.makeDebugger = makeDebugger.bind(null, {
    findDebuggees: () => this.windows,
    shouldAddNewGlobalAsDebuggee: this._shouldAddNewGlobalAsDebuggee
  });

  this.traits = { reconfigure: true, frames: true };
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
      let docShell = docShellsEnum.getNext();
      docShell.QueryInterface(Ci.nsIInterfaceRequestor)
              .getInterface(Ci.nsIWebProgress);
      docShells.push(docShell);
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
   * Getter for the original docShell the tabActor got attached to in the first
   * place.
   * Note that your actor should normally *not* rely on this top level docShell
   * if you want it to show information relative to the iframe that's currently
   * being inspected in the toolbox.
   */
  get originalDocShell() {
    if (!this._originalWindow) {
      return this.docShell;
    }

    return this._originalWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIWebNavigation)
                               .QueryInterface(Ci.nsIDocShell);
  },

  /**
   * Getter for the original window the tabActor got attached to in the first
   * place.
   * Note that your actor should normally *not* rely on this top level window if
   * you want it to show information relative to the iframe that's currently
   * being inspected in the toolbox.
   */
  get originalWindow() {
    return this._originalWindow || this.window;
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

  /**
   * This is called by BrowserTabList.getList for existing tab actors prior to
   * calling |form| below.  It can be used to do any async work that may be
   * needed to assemble the form.
   */
  update: function() {
    return promise.resolve(this);
  },

  form: function BTA_form() {
    dbg_assert(!this.exited,
               "form() shouldn't be called on exited browser actor.");
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
    this._styleSheetActors.clear();
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

  /**
   * Return true if the given global is associated with this tab and should be
   * added as a debuggee, false otherwise.
   */
  _shouldAddNewGlobalAsDebuggee: function (wrappedGlobal) {
    if (wrappedGlobal.hostAnnotations &&
        wrappedGlobal.hostAnnotations.type == "document" &&
        wrappedGlobal.hostAnnotations.element === this.window) {
      return true;
    }

    let global = unwrapDebuggerObjectGlobal(wrappedGlobal);
    if (!global) {
      return false;
    }

    // Check if the global is a sdk page-mod sandbox.
    let metadata = {};
    let id = "";
    try {
      id = getInnerId(this.window);
      metadata = Cu.getSandboxMetadata(global);
    }
    catch (e) {}
    if (metadata
        && metadata["inner-window-id"]
        && metadata["inner-window-id"] == id) {
      return true;
    }

    return false;
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

    // Save references to the original document we attached to
    this._originalWindow = this.window;

    // Ensure replying to attach() request first
    // before notifying about new docshells.
    DevToolsUtils.executeSoon(() => this._watchDocshells());

    this._attached = true;
  },

  _watchDocshells: function BTA_watchDocshells() {
    // In child processes, we watch all docshells living in the process.
    if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
      Services.obs.addObserver(this, "webnavigation-create", false);
    }
    Services.obs.addObserver(this, "webnavigation-destroy", false);

    // We watch for all child docshells under the current document,
    this._progressListener.watch(this.docShell);

    // And list all already existing ones.
    this._updateChildDocShells();
  },

  onSwitchToFrame: function BTA_onSwitchToFrame(aRequest) {
    let windowId = aRequest.windowId;
    let win;
    try {
      win = Services.wm.getOuterWindowWithId(windowId);
    } catch(e) {}
    if (!win) {
      return { error: "noWindow",
               message: "The related docshell is destroyed or not found" };
    } else if (win == this.window) {
      return {};
    }

    // Reply first before changing the document
    DevToolsUtils.executeSoon(() => this._changeTopLevelDocument(win));

    return {};
  },

  onListFrames: function BTA_onListFrames(aRequest) {
    let windows = this._docShellsToWindows(this.docShells);
    return { frames: windows };
  },

  observe: function (aSubject, aTopic, aData) {
    // Ignore any event that comes before/after the tab actor is attached
    // That typically happens during firefox shutdown.
    if (!this.attached) {
      return;
    }
    if (aTopic == "webnavigation-create") {
      aSubject.QueryInterface(Ci.nsIDocShell);
      // webnavigation-create is fired very early during docshell construction.
      // In new root docshells within child processes, involving TabChild,
      // this event is from within this call:
      //   http://hg.mozilla.org/mozilla-central/annotate/74d7fb43bb44/dom/ipc/TabChild.cpp#l912
      // whereas the chromeEventHandler (and most likely other stuff) is set later:
      //   http://hg.mozilla.org/mozilla-central/annotate/74d7fb43bb44/dom/ipc/TabChild.cpp#l944
      // So wait a tick before watching it:
      DevToolsUtils.executeSoon(() => {
        // In child processes, we have new root docshells,
        // let's watch them and all their child docshells.
        if (this._isRootDocShell(aSubject)) {
          this._progressListener.watch(aSubject);
        }
        this._notifyDocShellsUpdate([aSubject]);
      });
    } else if (aTopic == "webnavigation-destroy") {
      let webProgress = aSubject.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIWebProgress);
      this._notifyDocShellDestroy(webProgress);
    }
  },

  _isRootDocShell: function (docShell) {
    // Root docshells like top level xul windows don't have chromeEventHandler.
    // Root docshells in child processes have one, it is TabChildGlobal,
    // which isn't a DOM Element.
    // Non-root docshell have a chromeEventHandler that is either
    // xul:iframe, xul:browser or html:iframe.
    return !docShell.chromeEventHandler ||
           !(docShell.chromeEventHandler instanceof Ci.nsIDOMElement);
  },

  // Convert docShell list to windows objects list being sent to the client
  _docShellsToWindows: function (docshells) {
    return docshells.map(docShell => {
      let window = docShell.DOMWindow;
      let id = window.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDOMWindowUtils)
                     .outerWindowID;
      let parentID = undefined;
      // Ignore the parent of the original document on non-e10s firefox,
      // as we get the xul window as parent and don't care about it.
      if (window.parent && window != this._originalWindow) {
        parentID = window.parent
                         .QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindowUtils)
                         .outerWindowID;
      }
      return {
        id: id,
        url: window.location.href,
        title: window.document.title,
        parentID: parentID
      };
    });
  },

  _notifyDocShellsUpdate: function (docshells) {
    let windows = this._docShellsToWindows(docshells);
    this.conn.send({ from: this.actorID,
                     type: "frameUpdate",
                     frames: windows
                   });
  },

  _updateChildDocShells: function () {
    this._notifyDocShellsUpdate(this.docShells);
  },

  _notifyDocShellDestroy: function (webProgress) {
    let id = webProgress.DOMWindow
                        .QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIDOMWindowUtils)
                        .outerWindowID;
    this.conn.send({ from: this.actorID,
                     type: "frameUpdate",
                     frames: [{
                       id: id,
                       destroy: true
                     }]
                   });

    // Stop watching this docshell if it's a root one.
    // (child processes spawn new root docshells)
    webProgress.QueryInterface(Ci.nsIDocShell);
    if (this._isRootDocShell(webProgress)) {
      this._progressListener.unwatch(webProgress);
    }

    if (webProgress.DOMWindow == this._originalWindow) {
      // If for some reason (typically during Firefox shutdown), the original
      // document is destroyed, we detach the tab actor to unregister all listeners
      // and prevent any exception.
      this.exit();
      return;
    }

    // If the currently targeted context is destroyed,
    // and we aren't on the top-level document,
    // we have to switch to the top-level one.
    if (webProgress.DOMWindow == this.window &&
        this.window != this._originalWindow) {
      this._changeTopLevelDocument(this._originalWindow);
    }
  },

  _notifyDocShellDestroyAll: function () {
    this.conn.send({ from: this.actorID,
                     type: "frameUpdate",
                     destroyAll: true
                   });
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
    this._originalWindow = null;

    // Removes the observers being set in _watchDocShells
    if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
      Services.obs.removeObserver(this, "webnavigation-create", false);
    }
    Services.obs.removeObserver(this, "webnavigation-destroy", false);

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
      cacheDisabled: this._getCacheDisabled(),
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
      // This won't work while the browser is shutting down and we don't really
      // care.
      if (Services.startup.shuttingDown) {
        return;
      }
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
    if (typeof options.cacheDisabled !== "undefined" &&
        options.cacheDisabled !== this._getCacheDisabled()) {
      this._setCacheDisabled(options.cacheDisabled);
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
  _setCacheDisabled: function(disabled) {
    let enable =  Ci.nsIRequest.LOAD_NORMAL;
    let disable = Ci.nsIRequest.LOAD_BYPASS_CACHE |
                  Ci.nsIRequest.INHIBIT_CACHING;

    if (this.docShell) {
      this.docShell.defaultLoadFlags = disabled ? disable : enable;
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
  _getCacheDisabled: function() {
    if (!this.docShell) {
      // The tab is already closed.
      return null;
    }

    let disable = Ci.nsIRequest.LOAD_BYPASS_CACHE |
                  Ci.nsIRequest.INHIBIT_CACHING;
    return this.docShell.defaultLoadFlags === disable;
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

  _changeTopLevelDocument: function (window) {
    // Fake a will-navigate on the previous document
    // to let a chance to unregister it
    this._willNavigate(this.window, window.location.href, null, true);

    this._windowDestroyed(this.window, null, true);

    DevToolsUtils.executeSoon(() => {
      this._setWindow(window);

      // Then fake window-ready and navigate on the given document
      this._windowReady(window, true);
      DevToolsUtils.executeSoon(() => {
        this._navigate(window, true);
      });
    });
  },

  _setWindow: function (window) {
    let docShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIWebNavigation)
                         .QueryInterface(Ci.nsIDocShell);
    // Here is the very important call where we switch the currently
    // targeted context (it will indirectly update this.window and
    // many other attributes defined from docShell).
    Object.defineProperty(this, "docShell", {
      value: docShell,
      enumerable: true,
      configurable: true
    });
    events.emit(this, "changed-toplevel-document");
    let id = window.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindowUtils)
                   .outerWindowID;
    this.conn.send({ from: this.actorID,
                     type: "frameUpdate",
                     selected: id
                   });
  },

  /**
   * Handle location changes, by clearing the previous debuggees and enabling
   * debugging, which may have been disabled temporarily by the
   * DebuggerProgressListener.
   */
  _windowReady: function (window, isFrameSwitching = false) {
    let isTopLevel = window == this.window;

    // We just reset iframe list on WillNavigate, so we now list all existing
    // frames when we load a new document in the original window
    if (window == this._originalWindow && !isFrameSwitching) {
      this._updateChildDocShells();
    }

    events.emit(this, "window-ready", {
      window: window,
      isTopLevel: isTopLevel,
      id: getWindowID(window)
    });

    // TODO bug 997119: move that code to ThreadActor by listening to window-ready
    let threadActor = this.threadActor;
    if (isTopLevel) {
      threadActor.clearDebuggees();
      if (threadActor.dbg) {
        threadActor.dbg.enabled = true;
        threadActor.maybePauseOnExceptions();
      }
      // Update the global no matter if the debugger is on or off,
      // otherwise the global will be wrong when enabled later.
      threadActor.global = window;
    }

    for (let sheetActor of this._styleSheetActors.values()) {
      this._tabPool.removeActor(sheetActor);
    }
    this._styleSheetActors.clear();


    // Refresh the debuggee list when a new window object appears (top window or
    // iframe).
    if (threadActor.attached) {
      threadActor.dbg.addDebuggees();
    }
  },

  _windowDestroyed: function (window, id = null, isFrozen = false) {
    events.emit(this, "window-destroyed", {
      window: window,
      isTopLevel: window == this.window,
      id: id || getWindowID(window),
      isFrozen: isFrozen
    });
  },

  /**
   * Start notifying server and client about a new document
   * being loaded in the currently targeted context.
   */
  _willNavigate: function (window, newURI, request, isFrameSwitching = false) {
    let isTopLevel = window == this.window;
    let reset = false;

    if (window == this._originalWindow && !isFrameSwitching) {
      // Clear the iframe list if the original top-level document changes.
      this._notifyDocShellDestroyAll();

      // If the top level document changes and we are targeting
      // an iframe, we need to reset to the upcoming new top level document.
      // But for this will-navigate event, we will dispatch on the old window.
      // (The inspector codebase expect to receive will-navigate for the currently
      // displayed document in order to cleanup the markup view)
      if (this.window != this._originalWindow) {
        reset=true;
        window = this.window;
        isTopLevel = true;
      }
    }

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
      state: "start",
      isFrameSwitching: isFrameSwitching
    });

    if (reset) {
      this._setWindow(this._originalWindow);
    }
  },

  /**
   * Notify server and client about a new document done loading in the current
   * targeted context.
   */
  _navigate: function (window, isFrameSwitching = false) {
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
      state: "stop",
      isFrameSwitching: isFrameSwitching
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
  },

  /**
   * Create or return the StyleSheetActor for a style sheet. This method
   * is here because the Style Editor and Inspector share style sheet actors.
   *
   * @param DOMStyleSheet styleSheet
   *        The style sheet to creat an actor for.
   * @return StyleSheetActor actor
   *         The actor for this style sheet.
   *
   */
  createStyleSheetActor: function BTA_createStyleSheetActor(styleSheet) {
    if (this._styleSheetActors.has(styleSheet)) {
      return this._styleSheetActors.get(styleSheet);
    }
    let actor = new StyleSheetActor(styleSheet, this);
    this._styleSheetActors.set(styleSheet, actor);

    this._tabPool.addActor(actor);

    return actor;
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
  "reconfigure": TabActor.prototype.onReconfigure,
  "switchToFrame": TabActor.prototype.onSwitchToFrame,
  "listFrames": TabActor.prototype.onListFrames
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
    if (this._browser) {
      return this._browser.docShell;
    }
    // The tab is closed.
    return null;
  },
  enumerable: true,
  configurable: true
});

Object.defineProperty(BrowserTabActor.prototype, "title", {
  get: function() {
    // On Fennec, we can check the session store data for zombie tabs
    if (this._browser.__SS_restore) {
      let sessionStore = this._browser.__SS_data;
      // Get the last selected entry
      let entry = sessionStore.entries[sessionStore.index - 1];
      return entry.title;
    }
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

Object.defineProperty(BrowserTabActor.prototype, "url", {
  get: function() {
    // On Fennec, we can check the session store data for zombie tabs
    if (this._browser.__SS_restore) {
      let sessionStore = this._browser.__SS_data;
      // Get the last selected entry
      let entry = sessionStore.entries[sessionStore.index - 1];
      return entry.url;
    }
    if (this.webNavigation.currentURI) {
      return this.webNavigation.currentURI.spec;
    }
    return null;
  },
  enumerable: true,
  configurable: true
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
    let connect = DebuggerServer.connectToChild(this._conn, this._browser);
    return connect.then(form => {
      this._form = form;
      return this;
    });
  },

  get _mm() {
    return this._browser.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader
           .messageManager;
  },

  update: function() {
    let deferred = promise.defer();
    let onFormUpdate = msg => {
      this._mm.removeMessageListener("debug:form", onFormUpdate);
      this._form = msg.json;
      deferred.resolve(this);
    };
    this._mm.addMessageListener("debug:form", onFormUpdate);
    this._mm.sendAsyncMessage("debug:form");
    return deferred.promise;
  },

  form: function() {
    return this._form;
  },

  exit: function() {
    this._browser = null;
  },
};

exports.RemoteBrowserTabActor = RemoteBrowserTabActor;

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

  this._shouldAddNewGlobalAsDebuggee = this._shouldAddNewGlobalAsDebuggee.bind(this);

  this.makeDebugger = makeDebugger.bind(null, {
    findDebuggees: this._findDebuggees.bind(this),
    shouldAddNewGlobalAsDebuggee: this._shouldAddNewGlobalAsDebuggee
  });

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
      this._threadActor = new AddonThreadActor(this.conn, this);
      this._contextPool.addActor(this._threadActor);
    }

    return { type: "tabAttached", threadActor: this._threadActor.actorID };
  },

  onDetach: function BAA_onDetach() {
    if (!this.attached) {
      return { error: "wrongState" };
    }

    this._contextPool.removeActor(this._threadActor);

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
  },

  /**
   * Return true if the given global is associated with this addon and should be
   * added as a debuggee, false otherwise.
   */
  _shouldAddNewGlobalAsDebuggee: function (aGlobal) {
    const global = unwrapDebuggerObjectGlobal(aGlobal);
    try {
      // This will fail for non-Sandbox objects, hence the try-catch block.
      let metadata = Cu.getSandboxMetadata(global);
      if (metadata) {
        return metadata.addonID === this.id;
      }
    } catch (e) {}

    if (global instanceof Ci.nsIDOMWindow) {
      let id = {};
      if (mapURIToAddonID(global.document.documentURIObject, id)) {
        return id.value === this.id;
      }
      return false;
    }

    // Check the global for a __URI__ property and then try to map that to an
    // add-on
    let uridescriptor = aGlobal.getOwnPropertyDescriptor("__URI__");
    if (uridescriptor && "value" in uridescriptor && uridescriptor.value) {
      let uri;
      try {
        uri = Services.io.newURI(uridescriptor.value, null, null);
      }
      catch (e) {
        DevToolsUtils.reportException(
          "BrowserAddonActor.prototype._shouldAddNewGlobalAsDebuggee",
          new Error("Invalid URI: " + uridescriptor.value)
        );
        return false;
      }

      let id = {};
      if (mapURIToAddonID(uri, id)) {
        return id.value === this.id;
      }
    }

    return false;
  },

  /**
   * Yield the current set of globals associated with this addon that should be
   * added as debuggees.
   */
  _findDebuggees: function (dbg) {
    return dbg.findAllGlobals().filter(this._shouldAddNewGlobalAsDebuggee);
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
      this._knownWindowIDs.set(getWindowID(win), win);
    }
  },

  unwatch: function(docShell) {
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    // During process shutdown, the docshell may already be cleaned up and throw
    try {
      webProgress.removeProgressListener(this);
    } catch(e) {}

    // TODO: fix docShell.chromeEventHandler in child processes!
    let handler = docShell.chromeEventHandler ||
                  docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIContentFrameMessageManager);

    handler.removeEventListener("DOMWindowCreated", this._onWindowCreated, true);
    handler.removeEventListener("pageshow", this._onWindowCreated, true);
    handler.removeEventListener("pagehide", this._onWindowHidden, true);

    for (let win of this._getWindowsInDocShell(docShell)) {
      this._knownWindowIDs.delete(getWindowID(win));
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
      this._knownWindowIDs.set(getWindowID(window), window);
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
    this._tabActor._windowDestroyed(window, null, true);
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
      this._tabActor._windowDestroyed(window, innerID);
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

    // Catch any iframe location change
    if (isDocument && isStop) {
      // Watch document stop to ensure having the new iframe url.
      aProgress.QueryInterface(Ci.nsIDocShell);
      this._tabActor._notifyDocShellsUpdate([aProgress]);
    }

    let window = aProgress.DOMWindow;
    if (isDocument && isStart) {
      // One of the earliest events that tells us a new URI
      // is being loaded in this window.
      let newURI = aRequest instanceof Ci.nsIChannel ? aRequest.URI.spec : null;
      this._tabActor._willNavigate(window, newURI, aRequest);
    }
    if (isWindow && isStop) {
      // Somewhat equivalent of load event.
      // (window.document.readyState == complete)
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
