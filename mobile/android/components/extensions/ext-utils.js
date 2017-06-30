/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");

/* globals TabBase, WindowBase, TabTrackerBase, WindowTrackerBase, TabManagerBase, WindowManagerBase */
Cu.import("resource://gre/modules/ExtensionTabs.jsm");
/* globals EventDispatcher */
Cu.import("resource://gre/modules/Messaging.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");

var {
  DefaultWeakMap,
  ExtensionError,
  defineLazyGetter,
} = ExtensionUtils;

global.GlobalEventDispatcher = EventDispatcher.instance;

const BrowserStatusFilter = Components.Constructor(
  "@mozilla.org/appshell/component/browser-status-filter;1", "nsIWebProgress",
  "addProgressListener");

let tabTracker;
let windowTracker;

/**
 * A nsIWebProgressListener for a specific XUL browser, which delegates the
 * events that it receives to a tab progress listener, and prepends the browser
 * to their arguments list.
 *
 * @param {XULElement} browser
 *        A XUL browser element.
 * @param {object} listener
 *        A tab progress listener object.
 * @param {integer} flags
 *        The web progress notification flags with which to filter events.
 */
class BrowserProgressListener {
  constructor(browser, listener, flags) {
    this.listener = listener;
    this.browser = browser;
    this.filter = new BrowserStatusFilter(this, flags);
    this.browser.addProgressListener(this.filter, flags);
  }

  /**
   * Destroy the listener, and perform any necessary cleanup.
   */
  destroy() {
    this.browser.removeProgressListener(this.filter);
    this.filter.removeProgressListener(this);
  }

  /**
   * Calls the appropriate listener in the wrapped tab progress listener, with
   * the wrapped XUL browser object as its first argument, and the additional
   * arguments in `args`.
   *
   * @param {string} method
   *        The name of the nsIWebProgressListener method which is being
   *        delegated.
   * @param {*} args
   *        The arguments to pass to the delegated listener.
   * @private
   */
  delegate(method, ...args) {
    if (this.listener[method]) {
      this.listener[method](this.browser, ...args);
    }
  }

  onLocationChange(webProgress, request, locationURI, flags) {
    this.delegate("onLocationChange", webProgress, request, locationURI, flags);
  }
  onStateChange(webProgress, request, stateFlags, status) {
    this.delegate("onStateChange", webProgress, request, stateFlags, status);
  }
}

/**
 * Handles wrapping a tab progress listener in browser-specific
 * BrowserProgressListener instances, an attaching them to each tab in a given
 * browser window.
 *
 * @param {DOMWindow} window
 *        The browser window to which to attach the listeners.
 * @param {object} listener
 *        The tab progress listener to wrap.
 */
class ProgressListenerWrapper {
  constructor(window, listener) {
    this.window = window;
    this.listener = listener;
    this.listeners = new WeakMap();

    this.flags = Ci.nsIWebProgress.NOTIFY_STATE_ALL |
                 Ci.nsIWebProgress.NOTIFY_LOCATION;

    for (let nativeTab of this.window.BrowserApp.tabs) {
      this.addBrowserProgressListener(nativeTab.browser);
    }

    this.window.BrowserApp.deck.addEventListener("TabOpen", this);
  }

  /**
   * Destroy the wrapper, removing any remaining listeners it has added.
   */
  destroy() {
    this.window.BrowserApp.deck.removeEventListener("TabOpen", this);

    for (let nativeTab of this.window.BrowserApp.tabs) {
      this.removeProgressListener(nativeTab.browser);
    }
  }

  /**
   * Adds a progress listener to the given XUL browser element.
   *
   * @param {XULElement} browser
   *        The XUL browser to add the listener to.
   * @private
   */
  addBrowserProgressListener(browser) {
    this.removeProgressListener(browser);

    let listener = new BrowserProgressListener(browser, this.listener, this.flags);
    this.listeners.set(browser, listener);
  }

  /**
   * Removes a progress listener from the given XUL browser element.
   *
   * @param {XULElement} browser
   *        The XUL browser to remove the listener from.
   * @private
   */
  removeProgressListener(browser) {
    let listener = this.listeners.get(browser);
    if (listener) {
      listener.destroy();
      this.listeners.delete(browser);
    }
  }

  /**
   * Handles tab open events, and adds the necessary progress listeners to the
   * new tabs.
   *
   * @param {Event} event
   *        The DOM event to handle.
   * @private
   */
  handleEvent(event) {
    if (event.type === "TabOpen") {
      this.addBrowserProgressListener(event.originalTarget);
    }
  }

}

class WindowTracker extends WindowTrackerBase {
  constructor(...args) {
    super(...args);

    this.progressListeners = new DefaultWeakMap(() => new WeakMap());
  }

