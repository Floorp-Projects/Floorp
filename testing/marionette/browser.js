/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;

Cu.import("chrome://marionette/content/element.js");
Cu.import("chrome://marionette/content/error.js");
Cu.import("chrome://marionette/content/frame.js");

this.EXPORTED_SYMBOLS = ["browser"];

this.browser = {};

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";


/**
 * Get the <xul:browser> for the specified tab.
 *
 * @param {<xul:tab>} tab
 *     The tab whose browser needs to be returned.
 *
 * @return {<xul:browser>}
 *     The linked browser for the tab or null if no browser can be found.
 */
browser.getBrowserForTab = function (tab) {
  if ("browser" in tab) {
    // Fennec
    return tab.browser;

  } else if ("linkedBrowser" in tab) {
    // Firefox
    return tab.linkedBrowser;

  } else {
    return null;
  }
};

/**
 * Return the tab browser for the specified chrome window.
 *
 * @param {nsIDOMWindow} win
 *     The window whose tabbrowser needs to be accessed.
 *
 * @return {<xul:tabbrowser>}
 *     Tab browser or null if it's not a browser window.
 */
browser.getTabBrowser = function (win) {
  if ("BrowserApp" in win) {
    // Fennec
    return win.BrowserApp;

  } else if ("gBrowser" in win) {
    // Firefox
    return win.gBrowser;

  } else {
    return null;
  }
};

/**
 * Creates a browsing context wrapper.
 *
 * Browsing contexts handle interactions with the browser, according to
 * the current environment (Firefox, Fennec).
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
    this.tabBrowser = browser.getTabBrowser(win);

    this.knownFrames = [];

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
    this._browserWasRemote = null;
    this._hasRemotenessChange = false;
  }

  /**
   * Returns the content browser for the currently selected tab.
   * If there is no tab selected, null will be returned.
   */
  get contentBrowser() {
    if (this.tab) {
      return browser.getBrowserForTab(this.tab);
    }

    return null;
  }

  /**
   * The current frame ID is managed per browser element on desktop in
   * case the ID needs to be refreshed. The currently selected window is
   * identified by a tab.
   */
  get curFrameId() {
    let rv = null;
    if (this.tab) {
      rv = this.getIdForBrowser(this.contentBrowser);
    }
    return rv;
  }

  /**
   * Returns the current URL of the content browser.
   * If no browser is available, null will be returned.
   */
  get currentURL() {
    // Bug 1363368 - contentBrowser could be null until we wait for its
    // initialization been finished
    if (this.contentBrowser) {
      return this.contentBrowser.currentURI.spec;

    } else {
      throw new NoSuchWindowError("Current window does not have a content browser");
    }
  }

  /**
   * Gets the position and dimensions of the top-level browsing context.
   *
   * @return {Map.<string, number>}
   *     Object with |x|, |y|, |width|, and |height| properties.
   */
  get rect() {
    return {
      x: this.window.screenX,
      y: this.window.screenY,
      width: this.window.outerWidth,
      height: this.window.outerHeight,
    };
  }

  /**
   * Retrieves the current tabmodal UI object.  According to the browser
   * associated with the currently selected tab.
   */
  getTabModalUI() {
    let br = this.contentBrowser;
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
   * Close the current window.
   *
   * @return {Promise}
   *     A promise which is resolved when the current window has been closed.
   */
  closeWindow() {
    return new Promise(resolve => {
      this.window.addEventListener("unload", ev => {
        resolve();
      }, {once: true});
      this.window.close();
    });
  }

  /** Called when we start a session with this browser. */
  startSession(newSession, win, callback) {
    callback(win, newSession);
  }

  /**
   * Close the current tab.
   *
   * @return {Promise}
   *     A promise which is resolved when the current tab has been closed.
   *
   * @throws UnsupportedOperationError
   *     If tab handling for the current application isn't supported.
   */
  closeTab() {
    // If the current window is not a browser then close it directly. Do the
    // same if only one remaining tab is open, or no tab selected at all.
    if (!this.tabBrowser || this.tabBrowser.tabs.length === 1 || !this.tab) {
      return this.closeWindow();
    }

    return new Promise((resolve, reject) => {
      if (this.tabBrowser.closeTab) {
        // Fennec
        this.tabBrowser.deck.addEventListener("TabClose", ev => {
          resolve();
        }, {once: true});
        this.tabBrowser.closeTab(this.tab);

      } else if (this.tabBrowser.removeTab) {
        // Firefox
        this.tab.addEventListener("TabClose", ev => {
          resolve();
        }, {once: true});
        this.tabBrowser.removeTab(this.tab);

      } else {
        reject(new UnsupportedOperationError(
            `closeTab() not supported in ${this.driver.appName}`));
      }
    });
  }

  /**
   * Opens a tab with given URI.
   *
   * @param {string} uri
   *      URI to open.
   */
  addTab(uri) {
    return this.tabBrowser.addTab(uri, true);
  }

  /**
   * Set the current tab and update remoteness tracking if a tabbrowser is available.
   *
   * @param {number=} index
   *     Tab index to switch to. If the parameter is undefined,
   *     the currently selected tab will be used.
   * @param {nsIDOMWindow=} win
   *     Switch to this window before selecting the tab.
   * @param {boolean=} focus
   *      A boolean value which determins whether to focus
   *      the window. Defaults to true.
   *
   * @throws UnsupportedOperationError
   *     If tab handling for the current application isn't supported.
   */
  switchToTab(index, win, focus = true) {
    if (win) {
      this.window = win;
      this.tabBrowser = browser.getTabBrowser(win);
    }

    if (!this.tabBrowser) {
      return;
    }

    if (typeof index == "undefined") {
      this.tab = this.tabBrowser.selectedTab;
    } else {
      this.tab = this.tabBrowser.tabs[index];

      if (focus) {
        if (this.tabBrowser.selectTab) {
          // Fennec
          this.tabBrowser.selectTab(this.tab);

        } else if ("selectedTab" in this.tabBrowser) {
          // Firefox
          this.tabBrowser.selectedTab = this.tab;

        } else {
          throw new UnsupportedOperationError("switchToTab() not supported");
        }
      }
    }

    if (this.driver.appName == "Firefox") {
      this._browserWasRemote = this.contentBrowser.isRemoteBrowser;
      this._hasRemotenessChange = false;
    }
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
      if (this.tabBrowser) {
        // If we're setting up a new session on Firefox, we only process the
        // registration for this frame if it belongs to the current tab.
        if (!this.tab) {
          this.switchToTab();
        }

        if (target === this.contentBrowser) {
          this.updateIdForBrowser(this.contentBrowser, uid);
        }
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
    // None of these checks are relevant if we don't have a tab yet,
    // and may not apply on Fennec.
    if (this.driver.appName != "Firefox" ||
        this.tab === null ||
        this.contentBrowser === null) {
      return false;
    }

    if (this._hasRemotenessChange) {
      return true;
    }

    let currentIsRemote = this.contentBrowser.isRemoteBrowser;
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
