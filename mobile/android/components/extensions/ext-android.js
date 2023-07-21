/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * NOTE: If you change the globals in this file, you must check if the globals
 * list in mobile/android/.eslintrc.js also needs updating.
 */

ChromeUtils.defineESModuleGetters(this, {
  GeckoViewTabBridge: "resource://gre/modules/GeckoViewTab.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  mobileWindowTracker: "resource://gre/modules/GeckoViewWebExtension.sys.mjs",
});

var { EventDispatcher } = ChromeUtils.importESModule(
  "resource://gre/modules/Messaging.sys.mjs"
);

var { ExtensionCommon } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionCommon.sys.mjs"
);
var { ExtensionUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionUtils.sys.mjs"
);

var { DefaultWeakMap, ExtensionError } = ExtensionUtils;

var { defineLazyGetter } = ExtensionCommon;

const BrowserStatusFilter = Components.Constructor(
  "@mozilla.org/appshell/component/browser-status-filter;1",
  "nsIWebProgress",
  "addProgressListener"
);

const WINDOW_TYPE = "navigator:geckoview";

// We need let to break cyclic dependency
/* eslint-disable-next-line prefer-const */
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
    const window = this.browser.ownerGlobal;
    // GeckoView windows can become popups at any moment, so we need to check
    // here
    if (!windowTracker.isBrowserWindow(window)) {
      return;
    }

    this.delegate("onLocationChange", webProgress, request, locationURI, flags);
  }
  onStateChange(webProgress, request, stateFlags, status) {
    this.delegate("onStateChange", webProgress, request, stateFlags, status);
  }
}

const PROGRESS_LISTENER_FLAGS =
  Ci.nsIWebProgress.NOTIFY_STATE_ALL | Ci.nsIWebProgress.NOTIFY_LOCATION;

class ProgressListenerWrapper {
  constructor(window, listener) {
    this.listener = new BrowserProgressListener(
      window.browser,
      listener,
      PROGRESS_LISTENER_FLAGS
    );
  }

  destroy() {
    this.listener.destroy();
  }
}

class WindowTracker extends WindowTrackerBase {
  constructor(...args) {
    super(...args);

    this.progressListeners = new DefaultWeakMap(() => new WeakMap());
  }

  getCurrentWindow(context) {
    // In GeckoView the popup is on a separate window so getCurrentWindow for
    // the popup should return whatever is the topWindow.
    // TODO: Bug 1651506 use context?.viewType === "popup" instead
    if (context?.currentWindow?.moduleManager.settings.isPopup) {
      return this.topWindow;
    }
    return super.getCurrentWindow(context);
  }

  get topWindow() {
    return mobileWindowTracker.topWindow;
  }

  get topNonPBWindow() {
    return mobileWindowTracker.topNonPBWindow;
  }

  isBrowserWindow(window) {
    const { documentElement } = window.document;
    return documentElement.getAttribute("windowtype") === WINDOW_TYPE;
  }

  addProgressListener(window, listener) {
    const listeners = this.progressListeners.get(window);
    if (!listeners.has(listener)) {
      const wrapper = new ProgressListenerWrapper(window, listener);
      listeners.set(listener, wrapper);
    }
  }

  removeProgressListener(window, listener) {
    const listeners = this.progressListeners.get(window);
    const wrapper = listeners.get(listener);
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
 * @param {Function} listener
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
      const listener2 = {
        onEvent(event, data, callback) {
          listener(fire, data);
        },
      };

      EventDispatcher.instance.registerListener(listener2, [event]);
      return () => {
        EventDispatcher.instance.unregisterListener(listener2, [event]);
      };
    },
  }).api();
};

class TabTracker extends TabTrackerBase {
  init() {
    if (this.initialized) {
      return;
    }
    this.initialized = true;

    windowTracker.addOpenListener(window => {
      const nativeTab = window.tab;
      this.emit("tab-created", { nativeTab });
    });

    windowTracker.addCloseListener(window => {
      const { tab: nativeTab, browser } = window;
      const { windowId, tabId } = this.getBrowserData(browser);
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
    const window = windowTracker.getWindow(windowId, null, false);

    if (window) {
      const { tab } = window;
      if (tab) {
        return tab;
      }
    }

    if (default_ !== undefined) {
      return default_;
    }
    throw new ExtensionError(`Invalid tab ID: ${id}`);
  }

  getBrowserData(browser) {
    const window = browser.ownerGlobal;
    const tab = window?.tab;
    if (!tab) {
      return {
        tabId: -1,
        windowId: -1,
      };
    }

    const windowId = windowTracker.getId(window);

    if (!windowTracker.isBrowserWindow(window)) {
      return {
        windowId,
        tabId: -1,
      };
    }

    return {
      windowId,
      tabId: this.getId(tab),
    };
  }

  get activeTab() {
    const window = windowTracker.topWindow;
    if (window) {
      return window.tab;
    }
    return null;
  }
}

windowTracker = new WindowTracker();
const tabTracker = new TabTracker();

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
    return 0;
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
    return this.nativeTab.getActive();
  }

