/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

/* exported TabTrackerBase, TabManagerBase, TabBase, WindowTrackerBase, WindowManagerBase, WindowBase */

var EXPORTED_SYMBOLS = ["TabTrackerBase", "TabManagerBase", "TabBase", "WindowTrackerBase", "WindowManagerBase", "WindowBase"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");

const {
  DefaultMap,
  DefaultWeakMap,
  EventEmitter,
  ExtensionError,
} = ExtensionUtils;

class TabBase {
  constructor(extension, tab, id) {
    this.extension = extension;
    this.tabManager = extension.tabManager;
    this.id = id;
    this.tab = tab;
    this.activeTabWindowId = null;
  }

  get innerWindowId() {
    return this.browser.innerWindowId;
  }

  get hasTabPermission() {
    return this.extension.hasPermission("tabs") || this.hasActiveTabPermission;
  }

  get hasActiveTabPermission() {
    return (this.extension.hasPermission("activeTab") &&
            this.activeTabWindowId !== null &&
            this.activeTabWindowId === this.innerWindowId);
  }

  get incognito() {
    return PrivateBrowsingUtils.isBrowserPrivate(this.browser);
  }

  get _url() {
    return this.browser.currentURI.spec;
  }

  get url() {
    if (this.hasTabPermission) {
      return this._url;
    }
  }

  get uri() {
    if (this.hasTabPermission) {
      return this.browser.currentURI;
    }
  }

  get _title() {
    return this.browser.contentTitle || this.tab.label;
  }


  get title() {
    if (this.hasTabPermission) {
      return this._title;
    }
  }

  get favIconUrl() {
    if (this.hasTabPermission) {
      return this._favIconUrl;
    }
  }

  matches(queryInfo) {
    const PROPS = ["active", "audible", "cookieStoreId", "highlighted", "index", "pinned", "status", "title"];

    if (PROPS.some(prop => queryInfo[prop] !== null && queryInfo[prop] !== this[prop])) {
      return false;
    }

    if (queryInfo.muted !== null) {
      if (queryInfo.muted !== this.mutedInfo.muted) {
        return false;
      }
    }

    if (queryInfo.url && !queryInfo.url.matches(this.uri)) {
      return false;
    }

    return true;
  }

  convert() {
    let result = {
      id: this.id,
      index: this.index,
      windowId: this.windowId,
      selected: this.selected,
      highlighted: this.selected,
      active: this.selected,
      pinned: this.pinned,
      status: this.status,
      incognito: this.incognito,
      width: this.width,
      height: this.height,
      audible: this.audible,
      mutedInfo: this.mutedInfo,
    };

    if (this.extension.hasPermission("cookies")) {
      result.cookieStoreId = this.cookieStoreId;
    }

    if (this.hasTabPermission) {
      for (let prop of ["url", "title", "favIconUrl"]) {
        // We use the underscored variants here to avoid the redundant
        // permissions checks imposed on the public properties.
        let val = this[`_${prop}`];
        if (val) {
          result[prop] = val;
        }
      }
    }

    return result;
  }

  _execute(context, details, kind, method) {
    let options = {
      js: [],
      css: [],
      remove_css: method == "removeCSS",
    };

    // We require a `code` or a `file` property, but we can't accept both.
    if ((details.code === null) == (details.file === null)) {
      return Promise.reject({message: `${method} requires either a 'code' or a 'file' property, but not both`});
    }

    if (details.frameId !== null && details.allFrames) {
      return Promise.reject({message: `'frameId' and 'allFrames' are mutually exclusive`});
    }

    if (this.hasActiveTabPermission) {
      // If we have the "activeTab" permission for this tab, ignore
      // the host whitelist.
      options.matchesHost = ["<all_urls>"];
    } else {
      options.matchesHost = this.extension.whiteListedHosts.serialize();
    }

    if (details.code !== null) {
      options[`${kind}Code`] = details.code;
    }
    if (details.file !== null) {
      let url = context.uri.resolve(details.file);
      if (!this.extension.isExtensionURL(url)) {
        return Promise.reject({message: "Files to be injected must be within the extension"});
      }
      options[kind].push(url);
    }
    if (details.allFrames) {
      options.all_frames = details.allFrames;
    }
    if (details.frameId !== null) {
      options.frame_id = details.frameId;
    }
    if (details.matchAboutBlank) {
      options.match_about_blank = details.matchAboutBlank;
    }
    if (details.runAt !== null) {
      options.run_at = details.runAt;
    } else {
      options.run_at = "document_idle";
    }
    if (details.cssOrigin !== null) {
      options.css_origin = details.cssOrigin;
    } else {
      options.css_origin = "author";
    }

    let {browser} = this;
    let recipient = {
      innerWindowID: browser.innerWindowID,
    };

    return context.sendMessage(browser.messageManager, "Extension:Execute", {options}, {recipient});
  }

  executeScript(context, details) {
    return this._execute(context, details, "js", "executeScript");
  }

  insertCSS(context, details) {
    return this._execute(context, details, "css", "insertCSS").then(() => {});
  }

  removeCSS(context, details) {
    return this._execute(context, details, "css", "removeCSS").then(() => {});
  }
}

// Note: These must match the values in windows.json.
const WINDOW_ID_NONE = -1;
const WINDOW_ID_CURRENT = -2;

class WindowBase {
  constructor(extension, window, id) {
    this.extension = extension;
    this.window = window;
    this.id = id;
  }

  get xulWindow() {
    return this.window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIDocShell)
               .treeOwner.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIXULWindow);
  }

  isCurrentFor(context) {
    if (context && context.currentWindow) {
      return this.window === context.currentWindow;
    }
    return this.isLastFocused;
  }

  get type() {
    let {chromeFlags} = this.xulWindow;

    if (chromeFlags & Ci.nsIWebBrowserChrome.CHROME_OPENAS_DIALOG) {
      return "popup";
    }

    return "normal";
  }

  convert(getInfo) {
    let result = {
      id: this.id,
      focused: this.focused,
      top: this.top,
      left: this.left,
      width: this.width,
      height: this.height,
      incognito: this.incognito,
      type: this.type,
      state: this.state,
      alwaysOnTop: this.alwaysOnTop,
    };

    if (getInfo && getInfo.populate) {
      result.tabs = Array.from(this.getTabs(), tab => tab.convert());
    }

    return result;
  }

  matches(queryInfo, context) {
    if (queryInfo.lastFocusedWindow !== null && queryInfo.lastFocusedWindow !== this.isLastFocused) {
      return false;
    }

    if (queryInfo.windowType !== null && queryInfo.windowType !== this.type) {
      return false;
    }

    if (queryInfo.windowId !== null) {
      if (queryInfo.windowId === WINDOW_ID_CURRENT) {
        if (!this.isCurrentFor(context)) {
          return false;
        }
      } else if (queryInfo.windowId !== this.id) {
        return false;
      }
    }

    if (queryInfo.currentWindow !== null && queryInfo.currentWindow !== this.isCurrentFor(context)) {
      return false;
    }

    return true;
  }
}

