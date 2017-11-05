/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* globals EventEmitter */

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

var {
  DefaultMap,
  DefaultWeakMap,
  ExtensionError,
  defineLazyGetter,
  getWinUtils,
} = ExtensionUtils;

/**
 * The platform-specific type of native tab objects, which are wrapped by
 * TabBase instances.
 *
 * @typedef {Object|XULElement} NativeTab
 */

/**
 * @typedef {Object} MutedInfo
 * @property {boolean} muted
 *        True if the tab is currently muted, false otherwise.
 * @property {string} [reason]
 *        The reason the tab is muted. Either "user", if the tab was muted by a
 *        user, or "extension", if it was muted by an extension.
 * @property {string} [extensionId]
 *        If the tab was muted by an extension, contains the internal ID of that
 *        extension.
 */

/**
 * A platform-independent base class for extension-specific wrappers around
 * native tab objects.
 *
 * @param {Extension} extension
 *        The extension object for which this wrapper is being created. Used to
 *        determine permissions for access to certain properties and
 *        functionality.
 * @param {NativeTab} nativeTab
 *        The native tab object which is being wrapped. The type of this object
 *        varies by platform.
 * @param {integer} id
 *        The numeric ID of this tab object. This ID should be the same for
 *        every extension, and for the lifetime of the tab.
 */
class TabBase {
  constructor(extension, nativeTab, id) {
    this.extension = extension;
    this.tabManager = extension.tabManager;
    this.id = id;
    this.nativeTab = nativeTab;
    this.activeTabWindowID = null;
  }

  /**
   * Sends a message, via the given context, to the ExtensionContent running in
   * this tab. The tab's current innerWindowID is automatically added to the
   * recipient filter for the message, and is used to ensure that the message is
   * not processed if the content process navigates to a different content page
   * before the message is received.
   *
   * @param {BaseContext} context
   *        The context through which to send the message.
   * @param {string} messageName
   *        The name of the messge to send.
   * @param {object} [data = {}]
   *        Arbitrary, structured-clonable message data to send.
   * @param {object} [options]
   *        An options object, as accepted by BaseContext.sendMessage.
   *
   * @returns {Promise}
   */
  sendMessage(context, messageName, data = {}, options = null) {
    let {browser, innerWindowID} = this;

    options = Object.assign({}, options);
    options.recipient = Object.assign({innerWindowID}, options.recipient);

    return context.sendMessage(browser.messageManager, messageName,
                               data, options);
  }

  /**
   * Capture the visible area of this tab, and return the result as a data: URL.
   *
   * @param {BaseContext} context
   *        The extension context for which to perform the capture.
   * @param {Object} [options]
   *        The options with which to perform the capture.
   * @param {string} [options.format = "png"]
   *        The image format in which to encode the captured data. May be one of
   *        "png" or "jpeg".
   * @param {integer} [options.quality = 92]
   *        The quality at which to encode the captured image data, ranging from
   *        0 to 100. Has no effect for the "png" format.
   *
   * @returns {Promise<string>}
   */
  capture(context, options = null) {
    if (!options) {
      options = {};
    }
    if (options.format == null) {
      options.format = "png";
    }
    if (options.quality == null) {
      options.quality = 92;
    }

    let message = {
      options,
      width: this.width,
      height: this.height,
    };

    return this.sendMessage(context, "Extension:Capture", message);
  }

  /**
   * @property {integer | null} innerWindowID
   *        The last known innerWindowID loaded into this tab's docShell. This
   *        property must remain in sync with the last known values of
   *        properties such as `url` and `title`. Any operations on the content
   *        of an out-of-process tab will automatically fail if the
   *        innerWindowID of the tab when the message is received does not match
   *        the value of this property when the message was sent.
   *        @readonly
   */
  get innerWindowID() {
    return this.browser.innerWindowID;
  }

  /**
   * @property {boolean} hasTabPermission
   *        Returns true if the extension has permission to access restricted
   *        properties of this tab, such as `url`, `title`, and `favIconUrl`.
   *        @readonly
   */
  get hasTabPermission() {
    return this.extension.hasPermission("tabs") || this.hasActiveTabPermission;
  }

  /**
   * @property {boolean} hasActiveTabPermission
   *        Returns true if the extension has the "activeTab" permission, and
   *        has been granted access to this tab due to a user executing an
   *        extension action.
   *
   *        If true, the extension may load scripts and CSS into this tab, and
   *        access restricted properties, such as its `url`.
   *        @readonly
   */
  get hasActiveTabPermission() {
    return (this.extension.hasPermission("activeTab") &&
            this.activeTabWindowID != null &&
            this.activeTabWindowID === this.innerWindowID);
  }

  /**
   * @property {boolean} incognito
   *        Returns true if this is a private browsing tab, false otherwise.
   *        @readonly
   */
  get _incognito() {
    return PrivateBrowsingUtils.isBrowserPrivate(this.browser);
  }

  /**
   * @property {string} _url
   *        Returns the current URL of this tab. Does not do any permission
   *        checks.
   *        @readonly
   */
  get _url() {
    return this.browser.currentURI.spec;
  }

  /**
   * @property {string | null} url
   *        Returns the current URL of this tab if the extension has permission
   *        to read it, or null otherwise.
   *        @readonly
   */
  get url() {
    if (this.hasTabPermission) {
      return this._url;
    }
  }