  addProgressListener(window, listener) {
    let listeners = this.progressListeners.get(window);
    if (!listeners.has(listener)) {
      let wrapper = new ProgressListenerWrapper(window, listener);
      listeners.set(listener, wrapper);
    }
  }

  removeProgressListener(window, listener) {
    let listeners = this.progressListeners.get(window);
    let wrapper = listeners.get(listener);
    if (wrapper) {
      wrapper.destroy();
      listeners.delete(listener);
    }
  }
}

/**
 * An event manager API provider which listens for an event in the Android
 * global EventDispatcher, and calls the given listener function whenever an event
 * is received. That listener function receives a `fire` object, which it can
 * use to dispatch events to the extension, and an object detailing the
 * EventDispatcher event that was received.
 *
 * @param {BaseContext} context
 *        The extension context which the event manager belongs to.
 * @param {string} name
 *        The API name of the event manager, e.g.,"runtime.onMessage".
 * @param {string} event
 *        The name of the EventDispatcher event to listen for.
 * @param {function} listener
 *        The listener function to call when an EventDispatcher event is
 *        recieved.
 */
global.GlobalEventManager = class extends EventManager {
  constructor(context, name, event, listener) {
    super(context, name, fire => {
      let listener2 = {
        onEvent(event, data, callback) {
          listener(fire, data);
        },
      };

      GlobalEventDispatcher.registerListener(listener2, [event]);
      return () => {
        GlobalEventDispatcher.unregisterListener(listener2, [event]);
      };
    });
  }
};

/**
 * An event manager API provider which listens for a DOM event in any browser
 * window, and calls the given listener function whenever an event is received.
 * That listener function receives a `fire` object, which it can use to dispatch
 * events to the extension, and a DOM event object.
 *
 * @param {BaseContext} context
 *        The extension context which the event manager belongs to.
 * @param {string} name
 *        The API name of the event manager, e.g.,"runtime.onMessage".
 * @param {string} event
 *        The name of the DOM event to listen for.
 * @param {function} listener
 *        The listener function to call when a DOM event is received.
 */
global.WindowEventManager = class extends EventManager {
  constructor(context, name, event, listener) {
    super(context, name, fire => {
      let listener2 = listener.bind(null, fire);

      windowTracker.addListener(event, listener2);
      return () => {
        windowTracker.removeListener(event, listener2);
      };
    });
  }
};

class TabTracker extends TabTrackerBase {
  init() {
    if (this.initialized) {
      return;
    }
    this.initialized = true;

    windowTracker.addListener("TabClose", this);
    windowTracker.addListener("TabOpen", this);
  }

  getId(nativeTab) {
    return nativeTab.id;
  }

  getTab(id, default_ = undefined) {
    let win = windowTracker.topWindow;
    if (win) {
      let nativeTab = win.BrowserApp.getTabForId(id);
      if (nativeTab) {
        return nativeTab;
      }
    }
    if (default_ !== undefined) {
      return default_;
    }
    throw new ExtensionError(`Invalid tab ID: ${id}`);
  }

  /**
   * Handles tab open and close events, and emits the appropriate internal
   * events for them.
   *
   * @param {Event} event
   *        A DOM event to handle.
   * @private
   */
  handleEvent(event) {
    const {BrowserApp} = event.target.ownerGlobal;
    let nativeTab = BrowserApp.getTabForBrowser(event.target);

    switch (event.type) {
      case "TabOpen":
        this.emitCreated(nativeTab);
        break;

      case "TabClose":
        this.emitRemoved(nativeTab, false);
        break;
    }
  }

  /**
   * Emits a "tab-created" event for the given tab element.
   *
   * @param {NativeTab} nativeTab
   *        The tab element which is being created.
   * @private
   */
  emitCreated(nativeTab) {
    this.emit("tab-created", {nativeTab});
  }

  /**
   * Emits a "tab-removed" event for the given tab element.
   *
   * @param {NativeTab} nativeTab
   *        The tab element which is being removed.
   * @param {boolean} isWindowClosing
   *        True if the tab is being removed because the browser window is
   *        closing.
   * @private
   */
  emitRemoved(nativeTab, isWindowClosing) {
    let windowId = windowTracker.getId(nativeTab.browser.ownerGlobal);
    let tabId = this.getId(nativeTab);

    Services.tm.dispatchToMainThread(() => {
      this.emit("tab-removed", {nativeTab, tabId, windowId, isWindowClosing});
    });
  }

  getBrowserData(browser) {
    let result = {
      tabId: -1,
      windowId: -1,
    };

    let {BrowserApp} = browser.ownerGlobal;
    if (BrowserApp) {
      result.windowId = windowTracker.getId(browser.ownerGlobal);

      let nativeTab = BrowserApp.getTabForBrowser(browser);
      if (nativeTab) {
        result.tabId = this.getId(nativeTab);
      }
    }

    return result;
  }

