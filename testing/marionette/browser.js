/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;

Cu.import("chrome://marionette/content/element.js");
Cu.import("chrome://marionette/content/frame.js");

this.EXPORTED_SYMBOLS = ["browser"];

this.browser = {};

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

/**
 * Creates a browsing context wrapper.
 *
 * Browsing contexts handle interactions with the browser, according to
 * the current environment (desktop, B2G, Fennec, &c).
 *
 * @param {nsIDOMWindow} win
 *     The window whose browser needs to be accessed.
 * @param {GeckoDriver} driver
 *     Reference to the driver the browser is attached to.
 */
browser.Context = class {

  /**
   * @param {<xul:browser>} win
   *     Frame that is expected to contain the view of the web document.
   * @param {GeckoDriver} driver
   *     Reference to driver instance.
   */
  constructor(win, driver) {
    this.window = win;
    this.driver = driver;

    // In Firefox this is <xul:tabbrowser> (not <xul:browser>!)
    // and BrowserApp in Fennec
    this.browser = undefined;
    this.setBrowser(win);

    this.knownFrames = [];

    // Used in B2G to identify the homescreen content page
    this.mainContentId = null;

    // Used to set curFrameId upon new session
    this.newSession = true;

    this.seenEls = new element.Store();

    // A reference to the tab corresponding to the current window handle, if any.
    // Specifically, this.tab refers to the last tab that Marionette switched
    // to in this browser window. Note that this may not equal the currently
    // selected tab. For example, if Marionette switches to tab A, and then
    // clicks on a button that opens a new tab B in the same browser window,
    // this.tab will still point to tab A, despite tab B being the currently
    // selected tab.
    this.tab = null;
    this.pendingCommands = [];

    // We should have one frame.Manager per browser.Context so that we
    // can handle modals in each <xul:browser>.
    this.frameManager = new frame.Manager(driver);
    this.frameRegsPending = 0;

    // register all message listeners
    this.frameManager.addMessageManagerListeners(driver.mm);
    this.getIdForBrowser = driver.getIdForBrowser.bind(driver);
    this.updateIdForBrowser = driver.updateIdForBrowser.bind(driver);
    this._curFrameId = null;
    this._browserWasRemote = null;
    this._hasRemotenessChange = false;
  }

  /**
   * Get the <xul:browser> for the current tab in this tab browser.
   *
   * @return {<xul:browser>}
   *     Browser linked to |this.tab| or the tab browser's
   *     |selectedBrowser|.
   */
  get browserForTab() {
    if (this.browser.getBrowserForTab) {
      return this.browser.getBrowserForTab(this.tab);
    } else {
      return this.browser.selectedBrowser;
    }
  }

  /**
   * The current frame ID is managed per browser element on desktop in
   * case the ID needs to be refreshed. The currently selected window is
   * identified by a tab.
   */
  get curFrameId() {
    let rv = null;
    if (this.driver.appName == "B2G") {
      rv = this._curFrameId;
    } else if (this.tab) {
      rv = this.getIdForBrowser(this.browserForTab);
    }
    return rv;
  }

  set curFrameId(id) {
    if (this.driver.appName != "Firefox") {
      this._curFrameId = id;
    }
  }

  /**
   * Retrieves the current tabmodal UI object.  According to the browser
   * associated with the currently selected tab.
   */
  getTabModalUI() {
    let br = this.browserForTab;
    if (!br.hasAttribute("tabmodalPromptShowing")) {
      return null;
    }

    // The modal is a direct sibling of the browser element.
    // See tabbrowser.xml's getTabModalPromptBox.
    let modals = br.parentNode.getElementsByTagNameNS(
        XUL_NS, "tabmodalprompt");
    return modals[0].ui;
  }

  /**
   * Set the browser if the application is not B2G.
   *
   * @param {nsIDOMWindow} win
   *     Current window reference.
   */
  setBrowser(win) {
    switch (this.driver.appName) {
      case "Firefox":
        this.browser = win.gBrowser;
        break;

      case "Fennec":
        this.browser = win.BrowserApp;
        break;
    }
  }

  /** Called when we start a session with this browser. */
  startSession(newSession, win, callback) {
    callback(win, newSession);
  }

  /** Closes current tab. */
  closeTab() {
    if (this.browser &&
        this.browser.removeTab &&
        this.tab !== null && (this.driver.appName != "B2G")) {
      this.browser.removeTab(this.tab);
    }
  }

  /**
   * Opens a tab with given URI.
   *
   * @param {string} uri
   *      URI to open.
   */
  addTab(uri) {
    return this.browser.addTab(uri, true);
  }

  /**
   * Re-sets current tab and updates remoteness tracking.
   *
   * If a window is provided, the internal reference is updated before
   * proceeding.
   */
  switchToTab(ind, win) {
    if (win) {
      this.window = win;
      this.setBrowser(win);
    }
    if (this.browser.selectTabAtIndex) {
      this.browser.selectTabAtIndex(ind);
      this.tab = this.browser.selectedTab;
      this._browserWasRemote = this.browserForTab.isRemoteBrowser;
    }
    else {
      this.tab = this.browser.selectedTab;
    }
    this._hasRemotenessChange = false;
  }

  /**
   * Registers a new frame, and sets its current frame id to this frame
   * if it is not already assigned, and if a) we already have a session
   * or b) we're starting a new session and it is the right start frame.
   *
   * @param {string} uid
   *     Frame uid for use by Marionette.
   * @param the XUL <browser> that was the target of the originating message.
   */
  register(uid, target) {
    let remotenessChange = this.hasRemotenessChange();
    if (this.curFrameId === null || remotenessChange) {
      if (this.browser) {
        // If we're setting up a new session on Firefox, we only process the
        // registration for this frame if it belongs to the current tab.
        if (!this.tab) {
          this.switchToTab(this.browser.selectedIndex);
        }

        if (target == this.browserForTab) {
          this.updateIdForBrowser(this.browserForTab, uid);
          this.mainContentId = uid;
        }
      } else {
        this._curFrameId = uid;
        this.mainContentId = uid;
      }
    }

    // used to delete sessions
    this.knownFrames.push(uid);
    return remotenessChange;
  }

  /**
   * When navigating between pages results in changing a browser's
   * process, we need to take measures not to lose contact with a listener
   * script. This function does the necessary bookkeeping.
   */
  hasRemotenessChange() {
    // None of these checks are relevant on b2g or if we don't have a tab yet,
    // and may not apply on Fennec.
    if (this.driver.appName != "Firefox" ||
        this.tab === null ||
        this.browserForTab === null) {
      return false;
    }

    if (this._hasRemotenessChange) {
      return true;
    }

    let currentIsRemote = this.browserForTab.isRemoteBrowser;
    this._hasRemotenessChange = this._browserWasRemote !== currentIsRemote;
    this._browserWasRemote = currentIsRemote;
    return this._hasRemotenessChange;
  }

  /**
   * Flushes any pending commands queued when a remoteness change is being
   * processed and mark this remotenessUpdate as complete.
   */
  flushPendingCommands() {
    if (!this._hasRemotenessChange) {
      return;
    }

    this._hasRemotenessChange = false;
    this.pendingCommands.forEach(cb => cb());
    this.pendingCommands = [];
  }

  /**
    * This function intercepts commands interacting with content and queues
    * or executes them as needed.
    *
    * No commands interacting with content are safe to process until
    * the new listener script is loaded and registers itself.
    * This occurs when a command whose effect is asynchronous (such
    * as goBack) results in a remoteness change and new commands
    * are subsequently posted to the server.
    */
  executeWhenReady(cb) {
    if (this.hasRemotenessChange()) {
      this.pendingCommands.push(cb);
    } else {
      cb();
    }
  }

  /**
   * Returns the position of the OS window.
   */
  get position() {
    return {
      x: this.window.screenX,
      y: this.window.screenY,
    };
  }

};

/**
 * The window storage is used to save outer window IDs mapped to weak
 * references of Window objects.
 *
 * Usage:
 *
 *     let wins = new browser.Windows();
 *     wins.set(browser.outerWindowID, window);
 *
 *     ...
 *
 *     let win = wins.get(browser.outerWindowID);
 *
 */
browser.Windows = class extends Map {

  /**
   * Save a weak reference to the Window object.
   *
   * @param {string} id
   *     Outer window ID.
   * @param {Window} win
   *     Window object to save.
   *
   * @return {browser.Windows}
   *     Instance of self.
   */
  set(id, win) {
    let wref = Cu.getWeakReference(win);
    super.set(id, wref);
    return this;
  }

  /**
   * Get the window object stored by provided |id|.
   *
   * @param {string} id
   *     Outer window ID.
   *
   * @return {Window}
   *     Saved window object, or |undefined| if no window is stored by
   *     provided |id|.
   */
  get(id) {
    let wref = super.get(id);
    if (wref) {
      return wref.get();
    }
  }

};