  /**
   * @property {nsIURI | null} uri
   *        Returns the current URI of this tab if the extension has permission
   *        to read it, or null otherwise.
   *        @readonly
   */
  get uri() {
    if (this.hasTabPermission) {
      return this.browser.currentURI;
    }
  }

  /**
   * @property {string} _title
   *        Returns the current title of this tab. Does not do any permission
   *        checks.
   *        @readonly
   */
  get _title() {
    return this.browser.contentTitle || this.nativeTab.label;
  }


  /**
   * @property {nsIURI | null} title
   *        Returns the current title of this tab if the extension has permission
   *        to read it, or null otherwise.
   *        @readonly
   */
  get title() {
    if (this.hasTabPermission) {
      return this._title;
    }
  }

  /**
   * @property {string} _favIconUrl
   *        Returns the current favicon URL of this tab. Does not do any permission
   *        checks.
   *        @readonly
   *        @abstract
   */
  get _favIconUrl() {
    throw new Error("Not implemented");
  }

  /**
   * @property {nsIURI | null} faviconUrl
   *        Returns the current faviron URL of this tab if the extension has permission
   *        to read it, or null otherwise.
   *        @readonly
   */
  get favIconUrl() {
    if (this.hasTabPermission) {
      return this._favIconUrl;
    }
  }

  /**
   * @property {integer} lastAccessed
   *        Returns the last time the tab was accessed as the number of
   *        milliseconds since epoch.
   *        @readonly
   *        @abstract
   */
  get lastAccessed() {
    throw new Error("Not implemented");
  }

  /**
   * @property {boolean} audible
   *        Returns true if the tab is currently playing audio, false otherwise.
   *        @readonly
   *        @abstract
   */
  get audible() {
    throw new Error("Not implemented");
  }

  /**
   * @property {XULElement} browser
   *        Returns the XUL browser for the given tab.
   *        @readonly
   *        @abstract
   */
  get browser() {
    throw new Error("Not implemented");
  }

  /**
   * @property {nsIFrameLoader} browser
   *        Returns the frameloader for the given tab.
   *        @readonly
   */
  get frameLoader() {
    return this.browser.frameLoader;
  }

  /**
   * @property {string} cookieStoreId
   *        Returns the cookie store identifier for the given tab.
   *        @readonly
   *        @abstract
   */
  get cookieStoreId() {
    throw new Error("Not implemented");
  }

  /**
   * @property {integer} openerTabId
   *        Returns the ID of the tab which opened this one.
   *        @readonly
   */
  get openerTabId() {
    return null;
  }

  /**
   * @property {integer} height
   *        Returns the pixel height of the visible area of the tab.
   *        @readonly
   *        @abstract
   */
  get height() {
    throw new Error("Not implemented");
  }

  /**
   * @property {integer} index
   *        Returns the index of the tab in its window's tab list.
   *        @readonly
   *        @abstract
   */
  get index() {
    throw new Error("Not implemented");
  }

  /**
   * @property {MutedInfo} mutedInfo
   *        Returns information about the tab's current audio muting status.
   *        @readonly
   *        @abstract
   */
  get mutedInfo() {
    throw new Error("Not implemented");
  }

  /**
   * @property {boolean} pinned
   *        Returns true if the tab is pinned, false otherwise.
   *        @readonly
   *        @abstract
   */
  get pinned() {
    throw new Error("Not implemented");
  }

  /**
   * @property {boolean} active
   *        Returns true if the tab is the currently-selected tab, false
   *        otherwise.
   *        @readonly
   *        @abstract
   */
  get active() {
    throw new Error("Not implemented");
  }

  /**
   * @property {boolean} selected
   *        An alias for `active`.
   *        @readonly
   *        @abstract
   */
  get selected() {
    throw new Error("Not implemented");
  }

  /**
   * @property {string} status
   *        Returns the current loading status of the tab. May be either
   *        "loading" or "complete".
   *        @readonly
   *        @abstract
   */
  get status() {
    throw new Error("Not implemented");
  }

  /**
   * @property {integer} height
   *        Returns the pixel height of the visible area of the tab.
   *        @readonly
   *        @abstract
   */
  get width() {
    throw new Error("Not implemented");
  }

  /**
   * @property {DOMWindow} window
   *        Returns the browser window to which the tab belongs.
   *        @readonly
   *        @abstract
   */
  get window() {
    throw new Error("Not implemented");
  }

  /**
   * @property {integer} window
   *        Returns the numeric ID of the browser window to which the tab belongs.
   *        @readonly
   *        @abstract
   */
  get windowId() {
    throw new Error("Not implemented");
  }

  /**
   * @property {boolean} isArticle
   *        Returns true if the document in the tab can be rendered in reader
   *        mode.
   *        @readonly
   *        @abstract
   */
  get isArticle() {
    throw new Error("Not implemented");
  }

  /**
   * @property {boolean} isInReaderMode
   *        Returns true if the document in the tab is being rendered in reader
   *        mode.
   *        @readonly
   *        @abstract
   */
  get isInReaderMode() {
    throw new Error("Not implemented");
  }

