/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "GeckoViewTabBridge",
  "resource://gre/modules/GeckoViewTab.jsm"
);

/* globals EventDispatcher */
var { EventDispatcher } = ChromeUtils.import(
  "resource://gre/modules/Messaging.jsm"
);

var { ExtensionCommon } = ChromeUtils.import(
  "resource://gre/modules/ExtensionCommon.jsm"
);
var { ExtensionUtils } = ChromeUtils.import(
  "resource://gre/modules/ExtensionUtils.jsm"
);

var { DefaultWeakMap, ExtensionError } = ExtensionUtils;

var { defineLazyGetter } = ExtensionCommon;

global.GlobalEventDispatcher = EventDispatcher.instance;

const BrowserStatusFilter = Components.Constructor(
  "@mozilla.org/appshell/component/browser-status-filter;1",
  "nsIWebProgress",
  "addProgressListener"
);

const WINDOW_TYPE = Services.androidBridge.isFennec
  ? "navigator:browser"
  : "navigator:geckoview";

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

const PROGRESS_LISTENER_FLAGS =
  Ci.nsIWebProgress.NOTIFY_STATE_ALL | Ci.nsIWebProgress.NOTIFY_LOCATION;

class GeckoViewProgressListenerWrapper {
  constructor(window, listener) {
    this.listener = new BrowserProgressListener(
      window.BrowserApp.selectedBrowser,
      listener,
      PROGRESS_LISTENER_FLAGS
    );
  }