Object.assign(WindowBase, {WINDOW_ID_NONE, WINDOW_ID_CURRENT});

class TabTrackerBase extends EventEmitter {
  on(...args) {
    if (!this.initialized) {
      this.init();
    }

    return super.on(...args); // eslint-disable-line mozilla/balanced-listeners
  }
}

class StatusListener {
  constructor(listener) {
    this.listener = listener;
  }

  onStateChange(browser, webProgress, request, stateFlags, statusCode) {
    if (!webProgress.isTopLevel) {
      return;
    }

    let status;
    if (stateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW) {
      if (stateFlags & Ci.nsIWebProgressListener.STATE_START) {
        status = "loading";
      } else if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
        status = "complete";
      }
    } else if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
               statusCode == Cr.NS_BINDING_ABORTED) {
      status = "complete";
    }

    if (status) {
      this.listener({browser, status});
    }
  }

  onLocationChange(browser, webProgress, request, locationURI, flags) {
    if (webProgress.isTopLevel) {
      let status = webProgress.isLoadingDocument ? "loading" : "complete";
      this.listener({browser, status, url: locationURI.spec});
    }
  }
}

class WindowTrackerBase extends EventEmitter {
  constructor() {
    super();

    this._handleWindowOpened = this._handleWindowOpened.bind(this);

    this._openListeners = new Set();
    this._closeListeners = new Set();

    this._listeners = new DefaultMap(() => new Set());

    this._statusListeners = new DefaultWeakMap(listener => {
      return new StatusListener(listener);
    });

    this._windowIds = new DefaultWeakMap(window => {
      window.QueryInterface(Ci.nsIInterfaceRequestor);

      return window.getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
    });
  }

