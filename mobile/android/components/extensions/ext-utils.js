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
  SingletonEventManager,
  defineLazyGetter,
} = ExtensionUtils;

global.GlobalEventDispatcher = EventDispatcher.instance;

const BrowserStatusFilter = Components.Constructor(
  "@mozilla.org/appshell/component/browser-status-filter;1", "nsIWebProgress",
  "addProgressListener");

let tabTracker;
let windowTracker;

class BrowserProgressListener {
  constructor(browser, listener, flags) {
    this.listener = listener;
    this.browser = browser;
    this.filter = new BrowserStatusFilter(this, flags);
  }

  destroy() {
    this.filter.removeProgressListener(this);
  }

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

  onProgressChange(webProgress, request, curSelfProgress, maxSelfProgress, curTotalProgress, maxTotalProgress) {}
  onStatusChange(webProgress, request, status, message) {}
  onSecurityChange(webProgress, request, state) {}
}

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

  destroy() {
    this.window.BrowserApp.deck.removeEventListener("TabOpen", this);

    for (let nativeTab of this.window.BrowserApp.tabs) {
      this.removeProgressListener(nativeTab.browser);
    }
  }

  addBrowserProgressListener(browser) {
    this.removeProgressListener(browser);

    let listener = new BrowserProgressListener(browser, this.listener, this.flags);
    this.listeners.set(browser, listener);

    browser.addProgressListener(listener.filter, this.flags);
  }

  removeProgressListener(browser) {
    let listener = this.listeners.get(browser);
    if (listener) {
      browser.removeProgressListener(listener.filter);
      listener.destroy();
      this.listeners.delete(browser);
    }
  }

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
global.GlobalEventManager = class extends SingletonEventManager {
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

global.WindowEventManager = class extends SingletonEventManager {
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

  emitCreated(nativeTab) {
    this.emit("tab-created", {nativeTab});
  }

  emitRemoved(nativeTab, isWindowClosing) {
    let windowId = windowTracker.getId(nativeTab.browser.ownerGlobal);
    let tabId = this.getId(nativeTab);

    Services.tm.mainThread.dispatch(() => {
      this.emit("tab-removed", {nativeTab, tabId, windowId, isWindowClosing});
    }, Ci.nsIThread.DISPATCH_NORMAL);
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

Object.assign(global, {Tab, Window});

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