  /**
   * Returns true if this tab matches the the given query info object. Omitted
   * or null have no effect on the match.
   *
   * @param {object} queryInfo
   *        The query info against which to match.
   * @param {boolean} [queryInfo.active]
   *        Matches against the exact value of the tab's `active` attribute.
   * @param {boolean} [queryInfo.audible]
   *        Matches against the exact value of the tab's `audible` attribute.
   * @param {string} [queryInfo.cookieStoreId]
   *        Matches against the exact value of the tab's `cookieStoreId` attribute.
   * @param {boolean} [queryInfo.highlighted]
   *        Matches against the exact value of the tab's `highlighted` attribute.
   * @param {integer} [queryInfo.index]
   *        Matches against the exact value of the tab's `index` attribute.
   * @param {boolean} [queryInfo.muted]
   *        Matches against the exact value of the tab's `mutedInfo.muted` attribute.
   * @param {boolean} [queryInfo.pinned]
   *        Matches against the exact value of the tab's `pinned` attribute.
   * @param {string} [queryInfo.status]
   *        Matches against the exact value of the tab's `status` attribute.
   * @param {string} [queryInfo.title]
   *        Matches against the exact value of the tab's `title` attribute.
   *
   *        Note: Per specification, this should perform a pattern match, rather
   *        than an exact value match, and will do so in the future.
   * @param {MatchPattern} [queryInfo.url]
   *        Requires the tab's URL to match the given MatchPattern object.
   *
   * @returns {boolean}
   *        True if the tab matches the query.
   */
  matches(queryInfo) {
    const PROPS = ["active", "audible", "cookieStoreId", "highlighted", "index", "openerTabId", "pinned", "status"];

    if (PROPS.some(prop => queryInfo[prop] != null && queryInfo[prop] !== this[prop])) {
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
    if (queryInfo.title && !queryInfo.title.matches(this.title)) {
      return false;
    }

    return true;
  }

  /**
   * Converts this tab object to a JSON-compatible object containing the values
   * of its properties which the extension is permitted to access, in the format
   * requried to be returned by WebExtension APIs.
   *
   * @param {Tab} [fallbackTab]
   *        A tab to retrieve geometry data from if the lazy geometry data for
   *        this tab hasn't been initialized yet.
   * @returns {object}
   */
  convert(fallbackTab = null) {
    let result = {
      id: this.id,
      index: this.index,
      windowId: this.windowId,
      highlighted: this.selected,
      active: this.selected,
      pinned: this.pinned,
      status: this.status,
      discarded: this.discarded,
      incognito: this.incognito,
      width: this.width,
      height: this.height,
      lastAccessed: this.lastAccessed,
      audible: this.audible,
      mutedInfo: this.mutedInfo,
      isArticle: this.isArticle,
      isInReaderMode: this.isInReaderMode,
    };

    // If the tab has not been fully layed-out yet, fallback to the geometry
    // from a different tab (usually the currently active tab).
    if (fallbackTab && (!result.width || !result.height)) {
      result.width = fallbackTab.width;
      result.height = fallbackTab.height;
    }

    let opener = this.openerTabId;
    if (opener) {
      result.openerTabId = opener;
    }

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

  /**
   * Inserts a script or stylesheet in the given tab, and returns a promise
   * which resolves when the operation has completed.
   *
   * @param {BaseContext} context
   *        The extension context for which to perform the injection.
   * @param {InjectDetails} details
   *        The InjectDetails object, specifying what to inject, where, and
   *        when.
   * @param {string} kind
   *        The kind of data being injected. Either "script" or "css".
   * @param {string} method
   *        The name of the method which was called to trigger the injection.
   *        Used to generate appropriate error messages on failure.
   *
   * @returns {Promise}
   *        Resolves to the result of the execution, once it has completed.
   * @private
   */
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

    options.hasActiveTabPermission = this.hasActiveTabPermission;
    options.matches = this.extension.whiteListedHosts.patterns.map(host => host.pattern);

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

    options.wantReturnValue = true;

    return this.sendMessage(context, "Extension:Execute", {options});
  }

  /**
   * Executes a script in the tab's content window, and returns a Promise which
   * resolves to the result of the evaluation, or rejects to the value of any
   * error the injection generates.
   *
   * @param {BaseContext} context
   *        The extension context for which to inject the script.
   * @param {InjectDetails} details
   *        The InjectDetails object, specifying what to inject, where, and
   *        when.
   *
   * @returns {Promise}
   *        Resolves to the result of the evaluation of the given script, once
   *        it has completed, or rejects with any error the evaluation
   *        generates.
   */
  executeScript(context, details) {
    return this._execute(context, details, "js", "executeScript");
  }

  /**
   * Injects CSS into the tab's content window, and returns a Promise which
   * resolves when the injection is complete.
   *
   * @param {BaseContext} context
   *        The extension context for which to inject the script.
   * @param {InjectDetails} details
   *        The InjectDetails object, specifying what to inject, and where.
   *
   * @returns {Promise}
   *        Resolves when the injection has completed.
   */
  insertCSS(context, details) {
    return this._execute(context, details, "css", "insertCSS").then(() => {});
  }


  /**
   * Removes CSS which was previously into the tab's content window via
   * `insertCSS`, and returns a Promise which resolves when the operation is
   * complete.
   *
   * @param {BaseContext} context
   *        The extension context for which to remove the CSS.
   * @param {InjectDetails} details
   *        The InjectDetails object, specifying what to remove, and from where.
   *
   * @returns {Promise}
   *        Resolves when the operation has completed.
   */
  removeCSS(context, details) {
    return this._execute(context, details, "css", "removeCSS").then(() => {});
  }
}

defineLazyGetter(TabBase.prototype, "incognito", function() { return this._incognito; });

// Note: These must match the values in windows.json.
const WINDOW_ID_NONE = -1;
const WINDOW_ID_CURRENT = -2;

/**
 * A platform-independent base class for extension-specific wrappers around
 * native browser windows
 *
 * @param {Extension} extension
 *        The extension object for which this wrapper is being created.
 * @param {DOMWindow} window
 *        The browser DOM window which is being wrapped.
 * @param {integer} id
 *        The numeric ID of this DOM window object. This ID should be the same for
 *        every extension, and for the lifetime of the window.
 */
class WindowBase {
  constructor(extension, window, id) {
    this.extension = extension;
    this.window = window;
    this.id = id;
  }