  get activeTab() {
    let window = windowTracker.topWindow;
    if (window && window.BrowserApp) {
      return window.BrowserApp.selectedTab;
    }
    return null;
  }
}

windowTracker = new WindowTracker();
tabTracker = new TabTracker();

Object.assign(global, {tabTracker, windowTracker});

class Tab extends TabBase {
  get _favIconUrl() {
    return undefined;
  }

  get audible() {
    return this.nativeTab.playingAudio;
  }

  get browser() {
    return this.nativeTab.browser;
  }

  get cookieStoreId() {
    return getCookieStoreIdForTab(this, this.nativeTab);
  }

  get height() {
    return this.browser.clientHeight;
  }

  get incognito() {
    return PrivateBrowsingUtils.isBrowserPrivate(this.browser);
  }

  get index() {
    return this.window.BrowserApp.tabs.indexOf(this.nativeTab);
  }

  get mutedInfo() {
    return {muted: false};
  }

  get pinned() {
    return false;
  }

  get active() {
    return this.nativeTab.getActive();
  }

  get selected() {
    return this.nativeTab.getActive();
  }

  get status() {
    if (this.browser.webProgress.isLoadingDocument) {
      return "loading";
    }
    return "complete";
  }

  get width() {
    return this.browser.clientWidth;
  }

  get window() {
    return this.browser.ownerGlobal;
  }

  get windowId() {
    return windowTracker.getId(this.window);
  }
}

// Manages tab-specific context data and dispatches tab select and close events.
class TabContext {
  constructor(getDefaults, extension) {
    this.extension = extension;
    this.getDefaults = getDefaults;
    this.tabData = new Map();

    GlobalEventDispatcher.registerListener(this, [
      "Tab:Selected",
      "Tab:Closed",
    ]);

    EventEmitter.decorate(this);
  }

  get(tabId) {
    if (!this.tabData.has(tabId)) {
      this.tabData.set(tabId, this.getDefaults());
    }

    return this.tabData.get(tabId);
  }

  clear(tabId) {
    this.tabData.delete(tabId);
  }

  /**
   * Required by the GlobalEventDispatcher module. This event will get
   * called whenever one of the registered listeners fires.
   * @param {string} event The event which fired.
   * @param {object} data Information about the event which fired.
   */
  onEvent(event, data) {
    switch (event) {
      case "Tab:Selected":
        this.emit("tab-selected", data.id);
        break;
      case "Tab:Closed":
        this.emit("tab-closed", data.tabId);
        break;
    }
  }

  shutdown() {
    GlobalEventDispatcher.unregisterListener(this, [
      "Tab:Selected",
      "Tab:Closed",
    ]);
  }
}

class Window extends WindowBase {
  get focused() {
    return this.window.document.hasFocus();
  }

  get top() {
    return this.window.screenY;
  }

  get left() {
    return this.window.screenX;
  }

  get width() {
    return this.window.outerWidth;
  }

  get height() {
    return this.window.outerHeight;
  }

  get incognito() {
    return PrivateBrowsingUtils.isWindowPrivate(this.window);
  }

  get alwaysOnTop() {
    return false;
  }

  get isLastFocused() {
    return this.window === windowTracker.topWindow;
  }

  get state() {
    return "fullscreen";
  }

  * getTabs() {
    let {tabManager} = this.extension;

    for (let nativeTab of this.window.BrowserApp.tabs) {
      yield tabManager.getWrapper(nativeTab);
    }
  }
}

Object.assign(global, {Tab, TabContext, Window});

class TabManager extends TabManagerBase {
  get(tabId, default_ = undefined) {
    let nativeTab = tabTracker.getTab(tabId, default_);

    if (nativeTab) {
      return this.getWrapper(nativeTab);
    }
    return default_;
  }

  addActiveTabPermission(nativeTab = tabTracker.activeTab) {
    return super.addActiveTabPermission(nativeTab);
  }

  revokeActiveTabPermission(nativeTab = tabTracker.activeTab) {
    return super.revokeActiveTabPermission(nativeTab);
  }

  wrapTab(nativeTab) {
    return new Tab(this.extension, nativeTab, nativeTab.id);
  }
}

class WindowManager extends WindowManagerBase {
  get(windowId, context) {
    let window = windowTracker.getWindow(windowId, context);

    return this.getWrapper(window);
  }

  * getAll() {
    for (let window of windowTracker.browserWindows()) {
      yield this.getWrapper(window);
    }
  }

  wrapWindow(window) {
    return new Window(this.extension, window, windowTracker.getId(window));
  }
}


extensions.on("startup", (type, extension) => { // eslint-disable-line mozilla/balanced-listeners
  defineLazyGetter(extension, "tabManager",
                   () => new TabManager(extension));
  defineLazyGetter(extension, "windowManager",
                   () => new WindowManager(extension));
});