  destroy() {
    this.listener.destroy();
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
class FennecProgressListenerWrapper {
  constructor(window, listener) {
    this.window = window;
    this.listener = listener;
    this.listeners = new WeakMap();

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

    let listener = new BrowserProgressListener(
      browser,
      this.listener,
      this.flags
    );
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

const ProgressListenerWrapper = Services.androidBridge.isFennec
  ? FennecProgressListenerWrapper
  : GeckoViewProgressListenerWrapper;

class WindowTracker extends WindowTrackerBase {
  constructor(...args) {
    super(...args);

    this.progressListeners = new DefaultWeakMap(() => new WeakMap());
  }

  get topWindow() {
    return Services.wm.getMostRecentWindow(WINDOW_TYPE);
  }

  get topNonPBWindow() {
    return Services.wm.getMostRecentNonPBWindow(WINDOW_TYPE);
  }

  isBrowserWindow(window) {
    let { documentElement } = window.document;
    return documentElement.getAttribute("windowtype") === WINDOW_TYPE;
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
 * Helper to create an event manager which listens for an event in the Android
 * global EventDispatcher, and calls the given listener function whenever the
 * event is received. That listener function receives a `fire` object,
 * which it can use to dispatch events to the extension, and an object
 * detailing the EventDispatcher event that was received.
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
 *
 * @returns {object} An injectable api for the new event.
 */
global.makeGlobalEvent = function makeGlobalEvent(
  context,
  name,
  event,
  listener
) {
  return new EventManager({
    context,
    name,
    register: fire => {
      let listener2 = {
        onEvent(event, data, callback) {
          listener(fire, data);
        },
      };

      GlobalEventDispatcher.registerListener(listener2, [event]);
      return () => {
        GlobalEventDispatcher.unregisterListener(listener2, [event]);
      };
    },
  }).api();
};

class GeckoViewTabTracker extends TabTrackerBase {
  init() {
    if (this.initialized) {
      return;
    }
    this.initialized = true;

    windowTracker.addOpenListener(window => {
      const nativeTab = window.BrowserApp.selectedTab;
      this.emit("tab-created", { nativeTab });
    });

    windowTracker.addCloseListener(window => {
      const nativeTab = window.BrowserApp.selectedTab;
      const { windowId, tabId } = this.getBrowserData(
        window.BrowserApp.selectedBrowser
      );
      this.emit("tab-removed", {
        nativeTab,
        tabId,
        windowId,
        // In GeckoView, it is not meaningful to speak of "window closed", because a tab is a window.
        // Until we have a meaningful way to group tabs (and close multiple tabs at once),
        // let's use isWindowClosing: false
        isWindowClosing: false,
      });
    });
  }

  getId(nativeTab) {
    return nativeTab.id;
  }

  getTab(id, default_ = undefined) {
    const windowId = GeckoViewTabBridge.tabIdToWindowId(id);
    const win = windowTracker.getWindow(windowId, null, false);

    if (win && win.BrowserApp) {
      let nativeTab = win.BrowserApp.selectedTab;
      if (nativeTab) {
        return nativeTab;
      }
    }

    if (default_ !== undefined) {
      return default_;
    }
    throw new ExtensionError(`Invalid tab ID: ${id}`);
  }

  getBrowserData(browser) {
    const window = browser.ownerGlobal;
    if (!window.BrowserApp) {
      return {
        tabId: -1,
        windowId: -1,
      };
    }
    return {
      tabId: this.getId(window.BrowserApp.selectedTab),
      windowId: windowTracker.getId(window),
    };
  }

  get activeTab() {
    let win = windowTracker.topWindow;
    if (win && win.BrowserApp) {
      return win.BrowserApp.selectedTab;
    }
    return null;
  }
}

class FennecTabTracker extends TabTrackerBase {
  constructor() {
    super();

    // Keep track of the extension popup tab.
    this._extensionPopupTabWeak = null;
    // Keep track of the selected tabId
    this._selectedTabId = null;
  }

  init() {
    if (this.initialized) {
      return;
    }
    this.initialized = true;

    windowTracker.addListener("TabClose", this);
    windowTracker.addListener("TabOpen", this);

    // Register a listener for the Tab:Selected global event,
    // so that we can close the popup when a popup tab has been
    // unselected.
    GlobalEventDispatcher.registerListener(this, ["Tab:Selected"]);
  }

  /**
   * Returns the currently opened popup tab if any
   */
  get extensionPopupTab() {
    if (this._extensionPopupTabWeak) {
      const tab = this._extensionPopupTabWeak.get();

      // Return the native tab only if the tab has not been removed in the meantime.
      if (tab.browser) {
        return tab;
      }

      // Clear the tracked popup tab if it has been closed in the meantime.
      this._extensionPopupTabWeak = null;
    }

    return undefined;
  }

  /**
   * Open a pageAction/browserAction popup url in a tab and keep track of
   * its weak reference (to be able to customize the activedTab using the tab parentId,
   * to skip it in the tabs.query and to set the parent tab as active when the popup
   * tab is currently selected).
   *
   * @param {string} popup
   *   The popup url to open in a tab.
   */
  openExtensionPopupTab(popup) {
    let win = windowTracker.topWindow;
    if (!win) {
      throw new ExtensionError(
        `Unable to open a popup without an active window`
      );
    }

    if (this.extensionPopupTab) {
      win.BrowserApp.closeTab(this.extensionPopupTab);
    }

    this.init();

    let { browser, id } = win.BrowserApp.selectedTab;
    let isPrivate = PrivateBrowsingUtils.isBrowserPrivate(browser);
    this._extensionPopupTabWeak = Cu.getWeakReference(
      win.BrowserApp.addTab(popup, {
        selected: true,
        parentId: id,
        isPrivate,
      })
    );
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
    const { BrowserApp } = event.target.ownerGlobal;
    const nativeTab = BrowserApp.getTabForBrowser(event.target);

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
   * Required by the GlobalEventDispatcher module. This event will get
   * called whenever one of the registered listeners fires.
   * @param {string} event The event which fired.
   * @param {object} data Information about the event which fired.
   */
  onEvent(event, data) {
    const { BrowserApp } = windowTracker.topWindow;

    switch (event) {
      case "Tab:Selected": {
        this._selectedTabId = data.id;

        // If a new tab has been selected while an extension popup tab is still open,
        // close it immediately.
        const nativeTab = BrowserApp.getTabForId(data.id);

        const popupTab = tabTracker.extensionPopupTab;
        if (popupTab && popupTab !== nativeTab) {
          BrowserApp.closeTab(popupTab);
        }

        break;
      }
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
    this.emit("tab-created", { nativeTab });
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

    if (this.extensionPopupTab && this.extensionPopupTab === nativeTab) {
      this._extensionPopupTabWeak = null;

      // Do not switch to the parent tab of the extension popup tab
      // if the popup tab is not the selected tab.
      if (this._selectedTabId !== tabId) {
        return;
      }

      // Select the parent tab when the closed tab was an extension popup tab.
      const { BrowserApp } = windowTracker.topWindow;
      const popupParentTab = BrowserApp.getTabForId(nativeTab.parentId);
      if (popupParentTab) {
        BrowserApp.selectTab(popupParentTab);
      }
    }

    Services.tm.dispatchToMainThread(() => {
      this.emit("tab-removed", { nativeTab, tabId, windowId, isWindowClosing });
    });
  }

  getBrowserData(browser) {
    let result = {
      tabId: -1,
      windowId: -1,
    };

    let { BrowserApp } = browser.ownerGlobal;
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
    let win = windowTracker.topWindow;
    if (win && win.BrowserApp) {
      const selectedTab = win.BrowserApp.selectedTab;

      // If the current tab is an extension popup tab, we use the parentId to retrieve
      // and return the tab that was selected when the popup tab has been opened.
      if (selectedTab === this.extensionPopupTab) {
        return win.BrowserApp.getTabForId(selectedTab.parentId);
      }

      return selectedTab;
    }

    return null;
  }
}

windowTracker = new WindowTracker();
if (Services.androidBridge.isFennec) {
  tabTracker = new FennecTabTracker();
} else {
  tabTracker = new GeckoViewTabTracker();
}

Object.assign(global, { tabTracker, windowTracker });

class Tab extends TabBase {
  get _favIconUrl() {
    return undefined;
  }

  get attention() {
    return false;
  }

  get audible() {
    return this.nativeTab.playingAudio;
  }

  get browser() {
    return this.nativeTab.browser;
  }

  get discarded() {
    return this.browser.getAttribute("pending") === "true";
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
    return { muted: false };
  }

  get lastAccessed() {
    return this.nativeTab.lastTouchedAt;
  }

  get pinned() {
    return false;
  }

  get active() {
    // If there is an extension popup tab and it is active,
    // then the parent tab of the extension popup tab is active
    // (while the extension popup tab will not be included in the
    // tabs.query results).
    if (tabTracker.extensionPopupTab) {
      if (
        tabTracker.extensionPopupTab.getActive() &&
        this.nativeTab.id === tabTracker.extensionPopupTab.parentId
      ) {
        return true;
      }

      // Never return true for an active extension popup, e.g. so that
      // the popup tab will not be part of the results of querying
      // all the active tabs.
      if (tabTracker.extensionPopupTab === this.nativeTab) {
        return false;
      }
    }
    return this.nativeTab.getActive();
  }

  get highlighted() {
    return this.active;
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

  get successorTabId() {
    return -1;
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

  // TODO: Just return false for these until properly implemented on Android.
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1402924
  get isArticle() {
    return false;
  }

  get isInReaderMode() {
    return false;
  }

  get hidden() {
    return false;
  }

  get sharingState() {
    return {
      screen: undefined,
      microphone: false,
      camera: false,
    };
  }
}

// Manages tab-specific context data and dispatches tab select and close events.
class TabContext extends EventEmitter {
  constructor(getDefaultPrototype) {
    super();

    this.getDefaultPrototype = getDefaultPrototype;
    this.tabData = new Map();

    GlobalEventDispatcher.registerListener(this, [
      "Tab:Selected",
      "Tab:Closed",
    ]);
  }

  get(tabId) {
    if (!this.tabData.has(tabId)) {
      let data = Object.create(this.getDefaultPrototype(tabId));
      this.tabData.set(tabId, data);
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

  *getTabs() {
    let { tabManager } = this.extension;

    for (let nativeTab of this.window.BrowserApp.tabs) {
      yield tabManager.getWrapper(nativeTab);
    }
  }

  *getHighlightedTabs() {
    yield this.activeTab;
  }

  get activeTab() {
    let { BrowserApp } = this.window;
    let { selectedTab } = BrowserApp;

    // If the current tab is an extension popup tab, we use the parentId to retrieve
    // and return the tab that was selected when the popup tab has been opened.
    if (selectedTab === tabTracker.extensionPopupTab) {
      selectedTab = BrowserApp.getTabForId(selectedTab.parentId);
    }

    let { tabManager } = this.extension;
    return tabManager.getWrapper(selectedTab);
  }

  getTabAtIndex(index) {
    let nativeTab = this.window.BrowserApp.tabs[index];
    if (nativeTab) {
      return this.extension.tabManager.getWrapper(nativeTab);
    }
  }
}

Object.assign(global, { Tab, TabContext, Window });

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

  canAccessTab(nativeTab) {
    return (
      this.extension.privateBrowsingAllowed ||
      !PrivateBrowsingUtils.isBrowserPrivate(nativeTab.browser)
    );
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

  *getAll() {
    for (let window of windowTracker.browserWindows()) {
      yield this.getWrapper(window);
    }
  }

  wrapWindow(window) {
    return new Window(this.extension, window, windowTracker.getId(window));
  }
}

// eslint-disable-next-line mozilla/balanced-listeners
extensions.on("startup", (type, extension) => {
  defineLazyGetter(extension, "tabManager", () => new TabManager(extension));
  defineLazyGetter(
    extension,
    "windowManager",
    () => new WindowManager(extension)
  );
});