  isBrowserWindow(window) {
    let {documentElement} = window.document;

    return documentElement.getAttribute("windowtype") === "navigator:browser";
  }

  // Returns an iterator for all browser windows. Unless |includeIncomplete| is
  // true, only fully-loaded windows are returned.
  * browserWindows(includeIncomplete = false) {
    // The window type parameter is only available once the window's document
    // element has been created. This means that, when looking for incomplete
    // browser windows, we need to ignore the type entirely for windows which
    // haven't finished loading, since we would otherwise skip browser windows
    // in their early loading stages.
    // This is particularly important given that the "domwindowcreated" event
    // fires for browser windows when they're in that in-between state, and just
    // before we register our own "domwindowcreated" listener.

    let e = Services.wm.getEnumerator("");
    while (e.hasMoreElements()) {
      let window = e.getNext();

      let ok = includeIncomplete;
      if (window.document.readyState === "complete") {
        ok = this.isBrowserWindow(window);
      }

      if (ok) {
        yield window;
      }
    }
  }

  get topWindow() {
    return Services.wm.getMostRecentWindow("navigator:browser");
  }

  getId(window) {
    return this._windowIds.get(window);
  }

  getCurrentWindow(context) {
    let {xulWindow} = context;
    if (xulWindow && context.viewType !== "background") {
      return xulWindow;
    }
    return this.topWindow;
  }

  getWindow(id, context) {
    if (id === WINDOW_ID_CURRENT) {
      return this.getCurrentWindow(context);
    }

    for (let window of this.browserWindows(true)) {
      if (this.getId(window) === id) {
        return window;
      }
    }
    throw new ExtensionError(`Invalid window ID: ${id}`);
  }

  get haveListeners() {
    return this._openListeners.size > 0 || this._closeListeners.size > 0;
  }

  addOpenListener(listener) {
    if (!this.haveListeners) {
      Services.ww.registerNotification(this);
    }

    this._openListeners.add(listener);

    for (let window of this.browserWindows(true)) {
      if (window.document.readyState !== "complete") {
        window.addEventListener("load", this);
      }
    }
  }

  removeOpenListener(listener) {
    this._openListeners.delete(listener);

    if (!this.haveListeners) {
      Services.ww.unregisterNotification(this);
    }
  }

  addCloseListener(listener) {
    if (!this.haveListeners) {
      Services.ww.registerNotification(this);
    }

    this._closeListeners.add(listener);
  }

  removeCloseListener(listener) {
    this._closeListeners.delete(listener);

    if (!this.haveListeners) {
      Services.ww.unregisterNotification(this);
    }
  }

  handleEvent(event) {
    if (event.type === "load") {
      event.currentTarget.removeEventListener(event.type, this);

      let window = event.target.defaultView;
      if (!this.isBrowserWindow(window)) {
        return;
      }

      for (let listener of this._openListeners) {
        try {
          listener(window);
        } catch (e) {
          Cu.reportError(e);
        }
      }
    }
  }

  observe(window, topic, data) {
    if (topic === "domwindowclosed") {
      if (!this.isBrowserWindow(window)) {
        return;
      }

      window.removeEventListener("load", this);
      for (let listener of this._closeListeners) {
        try {
          listener(window);
        } catch (e) {
          Cu.reportError(e);
        }
      }
    } else if (topic === "domwindowopened") {
      window.addEventListener("load", this);
    }
  }