  /**
   * @property {nsIXULWindow} xulWindow
   *        The nsIXULWindow object for this browser window.
   *        @readonly
   */
  get xulWindow() {
    return this.window.document.docShell.treeOwner
               .QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIXULWindow);
  }

  /**
   * Returns true if this window is the current window for the given extension
   * context, false otherwise.
   *
   * @param {BaseContext} context
   *        The extension context for which to perform the check.
   *
   * @returns {boolean}
   */
  isCurrentFor(context) {
    if (context && context.currentWindow) {
      return this.window === context.currentWindow;
    }
    return this.isLastFocused;
  }

  /**
   * @property {string} type
   *        The type of the window, as defined by the WebExtension API. May be
   *        either "normal" or "popup".
   *        @readonly
   */
  get type() {
    let {chromeFlags} = this.xulWindow;

    if (chromeFlags & Ci.nsIWebBrowserChrome.CHROME_OPENAS_DIALOG) {
      return "popup";
    }

    return "normal";
  }

  /**
   * Converts this window object to a JSON-compatible object which may be
   * returned to an extension, in the format requried to be returned by
   * WebExtension APIs.
   *
   * @param {object} [getInfo]
   *        An optional object, the properties of which determine what data is
   *        available on the result object.
   * @param {boolean} [getInfo.populate]
   *        Of true, the result object will contain a `tabs` property,
   *        containing an array of converted Tab objects, one for each tab in
   *        the window.
   *
   * @returns {object}
   */
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
      title: this.title,
    };

    if (getInfo && getInfo.populate) {
      result.tabs = Array.from(this.getTabs(), tab => tab.convert());
    }

    return result;
  }

  /**
   * Returns true if this window matches the the given query info object. Omitted
   * or null have no effect on the match.
   *
   * @param {object} queryInfo
   *        The query info against which to match.
   * @param {boolean} [queryInfo.currentWindow]
   *        Matches against against the return value of `isCurrentFor()` for the
   *        given context.
   * @param {boolean} [queryInfo.lastFocusedWindow]
   *        Matches against the exact value of the window's `isLastFocused` attribute.
   * @param {boolean} [queryInfo.windowId]
   *        Matches against the exact value of the window's ID, taking into
   *        account the special WINDOW_ID_CURRENT value.
   * @param {string} [queryInfo.windowType]
   *        Matches against the exact value of the window's `type` attribute.
   * @param {BaseContext} context
   *        The extension context for which the matching is being performed.
   *        Used to determine the current window for relevant properties.
   *
   * @returns {boolean}
   *        True if the window matches the query.
   */
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

  /**
   * @property {boolean} focused
   *        Returns true if the browser window is currently focused.
   *        @readonly
   *        @abstract
   */
  get focused() {
    throw new Error("Not implemented");
  }

  /**
   * @property {integer} top
   *        Returns the pixel offset of the top of the window from the top of
   *        the screen.
   *        @readonly
   *        @abstract
   */
  get top() {
    throw new Error("Not implemented");
  }

  /**
   * @property {integer} left
   *        Returns the pixel offset of the left of the window from the left of
   *        the screen.
   *        @readonly
   *        @abstract
   */
  get left() {
    throw new Error("Not implemented");
  }

  /**
   * @property {integer} width
   *        Returns the pixel width of the window.
   *        @readonly
   *        @abstract
   */
  get width() {
    throw new Error("Not implemented");
  }

  /**
   * @property {integer} height
   *        Returns the pixel height of the window.
   *        @readonly
   *        @abstract
   */
  get height() {
    throw new Error("Not implemented");
  }

  /**
   * @property {boolean} incognito
   *        Returns true if this is a private browsing window, false otherwise.
   *        @readonly
   *        @abstract
   */
  get incognito() {
    throw new Error("Not implemented");
  }

  /**
   * @property {boolean} alwaysOnTop
   *        Returns true if this window is constrained to always remain above
   *        other windows.
   *        @readonly
   *        @abstract
   */
  get alwaysOnTop() {
    throw new Error("Not implemented");
  }

  /**
   * @property {boolean} isLastFocused
   *        Returns true if this is the browser window which most recently had
   *        focus.
   *        @readonly
   *        @abstract
   */
  get isLastFocused() {
    throw new Error("Not implemented");
  }

  /**
   * @property {string} state
   *        Returns or sets the current state of this window, as determined by
   *        `getState()`.
   *        @abstract
   */
  get state() {
    throw new Error("Not implemented");
  }

  set state(state) {
    throw new Error("Not implemented");
  }

  /**
   * @property {nsIURI | null} title
   *        Returns the current title of this window if the extension has permission
   *        to read it, or null otherwise.
   *        @readonly
   */
  get title() {
    if (this.activeTab.hasTabPermission) {
      return this._title;
    }
  }

  // The JSDoc validator does not support @returns tags in abstract functions or
  // star functions without return statements.
  /* eslint-disable valid-jsdoc */
  /**
   * Returns the window state of the given window.
   *
   * @param {DOMWindow} window
   *        The window for which to return a state.
   *
   * @returns {string}
   *        The window's state. One of "normal", "minimized", "maximized",
   *        "fullscreen", or "docked".
   * @static
   * @abstract
   */
  static getState(window) {
    throw new Error("Not implemented");
  }

  /**
   * Returns an iterator of TabBase objects for each tab in this window.
   *
   * @returns {Iterator<TabBase>}
   */
  getTabs() {
    throw new Error("Not implemented");
  }

  /**
   * @property {TabBase} The window's currently active tab.
   */
  get activeTab() {
    throw new Error("Not implemented");
  }
  /* eslint-enable valid-jsdoc */
}