  get highlighted() {
    return this.active;
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

  get autoDiscardable() {
    // This property reflects whether the browser is allowed to auto-discard.
    // Since extensions cannot do so on Android, we return true here.
    return true;
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

    windowTracker.addListener("progress", this);

    this.getDefaultPrototype = getDefaultPrototype;
    this.tabData = new Map();
  }

  onLocationChange(browser, webProgress, request, locationURI, flags) {
    if (!webProgress.isTopLevel) {
      // Only pageAction and browserAction are consuming the "location-change" event
      // to update their per-tab status, and they should only do so in response of
      // location changes related to the top level frame (See Bug 1493470 for a rationale).
      return;
    }
    const { tab } = browser.ownerGlobal;
    // fromBrowse will be false in case of e.g. a hash change or history.pushState
    const fromBrowse = !(
      flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT
    );
    this.emit(
      "location-change",
      {
        id: tab.id,
        linkedBrowser: browser,
        // TODO: we don't support selected so we just alway say we are
        selected: true,
      },
      fromBrowse
    );
  }

  get(tabId) {
    if (!this.tabData.has(tabId)) {
      const data = Object.create(this.getDefaultPrototype(tabId));
      this.tabData.set(tabId, data);
    }

    return this.tabData.get(tabId);
  }

  clear(tabId) {
    this.tabData.delete(tabId);
  }

  shutdown() {
    windowTracker.removeListener("progress", this);
  }
}

class Window extends WindowBase {
  get focused() {
    return this.window.document.hasFocus();
  }

  isCurrentFor(context) {
    // In GeckoView the popup is on a separate window so the current window for
    // the popup is whatever is the topWindow.
    // TODO: Bug 1651506 use context?.viewType === "popup" instead
    if (context?.currentWindow?.moduleManager.settings.isPopup) {
      return mobileWindowTracker.topWindow == this.window;
    }
    return super.isCurrentFor(context);
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
    yield this.activeTab;
  }

  *getHighlightedTabs() {
    yield this.activeTab;
  }

  get activeTab() {
    const { tabManager } = this.extension;
    return tabManager.getWrapper(this.window.tab);
  }

  getTabAtIndex(index) {
    if (index == 0) {
      return this.activeTab;
    }
  }
}

Object.assign(global, { Tab, TabContext, Window });

class TabManager extends TabManagerBase {
  get(tabId, default_ = undefined) {
    const nativeTab = tabTracker.getTab(tabId, default_);

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
    const window = windowTracker.getWindow(windowId, context);

    return this.getWrapper(window);
  }

  *getAll(context) {
    for (const window of windowTracker.browserWindows()) {
      if (!this.canAccessWindow(window, context)) {
        continue;
      }
      const wrapped = this.getWrapper(window);
      if (wrapped) {
        yield wrapped;
      }
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

/* eslint-disable mozilla/balanced-listeners */
extensions.on("page-shutdown", (type, context) => {
  if (context.viewType == "tab") {
    const window = context.xulBrowser.ownerGlobal;
    if (!windowTracker.isBrowserWindow(window)) {
      // Content in non-browser window, e.g. ContentPage in xpcshell uses
      // chrome://extensions/content/dummy.xhtml as the window.
      return;
    }
    GeckoViewTabBridge.closeTab({
      window,
      extensionId: context.extension.id,
    });
  }
});
/* eslint-enable mozilla/balanced-listeners */

global.openOptionsPage = async extension => {
  const { options_ui } = extension.manifest;
  const extensionId = extension.id;

  if (options_ui.open_in_tab) {
    // Delegate new tab creation and open the options page in the new tab.
    const tab = await GeckoViewTabBridge.createNewTab({
      extensionId,
      createProperties: {
        url: options_ui.page,
        active: true,
      },
    });

    const { browser } = tab;
    const flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;

    browser.fixupAndLoadURIString(options_ui.page, {
      flags,
      triggeringPrincipal: extension.principal,
    });

    const newWindow = browser.ownerGlobal;
    mobileWindowTracker.setTabActive(newWindow, true);
    return;
  }

  // Delegate option page handling to the app.
  return GeckoViewTabBridge.openOptionsPage(extensionId);
};