  // If |type| is a normal event type, invoke |listener| each time
  // that event fires in any open window. If |type| is "progress", add
  // a web progress listener that covers all open windows.
  addListener(type, listener) {
    if (type === "domwindowopened") {
      return this.addOpenListener(listener);
    } else if (type === "domwindowclosed") {
      return this.addCloseListener(listener);
    }

    if (this._listeners.size === 0) {
      this.addOpenListener(this._handleWindowOpened);
    }

    if (type === "status") {
      listener = this._statusListeners.get(listener);
      type = "progress";
    }

    this._listeners.get(type).add(listener);

    // Register listener on all existing windows.
    for (let window of this.browserWindows()) {
      this._addWindowListener(window, type, listener);
    }
  }

  removeListener(eventType, listener) {
    if (eventType === "domwindowopened") {
      return this.removeOpenListener(listener);
    } else if (eventType === "domwindowclosed") {
      return this.removeCloseListener(listener);
    }

    if (eventType === "status") {
      listener = this._statusListeners.get(listener);
      eventType = "progress";
    }

    let listeners = this._listeners.get(eventType);
    listeners.delete(listener);

    if (listeners.size === 0) {
      this._listeners.delete(eventType);
      if (this._listeners.size === 0) {
        this.removeOpenListener(this._handleWindowOpened);
      }
    }

    // Unregister listener from all existing windows.
    let useCapture = eventType === "focus" || eventType === "blur";
    for (let window of this.browserWindows()) {
      if (eventType === "progress") {
        this.removeProgressListener(window, listener);
      } else {
        window.removeEventListener(eventType, listener, useCapture);
      }
    }
  }

  _addWindowListener(window, eventType, listener) {
    let useCapture = eventType === "focus" || eventType === "blur";

    if (eventType === "progress") {
      this.addProgressListener(window, listener);
    } else {
      window.addEventListener(eventType, listener, useCapture);
    }
  }

  _handleWindowOpened(window) {
    for (let [eventType, listeners] of this._listeners) {
      for (let listener of listeners) {
        this._addWindowListener(window, eventType, listener);
      }
    }
  }
}

class TabManagerBase {
  constructor(extension) {
    this.extension = extension;

    this._tabs = new DefaultWeakMap(tab => this.wrapTab(tab));
  }

  addActiveTabPermission(tab) {
    if (this.extension.hasPermission("activeTab")) {
      // Note that, unlike Chrome, we don't currently clear this permission with
      // the tab navigates. If the inner window is revived from BFCache before
      // we've granted this permission to a new inner window, the extension
      // maintains its permissions for it.
      tab = this.getWrapper(tab);
      tab.activeTabWindowId = tab.innerWindowId;
    }
  }

  revokeActiveTabPermission(tab) {
    this.getWrapper(tab).activeTabWindowId = null;
  }

  // Returns true if the extension has the "activeTab" permission for this tab.
  // This is somewhat more permissive than the generic "tabs" permission, as
  // checked by |hasTabPermission|, in that it also allows programmatic script
  // injection without an explicit host permission.
  hasActiveTabPermission(tab) {
    return this.getWrapper(tab).hasActiveTabPermission;
  }

  hasTabPermission(tab) {
    return this.getWrapper(tab).hasTabPermission;
  }

  getWrapper(tab) {
    return this._tabs.get(tab);
  }

  * query(queryInfo = null, context = null) {
    for (let window of this.extension.windowManager.query(queryInfo, context)) {
      for (let tab of window.getTabs()) {
        if (!queryInfo || tab.matches(queryInfo)) {
          yield tab;
        }
      }
    }
  }

  convert(tab) {
    return this.getWrapper(tab).convert();
  }

  wrapTab(tab) {
    throw new Error("Not implemented");
  }
}

class WindowManagerBase {
  constructor(extension) {
    this.extension = extension;

    this._windows = new DefaultWeakMap(window => this.wrapWindow(window));
  }

  convert(window, ...args) {
    return this.getWrapper(window).convert(...args);
  }

  getWrapper(tab) {
    return this._windows.get(tab);
  }

  * query(queryInfo = null, context = null) {
    for (let window of this.getAll()) {
      if (!queryInfo || window.matches(queryInfo, context)) {
        yield window;
      }
    }
  }
}