Object.assign(WindowBase, {WINDOW_ID_NONE, WINDOW_ID_CURRENT});

/**
 * The parameter type of "tab-attached" events, which are emitted when a
 * pre-existing tab is attached to a new window.
 *
 * @typedef {Object} TabAttachedEvent
 * @property {NativeTab} tab
 *        The native tab object in the window to which the tab is being
 *        attached. This may be a different object than was used to represent
 *        the tab in the old window.
 * @property {integer} tabId
 *        The ID of the tab being attached.
 * @property {integer} newWindowId
 *        The ID of the window to which the tab is being attached.
 * @property {integer} newPosition
 *        The position of the tab in the tab list of the new window.
 */

/**
 * The parameter type of "tab-detached" events, which are emitted when a
 * pre-existing tab is detached from a window, in order to be attached to a new
 * window.
 *
 * @typedef {Object} TabDetachedEvent
 * @property {NativeTab} tab
 *        The native tab object in the window from which the tab is being
 *        detached. This may be a different object than will be used to
 *        represent the tab in the new window.
 * @property {NativeTab} adoptedBy
 *        The native tab object in the window to which the tab will be attached,
 *        and is adopting the contents of this tab. This may be a different
 *        object than the tab in the previous window.
 * @property {integer} tabId
 *        The ID of the tab being detached.
 * @property {integer} oldWindowId
 *        The ID of the window from which the tab is being detached.
 * @property {integer} oldPosition
 *        The position of the tab in the tab list of the window from which it is
 *        being detached.
 */

/**
 * The parameter type of "tab-created" events, which are emitted when a
 * new tab is created.
 *
 * @typedef {Object} TabCreatedEvent
 * @property {NativeTab} tab
 *        The native tab object for the tab which is being created.
 */

/**
 * The parameter type of "tab-removed" events, which are emitted when a
 * tab is removed and destroyed.
 *
 * @typedef {Object} TabRemovedEvent
 * @property {NativeTab} tab
 *        The native tab object for the tab which is being removed.
 * @property {integer} tabId
 *        The ID of the tab being removed.
 * @property {integer} windowId
 *        The ID of the window from which the tab is being removed.
 * @property {boolean} isWindowClosing
 *        True if the tab is being removed because the window is closing.
 */

/**
 * An object containg basic, extension-independent information about the window
 * and tab that a XUL <browser> belongs to.
 *
 * @typedef {Object} BrowserData
 * @property {integer} tabId
 *        The numeric ID of the tab that a <browser> belongs to, or -1 if it
 *        does not belong to a tab.
 * @property {integer} windowId
 *        The numeric ID of the browser window that a <browser> belongs to, or -1
 *        if it does not belong to a browser window.
 */

/**
 * A platform-independent base class for the platform-specific TabTracker
 * classes, which track the opening and closing of tabs, and manage the mapping
 * of them between numeric IDs and native tab objects.
 *
 * Instances of this class are EventEmitters which emit the following events,
 * each with an argument of the given type:
 *
 * - "tab-attached" {@link TabAttacheEvent}
 * - "tab-detached" {@link TabDetachedEvent}
 * - "tab-created" {@link TabCreatedEvent}
 * - "tab-removed" {@link TabRemovedEvent}
 */
class TabTrackerBase extends EventEmitter {
  on(...args) {
    if (!this.initialized) {
      this.init();
    }

    return super.on(...args); // eslint-disable-line mozilla/balanced-listeners
  }


  /**
   * Called to initialize the tab tracking listeners the first time that an
   * event listener is added.
   *
   * @protected
   * @abstract
   */
  init() {
    throw new Error("Not implemented");
  }

  // The JSDoc validator does not support @returns tags in abstract functions or
  // star functions without return statements.
  /* eslint-disable valid-jsdoc */
  /**
   * Returns the numeric ID for the given native tab.
   *
   * @param {NativeTab} nativeTab
   *        The native tab for which to return an ID.
   *
   * @returns {integer}
   *        The tab's numeric ID.
   * @abstract
   */
  getId(nativeTab) {
    throw new Error("Not implemented");
  }

  /**
   * Returns the native tab with the given numeric ID.
   *
   * @param {integer} tabId
   *        The numeric ID of the tab to return.
   * @param {*} default_
   *        The value to return if no tab exists with the given ID.
   *
   * @returns {NativeTab}
   * @throws {ExtensionError}
   *       If no tab exists with the given ID and a default return value is not
   *       provided.
   * @abstract
   */
  getTab(tabId, default_ = undefined) {
    throw new Error("Not implemented");
  }

  /**
   * Returns basic information about the tab and window that the given browser
   * belongs to.
   *
   * @param {XULElement} browser
   *        The XUL browser element for which to return data.
   *
   * @returns {BrowserData}
   * @abstract
   */
  /* eslint-enable valid-jsdoc */
  getBrowserData(browser) {
    throw new Error("Not implemented");
  }

  /**
   * @property {NativeTab} activeTab
   *        Returns the native tab object for the active tab in the
   *        most-recently focused window, or null if no live tabs currently
   *        exist.
   *        @abstract
   */
  get activeTab() {
    throw new Error("Not implemented");
  }
}

/**
 * A browser progress listener instance which calls a given listener function
 * whenever the status of the given browser changes.
 *
 * @param {function(Object)} listener
 *        A function to be called whenever the status of a tab's top-level
 *        browser. It is passed an object with a `browser` property pointing to
 *        the XUL browser, and a `status` property with a string description of
 *        the browser's status.
 * @private
 */
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

/**
 * A platform-independent base class for the platform-specific WindowTracker
 * classes, which track the opening and closing of windows, and manage the
 * mapping of them between numeric IDs and native tab objects.
 */
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
      return getWinUtils(window).outerWindowID;
    });
  }

  isBrowserWindow(window) {
    let {documentElement} = window.document;

    return documentElement.getAttribute("windowtype") === "navigator:browser";
  }

  // The JSDoc validator does not support @returns tags in abstract functions or
  // star functions without return statements.
  /* eslint-disable valid-jsdoc */
  /**
   * Returns an iterator for all currently active browser windows.
   *
   * @param {boolean} [includeInomplete = false]
   *        If true, include browser windows which are not yet fully loaded.
   *        Otherwise, only include windows which are.
   *
   * @returns {Iterator<DOMWindow>}
   */
  /* eslint-enable valid-jsdoc */
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

  /**
   * @property {DOMWindow|null} topWindow
   *        The currently active, or topmost, browser window, or null if no
   *        browser window is currently open.
   *        @readonly
   */
  get topWindow() {
    return Services.wm.getMostRecentWindow("navigator:browser");
  }

  /**
   * Returns the numeric ID for the given browser window.
   *
   * @param {DOMWindow} window
   *        The DOM window for which to return an ID.
   *
   * @returns {integer}
   *        The window's numeric ID.
   */
  getId(window) {
    return this._windowIds.get(window);
  }

  /**
   * Returns the browser window to which the given context belongs, or the top
   * browser window if the context does not belong to a browser window.
   *
   * @param {BaseContext} context
   *        The extension context for which to return the current window.
   *
   * @returns {DOMWindow|null}
   */
  getCurrentWindow(context) {
    return context.currentWindow || this.topWindow;
  }

  /**
   * Returns the browser window with the given ID.
   *
   * @param {integer} id
   *        The ID of the window to return.
   * @param {BaseContext} context
   *        The extension context for which the matching is being performed.
   *        Used to determine the current window for relevant properties.
   *
   * @returns {DOMWindow}
   * @throws {ExtensionError}
   *        If no window exists with the given ID.
   */
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

  /**
   * @property {boolean} _haveListeners
   *        Returns true if any window open or close listeners are currently
   *        registered.
   * @private
   */
  get _haveListeners() {
    return this._openListeners.size > 0 || this._closeListeners.size > 0;
  }

  /**
   * Register the given listener function to be called whenever a new browser
   * window is opened.
   *
   * @param {function(DOMWindow)} listener
   *        The listener function to register.
   */
  addOpenListener(listener) {
    if (!this._haveListeners) {
      Services.ww.registerNotification(this);
    }

    this._openListeners.add(listener);

    for (let window of this.browserWindows(true)) {
      if (window.document.readyState !== "complete") {
        window.addEventListener("load", this);
      }
    }
  }

  /**
   * Unregister a listener function registered in a previous addOpenListener
   * call.
   *
   * @param {function(DOMWindow)} listener
   *        The listener function to unregister.
   */
  removeOpenListener(listener) {
    this._openListeners.delete(listener);

    if (!this._haveListeners) {
      Services.ww.unregisterNotification(this);
    }
  }

  /**
   * Register the given listener function to be called whenever a browser
   * window is closed.
   *
   * @param {function(DOMWindow)} listener
   *        The listener function to register.
   */
  addCloseListener(listener) {
    if (!this._haveListeners) {
      Services.ww.registerNotification(this);
    }

    this._closeListeners.add(listener);
  }

  /**
   * Unregister a listener function registered in a previous addCloseListener
   * call.
   *
   * @param {function(DOMWindow)} listener
   *        The listener function to unregister.
   */
  removeCloseListener(listener) {
    this._closeListeners.delete(listener);

    if (!this._haveListeners) {
      Services.ww.unregisterNotification(this);
    }
  }

  /**
   * Handles load events for recently-opened windows, and adds additional
   * listeners which may only be safely added when the window is fully loaded.
   *
   * @param {Event} event
   *        A DOM event to handle.
   * @private
   */
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

  /**
   * Observes "domwindowopened" and "domwindowclosed" events, notifies the
   * appropriate listeners, and adds necessary additional listeners to the new
   * windows.
   *
   * @param {DOMWindow} window
   *        A DOM window.
   * @param {string} topic
   *        The topic being observed.
   * @private
   */
  observe(window, topic) {
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

  /**
   * Add an event listener to be called whenever the given DOM event is recieved
   * at the top level of any browser window.
   *
   * @param {string} type
   *        The type of event to listen for. May be any valid DOM event name, or
   *        one of the following special cases:
   *
   *        - "progress": Adds a tab progress listener to every browser window.
   *        - "status": Adds a StatusListener to every tab of every browser
   *           window.
   *        - "domwindowopened": Acts as an alias for addOpenListener.
   *        - "domwindowclosed": Acts as an alias for addCloseListener.
   * @param {function|object} listener
   *        The listener to invoke in response to the given events.
   *
   * @returns {undefined}
   */
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

  /**
   * Removes an event listener previously registered via an addListener call.
   *
   * @param {string} type
   *        The type of event to stop listening for.
   * @param {function|object} listener
   *        The listener to remove.
   *
   * @returns {undefined}
   */
  removeListener(type, listener) {
    if (type === "domwindowopened") {
      return this.removeOpenListener(listener);
    } else if (type === "domwindowclosed") {
      return this.removeCloseListener(listener);
    }

    if (type === "status") {
      listener = this._statusListeners.get(listener);
      type = "progress";
    }

    let listeners = this._listeners.get(type);
    listeners.delete(listener);

    if (listeners.size === 0) {
      this._listeners.delete(type);
      if (this._listeners.size === 0) {
        this.removeOpenListener(this._handleWindowOpened);
      }
    }

    // Unregister listener from all existing windows.
    let useCapture = type === "focus" || type === "blur";
    for (let window of this.browserWindows()) {
      if (type === "progress") {
        this.removeProgressListener(window, listener);
      } else {
        window.removeEventListener(type, listener, useCapture);
      }
    }
  }

  /**
   * Adds a listener for the given event to the given window.
   *
   * @param {DOMWindow} window
   *        The browser window to which to add the listener.
   * @param {string} eventType
   *        The type of DOM event to listen for, or "progress" to add a tab
   *        progress listener.
   * @param {function|object} listener
   *        The listener to add.
   * @private
   */
  _addWindowListener(window, eventType, listener) {
    let useCapture = eventType === "focus" || eventType === "blur";

    if (eventType === "progress") {
      this.addProgressListener(window, listener);
    } else {
      window.addEventListener(eventType, listener, useCapture);
    }
  }

  /**
   * A private method which is called whenever a new browser window is opened,
   * and adds the necessary listeners to it.
   *
   * @param {DOMWindow} window
   *        The window being opened.
   * @private
   */
  _handleWindowOpened(window) {
    for (let [eventType, listeners] of this._listeners) {
      for (let listener of listeners) {
        this._addWindowListener(window, eventType, listener);
      }
    }
  }

  /**
   * Adds a tab progress listener to the given browser window.
   *
   * @param {DOMWindow} window
   *        The browser window to which to add the listener.
   * @param {object} listener
   *        The tab progress listener to add.
   * @abstract
   */
  addProgressListener(window, listener) {
    throw new Error("Not implemented");
  }

  /**
   * Removes a tab progress listener from the given browser window.
   *
   * @param {DOMWindow} window
   *        The browser window from which to remove the listener.
   * @param {object} listener
   *        The tab progress listener to remove.
   * @abstract
   */
  removeProgressListener(window, listener) {
    throw new Error("Not implemented");
  }
}

/**
 * Manages native tabs, their wrappers, and their dynamic permissions for a
 * particular extension.
 *
 * @param {Extension} extension
 *        The extension for which to manage tabs.
 */
class TabManagerBase {
  constructor(extension) {
    this.extension = extension;

    this._tabs = new DefaultWeakMap(tab => this.wrapTab(tab));
  }

  /**
   * If the extension has requested activeTab permission, grant it those
   * permissions for the current inner window in the given native tab.
   *
   * @param {NativeTab} nativeTab
   *        The native tab for which to grant permissions.
   */
  addActiveTabPermission(nativeTab) {
    if (this.extension.hasPermission("activeTab")) {
      // Note that, unlike Chrome, we don't currently clear this permission with
      // the tab navigates. If the inner window is revived from BFCache before
      // we've granted this permission to a new inner window, the extension
      // maintains its permissions for it.
      let tab = this.getWrapper(nativeTab);
      tab.activeTabWindowID = tab.innerWindowID;
    }
  }

  /**
   * Revoke the extension's activeTab permissions for the current inner window
   * of the given native tab.
   *
   * @param {NativeTab} nativeTab
   *        The native tab for which to revoke permissions.
   */
  revokeActiveTabPermission(nativeTab) {
    this.getWrapper(nativeTab).activeTabWindowID = null;
  }

  /**
   * Returns true if the extension has requested activeTab permission, and has
   * been granted permissions for the current inner window if this tab.
   *
   * @param {NativeTab} nativeTab
   *        The native tab for which to check permissions.
   * @returns {boolean}
   *        True if the extension has activeTab permissions for this tab.
   */
  hasActiveTabPermission(nativeTab) {
    return this.getWrapper(nativeTab).hasActiveTabPermission;
  }

  /**
   * Returns true if the extension has permissions to access restricted
   * properties of the given native tab. In practice, this means that it has
   * either requested the "tabs" permission or has activeTab permissions for the
   * given tab.
   *
   * @param {NativeTab} nativeTab
   *        The native tab for which to check permissions.
   * @returns {boolean}
   *        True if the extension has permissions for this tab.
   */
  hasTabPermission(nativeTab) {
    return this.getWrapper(nativeTab).hasTabPermission;
  }

  /**
   * Returns this extension's TabBase wrapper for the given native tab. This
   * method will always return the same wrapper object for any given native tab.
   *
   * @param {NativeTab} nativeTab
   *        The tab for which to return a wrapper.
   *
   * @returns {TabBase}
   *        The wrapper for this tab.
   */
  getWrapper(nativeTab) {
    return this._tabs.get(nativeTab);
  }

  /**
   * Converts the given native tab to a JSON-compatible object, in the format
   * requried to be returned by WebExtension APIs, which may be safely passed to
   * extension code.
   *
   * @param {NativeTab} nativeTab
   *        The native tab to convert.
   * @param {NativeTab} [fallbackTab]
   *        A tab to retrieve geometry data from if the lazy geometry data for
   *        this tab hasn't been initialized yet.
   *
   * @returns {Object}
   */
  convert(nativeTab, fallbackTab = null) {
    return this.getWrapper(nativeTab)
               .convert(fallbackTab && this.getWrapper(fallbackTab));
  }

  // The JSDoc validator does not support @returns tags in abstract functions or
  // star functions without return statements.
  /* eslint-disable valid-jsdoc */
  /**
   * Returns an iterator of TabBase objects which match the given query info.
   *
   * @param {Object|null} [queryInfo = null]
   *        An object containing properties on which to filter. May contain any
   *        properties which are recognized by {@link TabBase#matches} or
   *        {@link WindowBase#matches}. Unknown properties will be ignored.
   * @param {BaseContext|null} [context = null]
   *        The extension context for which the matching is being performed.
   *        Used to determine the current window for relevant properties.
   *
   * @returns {Iterator<TabBase>}
   */
  * query(queryInfo = null, context = null) {
    for (let window of this.extension.windowManager.query(queryInfo, context)) {
      for (let tab of window.getTabs()) {
        if (!queryInfo || tab.matches(queryInfo)) {
          yield tab;
        }
      }
    }
  }

  /**
   * Returns a TabBase wrapper for the tab with the given ID.
   *
   * @param {integer} id
   *        The ID of the tab for which to return a wrapper.
   *
   * @returns {TabBase}
   * @throws {ExtensionError}
   *        If no tab exists with the given ID.
   * @abstract
   */
  get(tabId) {
    throw new Error("Not implemented");
  }

  /**
   * Returns a new TabBase instance wrapping the given native tab.
   *
   * @param {NativeTab} nativeTab
   *        The native tab for which to return a wrapper.
   *
   * @returns {TabBase}
   * @protected
   * @abstract
   */
  /* eslint-enable valid-jsdoc */
  wrapTab(nativeTab) {
    throw new Error("Not implemented");
  }
}

/**
 * Manages native browser windows and their wrappers for a particular extension.
 *
 * @param {Extension} extension
 *        The extension for which to manage windows.
 */
class WindowManagerBase {
  constructor(extension) {
    this.extension = extension;

    this._windows = new DefaultWeakMap(window => this.wrapWindow(window));
  }

  /**
   * Converts the given browser window to a JSON-compatible object, in the
   * format requried to be returned by WebExtension APIs, which may be safely
   * passed to extension code.
   *
   * @param {DOMWindow} window
   *        The browser window to convert.
   * @param {*} args
   *        Additional arguments to be passed to {@link WindowBase#convert}.
   *
   * @returns {Object}
   */
  convert(window, ...args) {
    return this.getWrapper(window).convert(...args);
  }

  /**
   * Returns this extension's WindowBase wrapper for the given browser window.
   * This method will always return the same wrapper object for any given
   * browser window.
   *
   * @param {DOMWindow} window
   *        The browser window for which to return a wrapper.
   *
   * @returns {WindowBase}
   *        The wrapper for this tab.
   */
  getWrapper(window) {
    return this._windows.get(window);
  }

  // The JSDoc validator does not support @returns tags in abstract functions or
  // star functions without return statements.
  /* eslint-disable valid-jsdoc */
  /**
   * Returns an iterator of WindowBase objects which match the given query info.
   *
   * @param {Object|null} [queryInfo = null]
   *        An object containing properties on which to filter. May contain any
   *        properties which are recognized by {@link WindowBase#matches}.
   *        Unknown properties will be ignored.
   * @param {BaseContext|null} [context = null]
   *        The extension context for which the matching is being performed.
   *        Used to determine the current window for relevant properties.
   *
   * @returns {Iterator<WindowBase>}
   */
  * query(queryInfo = null, context = null) {
    for (let window of this.getAll()) {
      if (!queryInfo || window.matches(queryInfo, context)) {
        yield window;
      }
    }
  }

  /**
   * Returns a WindowBase wrapper for the browser window with the given ID.
   *
   * @param {integer} id
   *        The ID of the browser window for which to return a wrapper.
   * @param {BaseContext} context
   *        The extension context for which the matching is being performed.
   *        Used to determine the current window for relevant properties.
   *
   * @returns{WindowBase}
   * @throws {ExtensionError}
   *        If no window exists with the given ID.
   * @abstract
   */
  get(windowId, context) {
    throw new Error("Not implemented");
  }

  /**
   * Returns an iterator of WindowBase wrappers for each currently existing
   * browser window.
   *
   * @returns {Iterator<WindowBase>}
   * @abstract
   */
  getAll() {
    throw new Error("Not implemented");
  }

  /**
   * Returns a new WindowBase instance wrapping the given browser window.
   *
   * @param {DOMWindow} window
   *        The browser window for which to return a wrapper.
   *
   * @returns {WindowBase}
   * @protected
   * @abstract
   */
  wrapWindow(window) {
    throw new Error("Not implemented");
  }
  /* eslint-enable valid-jsdoc */
}

Object.assign(global, {TabTrackerBase, TabManagerBase, TabBase, WindowTrackerBase, WindowManagerBase, WindowBase});
