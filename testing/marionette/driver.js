/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/* global XPCNativeWrapper */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("chrome://marionette/content/accessibility.js");
Cu.import("chrome://marionette/content/addon.js");
Cu.import("chrome://marionette/content/assert.js");
Cu.import("chrome://marionette/content/atom.js");
const {
  browser,
  Context,
} = Cu.import("chrome://marionette/content/browser.js", {});
Cu.import("chrome://marionette/content/capture.js");
Cu.import("chrome://marionette/content/cert.js");
Cu.import("chrome://marionette/content/cookie.js");
const {
  ChromeWebElement,
  element,
  WebElement,
} = Cu.import("chrome://marionette/content/element.js", {});
const {
  InsecureCertificateError,
  InvalidArgumentError,
  InvalidCookieDomainError,
  InvalidSelectorError,
  NoAlertOpenError,
  NoSuchFrameError,
  NoSuchWindowError,
  SessionNotCreatedError,
  UnknownError,
  UnsupportedOperationError,
  WebDriverError,
} = Cu.import("chrome://marionette/content/error.js", {});
Cu.import("chrome://marionette/content/evaluate.js");
Cu.import("chrome://marionette/content/interaction.js");
Cu.import("chrome://marionette/content/l10n.js");
Cu.import("chrome://marionette/content/legacyaction.js");
Cu.import("chrome://marionette/content/modal.js");
Cu.import("chrome://marionette/content/proxy.js");
Cu.import("chrome://marionette/content/reftest.js");
Cu.import("chrome://marionette/content/session.js");
const {
  PollPromise,
  TimedPromise,
} = Cu.import("chrome://marionette/content/sync.js", {});
const {WindowState} = Cu.import("chrome://marionette/content/wm.js", {});

Cu.importGlobalProperties(["URL"]);

this.EXPORTED_SYMBOLS = ["GeckoDriver"];

const APP_ID_FIREFOX = "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}";

const FRAME_SCRIPT = "chrome://marionette/content/listener.js";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const CLICK_TO_START_PREF = "marionette.debugging.clicktostart";
const CONTENT_LISTENER_PREF = "marionette.contentListener";

const SUPPORTED_STRATEGIES = new Set([
  element.Strategy.ClassName,
  element.Strategy.Selector,
  element.Strategy.ID,
  element.Strategy.TagName,
  element.Strategy.XPath,
  element.Strategy.Anon,
  element.Strategy.AnonAttribute,
]);

const logger = Log.repository.getLogger("Marionette");
const globalMessageManager = Cc["@mozilla.org/globalmessagemanager;1"]
    .getService(Ci.nsIMessageBroadcaster);

/**
 * The Marionette WebDriver services provides a standard conforming
 * implementation of the W3C WebDriver specification.
 *
 * @see {@link https://w3c.github.io/webdriver/webdriver-spec.html}
 * @namespace driver
 */

/**
 * Helper function for converting a {@link nsISimpleEnumerator} to a
 * JavaScript iterator.
 *
 * @memberof driver
 *
 * @param {nsISimpleEnumerator} enumerator
 *     Enumerator to turn into  iterator.
 *
 * @return {Iterable}
 *     Iterator.
 */
function* enumeratorIterator(enumerator) {
  while (enumerator.hasMoreElements()) {
    yield enumerator.getNext();
  }
}

/**
 * Implements (parts of) the W3C WebDriver protocol.  GeckoDriver lives
 * in chrome space and mediates calls to the message listener of the current
 * browsing context's content frame message listener via ListenerProxy.
 *
 * Throughout this prototype, functions with the argument <var>cmd</var>'s
 * documentation refers to the contents of the <code>cmd.parameter</code>
 * object.
 *
 * @class GeckoDriver
 *
 * @param {string} appId
 *     Unique identifier of the application.
 * @param {MarionetteServer} server
 *     The instance of Marionette server.
 */
this.GeckoDriver = function(appId, server) {
  this.appId = appId;
  this._server = server;

  this.sessionID = null;
  this.wins = new browser.Windows();
  this.browsers = {};
  // points to current browser
  this.curBrowser = null;
  // topmost chrome frame
  this.mainFrame = null;
  // chrome iframe that currently has focus
  this.curFrame = null;
  this.currentFrameElement = null;
  this.observing = null;
  this._browserIds = new WeakMap();

  // The curent context decides if commands should affect chrome- or
  // content space.
  this.context = Context.Content;

  this.sandboxes = new Sandboxes(() => this.getCurrentWindow());
  this.legacyactions = new legacyaction.Chain();

  this.capabilities = new session.Capabilities();

  this.mm = globalMessageManager;
  this.listener = proxy.toListener(() => this.mm, this.sendAsync.bind(this),
      () => this.curBrowser);

  // points to an alert instance if a modal dialog is present
  this.dialog = null;
  this.dialogHandler = this.globalModalDialogHandler.bind(this);
};

Object.defineProperty(GeckoDriver.prototype, "a11yChecks", {
  get() {
    return this.capabilities.get("moz:accessibilityChecks");
  },
});

/**
 * Returns the current URL of the ChromeWindow or content browser,
 * depending on context.
 *
 * @return {URL}
 *     Read-only property containing the currently loaded URL.
 */
Object.defineProperty(GeckoDriver.prototype, "currentURL", {
  get() {
    switch (this.context) {
      case Context.Chrome:
        let chromeWin = this.getCurrentWindow();
        return new URL(chromeWin.location.href);

      case Context.Content:
        return new URL(this.curBrowser.currentURI.spec);

      default:
        throw new TypeError(`Unknown context: ${this.context}`);
    }
  },
});

Object.defineProperty(GeckoDriver.prototype, "title", {
  get() {
    switch (this.context) {
      case Context.Chrome:
        let chromeWin = this.getCurrentWindow();
        return chromeWin.document.documentElement.getAttribute("title");

      case Context.Content:
        return this.curBrowser.currentTitle;

      default:
        throw new TypeError(`Unknown context: ${this.context}`);
    }
  },
});

Object.defineProperty(GeckoDriver.prototype, "proxy", {
  get() {
    return this.capabilities.get("proxy");
  },
});

Object.defineProperty(GeckoDriver.prototype, "secureTLS", {
  get() {
    return !this.capabilities.get("acceptInsecureCerts");
  },
});

Object.defineProperty(GeckoDriver.prototype, "timeouts", {
  get() {
    return this.capabilities.get("timeouts");
  },

  set(newTimeouts) {
    this.capabilities.set("timeouts", newTimeouts);
  },
});

Object.defineProperty(GeckoDriver.prototype, "windows", {
  get() {
    return enumeratorIterator(Services.wm.getEnumerator(null));
  },
});

Object.defineProperty(GeckoDriver.prototype, "windowType", {
  get() {
    return this.curBrowser.window.document.documentElement.getAttribute("windowtype");
  },
});

Object.defineProperty(GeckoDriver.prototype, "windowHandles", {
  get() {
    let hs = [];

    for (let win of this.windows) {
      let tabBrowser = browser.getTabBrowser(win);

      // Only return handles for browser windows
      if (tabBrowser && tabBrowser.tabs) {
        tabBrowser.tabs.forEach(tab => {
          let winId = this.getIdForBrowser(browser.getBrowserForTab(tab));
          if (winId !== null) {
            hs.push(winId);
          }
        });
      }
    }

    return hs;
  },
});

Object.defineProperty(GeckoDriver.prototype, "chromeWindowHandles", {
  get() {
    let hs = [];

    for (let win of this.windows) {
      hs.push(getOuterWindowId(win));
    }

    return hs;
  },
});

GeckoDriver.prototype.QueryInterface = XPCOMUtils.generateQI([
  Ci.nsIMessageListener,
  Ci.nsIObserver,
  Ci.nsISupportsWeakReference,
]);

GeckoDriver.prototype.init = function() {
  this.mm.addMessageListener("Marionette:WebDriver:GetCapabilities", this);
  this.mm.addMessageListener("Marionette:GetLogLevel", this);
  this.mm.addMessageListener("Marionette:getVisibleCookies", this);
  this.mm.addMessageListener("Marionette:ListenersAttached", this);
  this.mm.addMessageListener("Marionette:Register", this);
  this.mm.addMessageListener("Marionette:switchedToFrame", this);
};

GeckoDriver.prototype.uninit = function() {
  this.mm.removeMessageListener("Marionette:WebDriver:GetCapabilities", this);
  this.mm.removeMessageListener("Marionette:GetLogLevel", this);
  this.mm.removeMessageListener("Marionette:getVisibleCookies", this);
  this.mm.removeMessageListener("Marionette:ListenersAttached", this);
  this.mm.removeMessageListener("Marionette:Register", this);
  this.mm.removeMessageListener("Marionette:switchedToFrame", this);
};

/**
 * Callback used to observe the creation of new modal or tab modal dialogs
 * during the session's lifetime.
 */
GeckoDriver.prototype.globalModalDialogHandler = function(subject, topic) {
  let winr;
  if (topic === modal.COMMON_DIALOG_LOADED) {
    // Always keep a weak reference to the current dialog
    winr = Cu.getWeakReference(subject);
  }
  this.dialog = new modal.Dialog(() => this.curBrowser, winr);
};

/**
 * Helper method to send async messages to the content listener.
 * Correct usage is to pass in the name of a function in listener.js,
 * a serialisable object, and optionally the current command's ID
 * when not using the modern dispatching technique.
 *
 * @param {string} name
 *     Suffix of the target message handler <tt>Marionette:SUFFIX</tt>.
 * @param {Object=} data
 *     Data that must be serialisable using {@link evaluate.toJSON}.
 * @param {number=} commandID
 *     Optional command ID to ensure synchronisity.
 *
 * @throws {JavaScriptError}
 *     If <var>data</var> could not be marshaled.
 * @throws {NoSuchWindowError}
 *     If there is no current target frame.
 */
GeckoDriver.prototype.sendAsync = function(name, data, commandID) {
  let payload = evaluate.toJSON(data, this.seenEls);

  // TODO(ato): When proxy.AsyncMessageChannel
  // is used for all chrome <-> content communication
  // this can be removed.
  if (commandID) {
    payload.commandID = commandID;
  }

  this.curBrowser.executeWhenReady(() => {
    if (this.curBrowser.curFrameId) {
      let target = `Marionette:${name}`;
      this.curBrowser.messageManager.sendAsyncMessage(target, payload);
    } else {
      throw new NoSuchWindowError(
          "No such content frame; perhaps the listener was not registered?");
    }
  });
};

/**
 * Get the session's current top-level browsing context.
 *
 * It will return the outer {@link ChromeWindow} previously selected by
 * window handle through {@link #switchToWindow}, or the first window that
 * was registered.
 *
 * @param {Context=} forcedContext
 *     Optional name of the context to use for finding the window.
 *     It will be required if a command always needs a specific context,
 *     whether which context is currently set. Defaults to the current
 *     context.
 *
 * @return {ChromeWindow}
 *     The current top-level browsing context.
 */
GeckoDriver.prototype.getCurrentWindow = function(forcedContext = undefined) {
  let context = typeof forcedContext == "undefined" ? this.context : forcedContext;
  let win = null;

  switch (context) {
    case Context.Chrome:
      if (this.curFrame !== null) {
        win = this.curFrame;
      } else if (this.curBrowser !== null) {
        win = this.curBrowser.window;
      }
      break;

    case Context.Content:
      if (this.curFrame !== null) {
        win = this.curFrame;
      } else if (this.curBrowser !== null && this.curBrowser.contentBrowser) {
        win = this.curBrowser.window;
      }
      break;
  }

  return win;
};

GeckoDriver.prototype.isReftestBrowser = function(element) {
  return this._reftest &&
    element &&
    element.tagName === "xul:browser" &&
    element.parentElement &&
    element.parentElement.id === "reftest";
};

GeckoDriver.prototype.addFrameCloseListener = function(action) {
  let win = this.getCurrentWindow();
  this.mozBrowserClose = e => {
    if (e.target.id == this.oopFrameId) {
      win.removeEventListener("mozbrowserclose", this.mozBrowserClose, true);
      throw new NoSuchWindowError("The window closed during action: " + action);
    }
  };
  win.addEventListener("mozbrowserclose", this.mozBrowserClose, true);
};

/**
 * Create a new browsing context for window and add to known browsers.
 *
 * @param {ChromeWindow} win
 *     Window for which we will create a browsing context.
 *
 * @return {string}
 *     Returns the unique server-assigned ID of the window.
 */
GeckoDriver.prototype.addBrowser = function(window) {
  let bc = new browser.Context(window, this);
  let winId = getOuterWindowId(window);

  this.browsers[winId] = bc;
  this.curBrowser = this.browsers[winId];
  if (!this.wins.has(winId)) {
    // add this to seenItems so we can guarantee
    // the user will get winId as this window's id
    this.wins.set(winId, window);
  }
};

/**
 * Registers a new browser, win, with Marionette.
 *
 * If we have not seen the browser content window before, the listener
 * frame script will be loaded into it.  If isNewSession is true, we will
 * switch focus to the start frame when it registers.
 *
 * @param {ChromeWindow} win
 *     Window whose browser we need to access.
 * @param {boolean=} [false] isNewSession
 *     True if this is the first time we're talking to this browser.
 */
GeckoDriver.prototype.startBrowser = function(window, isNewSession = false) {
  this.mainFrame = window;
  this.curFrame = null;
  this.addBrowser(window);
  this.whenBrowserStarted(window, isNewSession);
};

/**
 * Callback invoked after a new session has been started in a browser.
 * Loads the Marionette frame script into the browser if needed.
 *
 * @param {ChromeWindow} window
 *     Window whose browser we need to access.
 * @param {boolean} isNewSession
 *     True if this is the first time we're talking to this browser.
 */
GeckoDriver.prototype.whenBrowserStarted = function(window, isNewSession) {
  let mm = window.messageManager;
  if (mm) {
    if (!isNewSession) {
      // Loading the frame script corresponds to a situation we need to
      // return to the server. If the messageManager is a message broadcaster
      // with no children, we don't have a hope of coming back from this
      // call, so send the ack here. Otherwise, make a note of how many
      // child scripts will be loaded so we known when it's safe to return.
      // Child managers may not have child scripts yet (e.g. socialapi),
      // only count child managers that have children, but only count the top
      // level children as they are the ones that we expect a response from.
      if (mm.childCount !== 0) {
        this.curBrowser.frameRegsPending = 0;
        for (let i = 0; i < mm.childCount; i++) {
          if (mm.getChildAt(i).childCount !== 0) {
            this.curBrowser.frameRegsPending += 1;
          }
        }
      }
    }

    if (!Preferences.get(CONTENT_LISTENER_PREF) || !isNewSession) {
      // load listener into the remote frame
      // and any applicable new frames
      // opened after this call
      mm.loadFrameScript(FRAME_SCRIPT, true);
      Preferences.set(CONTENT_LISTENER_PREF, true);
    }
  } else {
    logger.error("Unable to load content frame script");
  }
};

/**
 * Recursively get all labeled text.
 *
 * @param {Element} el
 *     The parent element.
 * @param {Array.<string>} lines
 *      Array that holds the text lines.
 */
GeckoDriver.prototype.getVisibleText = function(el, lines) {
  try {
    if (atom.isElementDisplayed(el, this.getCurrentWindow())) {
      if (el.value) {
        lines.push(el.value);
      }
      for (let child in el.childNodes) {
        this.getVisibleText(el.childNodes[child], lines);
      }
    }
  } catch (e) {
    if (el.nodeName == "#text") {
      lines.push(el.textContent);
    }
  }
};

/**
 * Handles registration of new content listener browsers.  Depending on
 * their type they are either accepted or ignored.
 */
GeckoDriver.prototype.registerBrowser = function(id, be) {
  let nullPrevious = this.curBrowser.curFrameId === null;
  let listenerWindow = Services.wm.getOuterWindowWithId(id);

  // We want to ignore frames that are XUL browsers that aren't in the "main"
  // tabbrowser, but accept things on Fennec (which doesn't have a
  // xul:tabbrowser), and accept HTML iframes (because tests depend on it),
  // as well as XUL frames. Ideally this should be cleaned up and we should
  // keep track of browsers a different way.
  if (this.appId != APP_ID_FIREFOX || be.namespaceURI != XUL_NS ||
      be.nodeName != "browser" || be.getTabBrowser()) {
    // curBrowser holds all the registered frames in knownFrames
    this.curBrowser.register(id, be);
  }

  this.wins.set(id, listenerWindow);
  if (nullPrevious && (this.curBrowser.curFrameId !== null)) {
    this.sendAsync("newSession");
  }

  return id;
};

GeckoDriver.prototype.registerPromise = function() {
  const li = "Marionette:Register";

  return new Promise(resolve => {
    let cb = msg => {
      let wid = msg.json.value;
      let be = msg.target;
      let outerWindowID = this.registerBrowser(wid, be);

      if (this.curBrowser.frameRegsPending > 0) {
        this.curBrowser.frameRegsPending--;
      }

      if (this.curBrowser.frameRegsPending === 0) {
        this.mm.removeMessageListener(li, cb);
        resolve();
      }

      // this is a sync message and listeners expect the ID back
      return outerWindowID;
    };
    this.mm.addMessageListener(li, cb);
  });
};

GeckoDriver.prototype.listeningPromise = function() {
  const li = "Marionette:ListenersAttached";

  return new Promise(resolve => {
    let cb = msg => {
      if (msg.json.outerWindowID === this.curBrowser.curFrameId) {
        this.mm.removeMessageListener(li, cb);
        resolve();
      }
    };
    this.mm.addMessageListener(li, cb);
  });
};

/**
 * Create a new WebDriver session.
 *
 * It is expected that the caller performs the necessary checks on
 * the requested capabilities to be WebDriver conforming.  The WebDriver
 * service offered by Marionette does not match or negotiate capabilities
 * beyond type- and bounds checks.
 *
 * <h3>Capabilities</h3>
 *
 * <dl>
 *  <dt><code>pageLoadStrategy</code> (string)
 *  <dd>The page load strategy to use for the current session.  Must be
 *   one of "<tt>none</tt>", "<tt>eager</tt>", and "<tt>normal</tt>".
 *
 *  <dt><code>acceptInsecureCerts</code> (boolean)
 *  <dd>Indicates whether untrusted and self-signed TLS certificates
 *   are implicitly trusted on navigation for the duration of the session.
 *
 *  <dt><code>timeouts</code> (Timeouts object)
 *  <dd>Describes the timeouts imposed on certian session operations.
 *
 *  <dt><code>proxy</code> (Proxy object)
 *  <dd>Defines the proxy configuration.
 *
 *  <dt><code>moz:accessibilityChecks</code> (boolean)
 *  <dd>Run a11y checks when clicking elements.
 *
 *  <dt><code>moz:webdriverClick</code> (boolean)
 *  <dd>Use a WebDriver conforming <i>WebDriver::ElementClick</i>.
 * </dl>
 *
 * <h4>Timeouts object</h4>
 *
 * <dl>
 *  <dt><code>script</code> (number)
 *  <dd>Determines when to interrupt a script that is being evaluates.
 *
 *  <dt><code>pageLoad</code> (number)
 *  <dd>Provides the timeout limit used to interrupt navigation of the
 *   browsing context.
 *
 *  <dt><code>implicit</code> (number)
 *  <dd>Gives the timeout of when to abort when locating an element.
 * </dl>
 *
 * <h4>Proxy object</h4>
 *
 * <dl>
 *  <dt><code>proxyType</code> (string)
 *  <dd>Indicates the type of proxy configuration.  Must be one
 *   of "<tt>pac</tt>", "<tt>direct</tt>", "<tt>autodetect</tt>",
 *   "<tt>system</tt>", or "<tt>manual</tt>".
 *
 *  <dt><code>proxyAutoconfigUrl</code> (string)
 *  <dd>Defines the URL for a proxy auto-config file if
 *   <code>proxyType</code> is equal to "<tt>pac</tt>".
 *
 *  <dt><code>ftpProxy</code> (string)
 *  <dd>Defines the proxy host for FTP traffic when the
 *   <code>proxyType</code> is "<tt>manual</tt>".
 *
 *  <dt><code>httpProxy</code> (string)
 *  <dd>Defines the proxy host for HTTP traffic when the
 *   <code>proxyType</code> is "<tt>manual</tt>".
 *
 *  <dt><code>noProxy</code> (string)
 *  <dd>Lists the adress for which the proxy should be bypassed when
 *   the <code>proxyType</code> is "<tt>manual</tt>".  Must be a JSON
 *   List containing any number of any of domains, IPv4 addresses, or IPv6
 *   addresses.
 *
 *  <dt><code>sslProxy</code> (string)
 *  <dd>Defines the proxy host for encrypted TLS traffic when the
 *   <code>proxyType</code> is "<tt>manual</tt>".
 *
 *  <dt><code>socksProxy</code> (string)
 *  <dd>Defines the proxy host for a SOCKS proxy traffic when the
 *   <code>proxyType</code> is "<tt>manual</tt>".
 *
 *  <dt><code>socksVersion</code> (string)
 *  <dd>Defines the SOCKS proxy version when the <code>proxyType</code> is
 *   "<tt>manual</tt>".  It must be any integer between 0 and 255
 *   inclusive.
 * </dl>
 *
 * <h3>Example</h3>
 *
 * Input:
 *
 * <pre><code>
 *     {"capabilities": {"acceptInsecureCerts": true}}
 * </code></pre>
 *
 * @param {string=} sessionId
 *     Normally a unique ID is given to a new session, however this can
 *     be overriden by providing this field.
 * @param {Object.<string, *>=} capabilities
 *     JSON Object containing any of the recognised capabilities listed
 *     above.
 *
 * @return {Object}
 *     Session ID and capabilities offered by the WebDriver service.
 *
 * @throws {SessionNotCreatedError}
 *     If, for whatever reason, a session could not be created.
 */
GeckoDriver.prototype.newSession = async function(cmd) {
  if (this.sessionID) {
    throw new SessionNotCreatedError("Maximum number of active sessions");
  }
  this.sessionID = WebElement.generateUUID();

  try {
    this.capabilities = session.Capabilities.fromJSON(cmd.parameters);

    if (!this.secureTLS) {
      logger.warn("TLS certificate errors will be ignored for this session");
      let acceptAllCerts = new cert.InsecureSweepingOverride();
      cert.installOverride(acceptAllCerts);
    }

    if (this.proxy.init()) {
      logger.info("Proxy settings initialised: " + JSON.stringify(this.proxy));
    }
  } catch (e) {
    throw new SessionNotCreatedError(e);
  }

  // If we are testing accessibility with marionette, start a11y service in
  // chrome first. This will ensure that we do not have any content-only
  // services hanging around.
  if (this.a11yChecks && accessibility.service) {
    logger.info("Preemptively starting accessibility service in Chrome");
  }

  let registerBrowsers = this.registerPromise();
  let browserListening = this.listeningPromise();

  let waitForWindow = function() {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    if (!win) {
      // if the window isn't even created, just poll wait for it
      let checkTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      checkTimer.initWithCallback(waitForWindow.bind(this), 100,
          Ci.nsITimer.TYPE_ONE_SHOT);
    } else if (win.document.readyState != "complete") {
      // otherwise, wait for it to be fully loaded before proceeding
      let listener = ev => {
        // ensure that we proceed, on the top level document load event
        // (not an iframe one...)
        if (ev.target != win.document) {
          return;
        }
        win.removeEventListener("load", listener);
        waitForWindow.call(this);
      };
      win.addEventListener("load", listener, true);
    } else {
      let clickToStart = Preferences.get(CLICK_TO_START_PREF);
      if (clickToStart) {
        Services.prompt.alert(win, "", "Click to start execution of marionette tests");
      }
      this.startBrowser(win, true);
    }
  };

  if (!Preferences.get(CONTENT_LISTENER_PREF)) {
    waitForWindow.call(this);
  } else if (this.appId != APP_ID_FIREFOX && this.curBrowser === null) {
    // if there is a content listener, then we just wake it up
    let win = this.getCurrentWindow();
    this.addBrowser(win);
    this.whenBrowserStarted(win, false);
  } else {
    throw new WebDriverError("Session already running");
  }

  await registerBrowsers;
  await browserListening;

  if (this.curBrowser.tab) {
    this.curBrowser.contentBrowser.focus();
  }

  // Setup global listener for modal dialogs, and check if there is already
  // one open for the currently selected browser window.
  modal.addHandler(this.dialogHandler);
  this.dialog = modal.findModalDialogs(this.curBrowser);

  return {
    sessionId: this.sessionID,
    capabilities: this.capabilities,
  };
};

/**
 * Send the current session's capabilities to the client.
 *
 * Capabilities informs the client of which WebDriver features are
 * supported by Firefox and Marionette.  They are immutable for the
 * length of the session.
 *
 * The return value is an immutable map of string keys
 * ("capabilities") to values, which may be of types boolean,
 * numerical or string.
 */
GeckoDriver.prototype.getSessionCapabilities = function(cmd, resp) {
  resp.body.capabilities = this.capabilities;
};

/**
 * Sets the context of the subsequent commands.
 *
 * All subsequent requests to commands that in some way involve
 * interaction with a browsing context will target the chosen browsing
 * context.
 *
 * @param {string} value
 *     Name of the context to be switched to.  Must be one of "chrome" or
 *     "content".
 *
 * @throws {InvalidArgumentError}
 *     If <var>value</var> is not a string.
 * @throws {WebDriverError}
 *     If <var>value</var> is not a valid browsing context.
 */
GeckoDriver.prototype.setContext = function(cmd) {
  let value = assert.string(cmd.parameters.value);
  this.context = Context.fromString(value);
};

/**
 * Gets the context type that is Marionette's current target for
 * browsing context scoped commands.
 *
 * You may choose a context through the {@link #setContext} command.
 *
 * The default browsing context is {@link Context.Content}.
 *
 * @return {Context}
 *     Current context.
 */
GeckoDriver.prototype.getContext = function() {
  return this.context;
};

/**
 * Executes a JavaScript function in the context of the current browsing
 * context, if in content space, or in chrome space otherwise, and returns
 * the return value of the function.
 *
 * It is important to note that if the <var>sandboxName</var> parameter
 * is left undefined, the script will be evaluated in a mutable sandbox,
 * causing any change it makes on the global state of the document to have
 * lasting side-effects.
 *
 * @param {string} script
 *     Script to evaluate as a function body.
 * @param {Array.<(string|boolean|number|object|WebElement)>} args
 *     Arguments exposed to the script in <code>arguments</code>.
 *     The array items must be serialisable to the WebDriver protocol.
 * @param {number} scriptTimeout
 *     Duration in milliseconds of when to interrupt and abort the
 *     script evaluation.
 * @param {string=} sandbox
 *     Name of the sandbox to evaluate the script in.  The sandbox is
 *     cached for later re-use on the same Window object if
 *     <var>newSandbox</var> is false.  If he parameter is undefined,
 *     the script is evaluated in a mutable sandbox.  If the parameter
 *     is "system", it will be evaluted in a sandbox with elevated system
 *     privileges, equivalent to chrome space.
 * @param {boolean=} newSandbox
 *     Forces the script to be evaluated in a fresh sandbox.  Note that if
 *     it is undefined, the script will normally be evaluted in a fresh
 *     sandbox.
 * @param {string=} filename
 *     Filename of the client's program where this script is evaluated.
 * @param {number=} line
 *     Line in the client's program where this script is evaluated.
 * @param {boolean=} debug_script
 *     Attach an <code>onerror</code> event handler on the {@link Window}
 *     object.  It does not differentiate content errors from chrome errors.
 * @param {boolean=} directInject
 *     Evaluate the script without wrapping it in a function.
 *
 * @return {(string|boolean|number|object|WebElement)}
 *     Return value from the script, or null which signifies either the
 *     JavaScript notion of null or undefined.
 *
 * @throws {ScriptTimeoutError}
 *     If the script was interrupted due to reaching the
 *     <var>scriptTimeout</var> or default timeout.
 * @throws {JavaScriptError}
 *     If an {@link Error} was thrown whilst evaluating the script.
 */
GeckoDriver.prototype.executeScript = async function(cmd, resp) {
  assert.window(this.getCurrentWindow());

  let {script, args, scriptTimeout} = cmd.parameters;
  scriptTimeout = scriptTimeout || this.timeouts.script;

  let opts = {
    sandboxName: cmd.parameters.sandbox,
    newSandbox: !!(typeof cmd.parameters.newSandbox == "undefined") ||
        cmd.parameters.newSandbox,
    file: cmd.parameters.filename,
    line: cmd.parameters.line,
    debug: cmd.parameters.debug_script,
  };

  resp.body.value = await this.execute_(script, args, scriptTimeout, opts);
};

/**
 * Executes a JavaScript function in the context of the current browsing
 * context, if in content space, or in chrome space otherwise, and returns
 * the object passed to the callback.
 *
 * The callback is always the last argument to the <var>arguments</var>
 * list passed to the function scope of the script.  It can be retrieved
 * as such:
 *
 * <pre><code>
 *     let callback = arguments[arguments.length - 1];
 *     callback("foo");
 *     // "foo" is returned
 * </code></pre>
 *
 * It is important to note that if the <var>sandboxName</var> parameter
 * is left undefined, the script will be evaluated in a mutable sandbox,
 * causing any change it makes on the global state of the document to have
 * lasting side-effects.
 *
 * @param {string} script
 *     Script to evaluate as a function body.
 * @param {Array.<(string|boolean|number|object|WebElement)>} args
 *     Arguments exposed to the script in <code>arguments</code>.
 *     The array items must be serialisable to the WebDriver protocol.
 * @param {number} scriptTimeout
 *     Duration in milliseconds of when to interrupt and abort the
 *     script evaluation.
 * @param {string=} sandbox
 *     Name of the sandbox to evaluate the script in.  The sandbox is
 *     cached for later re-use on the same Window object if
 *     <var>newSandbox</var> is false.  If the parameter is undefined,
 *     the script is evaluated in a mutable sandbox.  If the parameter
 *     is "system", it will be evaluted in a sandbox with elevated system
 *     privileges, equivalent to chrome space.
 * @param {boolean=} newSandbox
 *     Forces the script to be evaluated in a fresh sandbox.  Note that if
 *     it is undefined, the script will normally be evaluted in a fresh
 *     sandbox.
 * @param {string=} filename
 *     Filename of the client's program where this script is evaluated.
 * @param {number=} line
 *     Line in the client's program where this script is evaluated.
 * @param {boolean=} debug_script
 *     Attach an <code>onerror</code> event handler on the {@link Window}
 *     object.  It does not differentiate content errors from chrome errors.
 * @param {boolean=} directInject
 *     Evaluate the script without wrapping it in a function.
 *
 * @return {(string|boolean|number|object|WebElement)}
 *     Return value from the script, or null which signifies either the
 *     JavaScript notion of null or undefined.
 *
 * @throws {ScriptTimeoutError}
 *     If the script was interrupted due to reaching the
 *     <var>scriptTimeout</var> or default timeout.
 * @throws {JavaScriptError}
 *     If an Error was thrown whilst evaluating the script.
 */
GeckoDriver.prototype.executeAsyncScript = async function(cmd, resp) {
  assert.window(this.getCurrentWindow());

  let {script, args, scriptTimeout} = cmd.parameters;
  scriptTimeout = scriptTimeout || this.timeouts.script;

  let opts = {
    sandboxName: cmd.parameters.sandbox,
    newSandbox: !!(typeof cmd.parameters.newSandbox == "undefined") ||
        cmd.parameters.newSandbox,
    file: cmd.parameters.filename,
    line: cmd.parameters.line,
    debug: cmd.parameters.debug_script,
    async: true,
  };

  resp.body.value = await this.execute_(script, args, scriptTimeout, opts);
};

GeckoDriver.prototype.execute_ = async function(
    script, args, timeout, opts = {}) {
  let res, els;

  switch (this.context) {
    case Context.Content:
      // evaluate in content with lasting side-effects
      if (!opts.sandboxName) {
        res = await this.listener.execute(script, args, timeout, opts);

      // evaluate in content with sandbox
      } else {
        res = await this.listener.executeInSandbox(
            script, args, timeout, opts);
      }

      break;

    case Context.Chrome:
      let sb = this.sandboxes.get(opts.sandboxName, opts.newSandbox);
      opts.timeout = timeout;
      let wargs = evaluate.fromJSON(args, this.curBrowser.seenEls, sb.window);
      res = await evaluate.sandbox(sb, script, wargs, opts);
      els = this.curBrowser.seenEls;
      break;

    default:
      throw new TypeError(`Unknown context: ${this.context}`);
  }

  return evaluate.toJSON(res, els);
};

/**
 * Navigate to given URL.
 *
 * Navigates the current browsing context to the given URL and waits for
 * the document to load or the session's page timeout duration to elapse
 * before returning.
 *
 * The command will return with a failure if there is an error loading
 * the document or the URL is blocked.  This can occur if it fails to
 * reach host, the URL is malformed, or if there is a certificate issue
 * to name some examples.
 *
 * The document is considered successfully loaded when the
 * DOMContentLoaded event on the frame element associated with the
 * current window triggers and document.readyState is "complete".
 *
 * In chrome context it will change the current window's location to
 * the supplied URL and wait until document.readyState equals "complete"
 * or the page timeout duration has elapsed.
 *
 * @param {string} url
 *     URL to navigate to.
 *
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.get = async function(cmd) {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let url = cmd.parameters.url;

  let get = this.listener.get({url, pageTimeout: this.timeouts.pageLoad});

  // If a reload of the frame script interrupts our page load, this will
  // never return. We need to re-issue this request to correctly poll for
  // readyState and send errors.
  this.curBrowser.pendingCommands.push(() => {
    let parameters = {
      // TODO(ato): Bug 1242595
      commandID: this.listener.activeMessageId,
      pageTimeout: this.timeouts.pageLoad,
      startTime: new Date().getTime(),
    };
    this.curBrowser.messageManager.sendAsyncMessage(
        "Marionette:waitForPageLoaded", parameters);
  });

  await get;

  this.curBrowser.contentBrowser.focus();
};

/**
 * Get a string representing the current URL.
 *
 * On Desktop this returns a string representation of the URL of the
 * current top level browsing context.  This is equivalent to
 * document.location.href.
 *
 * When in the context of the chrome, this returns the canonical URL
 * of the current resource.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getCurrentUrl = function() {
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  return this.currentURL.toString();
};

/**
 * Gets the current title of the window.
 *
 * @return {string}
 *     Document title of the top-level browsing context.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getTitle = function() {
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  return this.title;
};

/** Gets the current type of the window. */
GeckoDriver.prototype.getWindowType = function(cmd, resp) {
  assert.window(this.getCurrentWindow());

  resp.body.value = this.windowType;
};

/**
 * Gets the page source of the content document.
 *
 * @return {string}
 *     String serialisation of the DOM of the current browsing context's
 *     active document.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getPageSource = async function(cmd, resp) {
  const win = assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  switch (this.context) {
    case Context.Chrome:
      let s = new win.XMLSerializer();
      resp.body.value = s.serializeToString(win.document);
      break;

    case Context.Content:
      resp.body.value = await this.listener.getPageSource();
      break;
  }
};

/**
 * Cause the browser to traverse one step backward in the joint history
 * of the current browsing context.
 *
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.goBack = async function() {
  assert.content(this.context);
  assert.contentBrowser(this.curBrowser);
  this._assertAndDismissModal();

  // If there is no history, just return
  if (!this.curBrowser.contentBrowser.webNavigation.canGoBack) {
    return;
  }

  let lastURL = this.currentURL;
  let goBack = this.listener.goBack({pageTimeout: this.timeouts.pageLoad});

  // If a reload of the frame script interrupts our page load, this will
  // never return. We need to re-issue this request to correctly poll for
  // readyState and send errors.
  this.curBrowser.pendingCommands.push(() => {
    let parameters = {
      // TODO(ato): Bug 1242595
      commandID: this.listener.activeMessageId,
      lastSeenURL: lastURL.toString(),
      pageTimeout: this.timeouts.pageLoad,
      startTime: new Date().getTime(),
    };
    this.curBrowser.messageManager.sendAsyncMessage(
        "Marionette:waitForPageLoaded", parameters);
  });

  await goBack;
};

/**
 * Cause the browser to traverse one step forward in the joint history
 * of the current browsing context.
 *
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.goForward = async function() {
  assert.content(this.context);
  assert.contentBrowser(this.curBrowser);
  this._assertAndDismissModal();

  // If there is no history, just return
  if (!this.curBrowser.contentBrowser.webNavigation.canGoForward) {
    return;
  }

  let lastURL = this.currentURL;
  let goForward = this.listener.goForward(
      {pageTimeout: this.timeouts.pageLoad});

  // If a reload of the frame script interrupts our page load, this will
  // never return. We need to re-issue this request to correctly poll for
  // readyState and send errors.
  this.curBrowser.pendingCommands.push(() => {
    let parameters = {
      // TODO(ato): Bug 1242595
      commandID: this.listener.activeMessageId,
      lastSeenURL: lastURL.toString(),
      pageTimeout: this.timeouts.pageLoad,
      startTime: new Date().getTime(),
    };
    this.curBrowser.messageManager.sendAsyncMessage(
        "Marionette:waitForPageLoaded", parameters);
  });

  await goForward;
};

/**
 * Causes the browser to reload the page in current top-level browsing
 * context.
 *
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.refresh = async function() {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let refresh = this.listener.refresh(
      {pageTimeout: this.timeouts.pageLoad});

  // If a reload of the frame script interrupts our page load, this will
  // never return. We need to re-issue this request to correctly poll for
  // readyState and send errors.
  this.curBrowser.pendingCommands.push(() => {
    let parameters = {
      // TODO(ato): Bug 1242595
      commandID: this.listener.activeMessageId,
      pageTimeout: this.timeouts.pageLoad,
      startTime: new Date().getTime(),
    };
    this.curBrowser.messageManager.sendAsyncMessage(
        "Marionette:waitForPageLoaded", parameters);
  });

  await refresh;
};

/**
 * Forces an update for the given browser's id.
 */
GeckoDriver.prototype.updateIdForBrowser = function(browser, newId) {
  this._browserIds.set(browser.permanentKey, newId);
};

/**
 * Retrieves a listener id for the given xul browser element. In case
 * the browser is not known, an attempt is made to retrieve the id from
 * a CPOW, and null is returned if this fails.
 */
GeckoDriver.prototype.getIdForBrowser = function(browser) {
  if (browser === null) {
    return null;
  }
  let permKey = browser.permanentKey;
  if (this._browserIds.has(permKey)) {
    return this._browserIds.get(permKey);
  }

  let winId = browser.outerWindowID;
  if (winId) {
    this._browserIds.set(permKey, winId);
    return winId;
  }
  return null;
},

/**
 * Get the current window's handle. On desktop this typically corresponds
 * to the currently selected tab.
 *
 * Return an opaque server-assigned identifier to this window that
 * uniquely identifies it within this Marionette instance.  This can
 * be used to switch to this window at a later point.
 *
 * @return {string}
 *     Unique window handle.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 */
GeckoDriver.prototype.getWindowHandle = function() {
  assert.contentBrowser(this.curBrowser);

  return this.curBrowser.curFrameId.toString();
};

/**
 * Get a list of top-level browsing contexts. On desktop this typically
 * corresponds to the set of open tabs for browser windows, or the window
 * itself for non-browser chrome windows.
 *
 * Each window handle is assigned by the server and is guaranteed unique,
 * however the return array does not have a specified ordering.
 *
 * @return {Array.<string>}
 *     Unique window handles.
 */
GeckoDriver.prototype.getWindowHandles = function() {
  return this.windowHandles.map(String);
};

/**
 * Get the current window's handle.  This corresponds to a window that
 * may itself contain tabs.
 *
 * Return an opaque server-assigned identifier to this window that
 * uniquely identifies it within this Marionette instance.  This can
 * be used to switch to this window at a later point.
 *
 * @return {string}
 *     Unique window handle.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 */
GeckoDriver.prototype.getChromeWindowHandle = function(cmd, resp) {
  assert.window(this.getCurrentWindow(Context.Chrome));

  for (let i in this.browsers) {
    if (this.curBrowser == this.browsers[i]) {
      resp.body.value = i;
      return;
    }
  }
};

/**
 * Returns identifiers for each open chrome window for tests interested in
 * managing a set of chrome windows and tabs separately.
 *
 * @return {Array.<string>}
 *     Unique window handles.
 */
GeckoDriver.prototype.getChromeWindowHandles = function() {
  return this.chromeWindowHandles.map(String);
};

/**
 * Get the current position and size of the browser window currently in focus.
 *
 * Will return the current browser window size in pixels. Refers to
 * window outerWidth and outerHeight values, which include scroll bars,
 * title bars, etc.
 *
 * @return {Object.<string, number>}
 *     Object with |x| and |y| coordinates, and |width| and |height|
 *     of browser window.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getWindowRect = function() {
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();
  return this.curBrowser.rect;
};

/**
 * Set the window position and size of the browser on the operating
 * system window manager.
 *
 * The supplied |width| and |height| values refer to the window outerWidth
 * and outerHeight values, which include browser chrome and OS-level
 * window borders.
 *
 * @param {number} x
 *     X coordinate of the top/left of the window that it will be
 *     moved to.
 * @param {number} y
 *     Y coordinate of the top/left of the window that it will be
 *     moved to.
 * @param {number} width
 *     Width to resize the window to.
 * @param {number} height
 *     Height to resize the window to.
 *
 * @return {Object.<string, number>}
 *     Object with |x| and |y| coordinates and |width| and |height|
 *     dimensions.
 *
 * @throws {UnsupportedOperationError}
 *     Not applicable to application.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.setWindowRect = async function(cmd) {
  assert.firefox();
  const win = assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let {x, y, width, height} = cmd.parameters;
  let origRect = this.curBrowser.rect;

  // Synchronous resize to |width| and |height| dimensions.
  async function resizeWindow(width, height) {
    return new Promise(resolve => {
      win.addEventListener("resize", whenIdle(win, resolve), {once: true});
      win.resizeTo(width, height);
    });
  }

  // Wait until window size has changed.  We can't wait for the
  // user-requested window size as this may not be achievable on the
  // current system.
  const windowResizeChange = async () => {
    return new PollPromise((resolve, reject) => {
      let curRect = this.curBrowser.rect;
      if (curRect.width != origRect.width &&
          curRect.height != origRect.height) {
        resolve();
      } else {
        reject();
      }
    });
  };

  // Wait for the window position to change.
  async function windowPosition(x, y) {
    return new PollPromise((resolve, reject) => {
      if ((x == win.screenX && y == win.screenY) ||
          (win.screenX != origRect.x || win.screenY != origRect.y)) {
        resolve();
      } else {
        reject();
      }
    });
  }

  switch (WindowState.from(win.windowState)) {
    case WindowState.Fullscreen:
      await exitFullscreen(win);
      break;

    case WindowState.Minimized:
      await restoreWindow(win, this.curBrowser.eventObserver);
      break;
  }

  if (height != null && width != null) {
    assert.positiveInteger(height);
    assert.positiveInteger(width);

    if (win.outerWidth != width || win.outerHeight != height) {
      await resizeWindow(width, height);
      await windowResizeChange();
    }
  }

  if (x != null && y != null) {
    assert.integer(x);
    assert.integer(y);

    win.moveTo(x, y);
    await windowPosition(x, y);
  }

  return this.curBrowser.rect;
};

/**
 * Switch current top-level browsing context by name or server-assigned
 * ID.  Searches for windows by name, then ID.  Content windows take
 * precedence.
 *
 * @param {string} name
 *     Target name or ID of the window to switch to.
 * @param {boolean=} focus
 *      A boolean value which determines whether to focus
 *      the window. Defaults to true.
 */
GeckoDriver.prototype.switchToWindow = async function(cmd) {
  let focus = true;
  if (typeof cmd.parameters.focus != "undefined") {
    focus = cmd.parameters.focus;
  }

  // Window IDs are internally handled as numbers, but here it could
  // also be the name of the window.
  let switchTo = parseInt(cmd.parameters.name);
  if (isNaN(switchTo)) {
    switchTo = cmd.parameters.name;
  }

  let byNameOrId = function(win, windowId) {
    return switchTo === win.name || switchTo === windowId;
  };

  let found = this.findWindow(this.windows, byNameOrId);

  if (found) {
    await this.setWindowHandle(found, focus);
  } else {
    throw new NoSuchWindowError(`Unable to locate window: ${switchTo}`);
  }
};

/**
 * Find a specific window according to some filter function.
 *
 * @param {Iterable.<Window>} winIterable
 *     Iterable that emits Windows.
 * @param {function(Window, number): boolean} filter
 *     A callback function taking two arguments; the window and
 *     the outerId of the window, and returning a boolean indicating
 *     whether the window is the target.
 *
 * @return {Object}
 *     A window handle object containing the window and some
 *     associated metadata.
 */
GeckoDriver.prototype.findWindow = function(winIterable, filter) {
  for (let win of winIterable) {
    let outerId = getOuterWindowId(win);
    let tabBrowser = browser.getTabBrowser(win);

    // In case the wanted window is a chrome window, we are done.
    if (filter(win, outerId)) {
      return {win, outerId, hasTabBrowser: !!tabBrowser};

    // Otherwise check if the chrome window has a tab browser, and that it
    // contains a tab with the wanted window handle.
    } else if (tabBrowser && tabBrowser.tabs) {
      for (let i = 0; i < tabBrowser.tabs.length; ++i) {
        let contentBrowser = browser.getBrowserForTab(tabBrowser.tabs[i]);
        let contentWindowId = this.getIdForBrowser(contentBrowser);

        if (filter(win, contentWindowId)) {
          return {
            win,
            outerId,
            hasTabBrowser: true,
            tabIndex: i,
          };
        }
      }
    }
  }

  return null;
};

/**
 * Switch the marionette window to a given window. If the browser in
 * the window is unregistered, registers that browser and waits for
 * the registration is complete. If |focus| is true then set the focus
 * on the window.
 *
 * @param {Object} winProperties
 *     Object containing window properties such as returned from
 *     GeckoDriver#findWindow
 * @param {boolean=} focus
 *     A boolean value which determines whether to focus the window.
 *     Defaults to true.
 */
GeckoDriver.prototype.setWindowHandle = async function(
    winProperties, focus = true) {
  if (!(winProperties.outerId in this.browsers)) {
    // Initialise Marionette if the current chrome window has not been seen
    // before. Also register the initial tab, if one exists.
    let registerBrowsers, browserListening;

    if (winProperties.hasTabBrowser) {
      registerBrowsers = this.registerPromise();
      browserListening = this.listeningPromise();
    }

    this.startBrowser(winProperties.win, false /* isNewSession */);

    if (registerBrowsers && browserListening) {
      await registerBrowsers;
      await browserListening;
    }

  } else {
    // Otherwise switch to the known chrome window, and activate the tab
    // if it's a content browser.
    this.curBrowser = this.browsers[winProperties.outerId];

    if ("tabIndex" in winProperties) {
      this.curBrowser.switchToTab(
          winProperties.tabIndex, winProperties.win, focus);
    }
  }
};

GeckoDriver.prototype.getActiveFrame = function(cmd, resp) {
  assert.window(this.getCurrentWindow());

  switch (this.context) {
    case Context.Chrome:
      // no frame means top-level
      resp.body.value = null;
      if (this.curFrame) {
        resp.body.value = this.curBrowser.seenEls.add(
            this.curFrame.frameElement);
      }
      break;

    case Context.Content:
      resp.body.value = null;
      if (this.currentFrameElement !== null) {
        resp.body.value = this.currentFrameElement;
      }
      break;
  }
};

/**
 * Set the current browsing context for future commands to the parent
 * of the current browsing context.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.switchToParentFrame = async function() {
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  await this.listener.switchToParentFrame();
};

/**
 * Switch to a given frame within the current window.
 *
 * @param {Object} element
 *     A web element reference to the element to switch to.
 * @param {(string|number)} id
 *     If element is not defined, then this holds either the id, name,
 *     or index of the frame to switch to.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.switchToFrame = async function(cmd) {
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let {id, focus} = cmd.parameters;

  // TODO(ato): element can be either string (deprecated) or a web
  // element JSON Object.  Can be removed with Firefox 60.
  let byFrame;
  if (typeof cmd.parameters.element == "string") {
    byFrame = WebElement.fromUUID(cmd.parameters.element, Context.Chrome);
  } else if (cmd.parameters.element) {
    byFrame = WebElement.fromJSON(cmd.parameters.element);
  }

  const otherErrorsExpr = /about:.+(error)|(blocked)\?/;
  const checkTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

  let curWindow = this.getCurrentWindow();

  let checkLoad = function() {
    let win = this.getCurrentWindow();
    if (win.document.readyState == "complete") {
      return;
    } else if (win.document.readyState == "interactive") {
      let documentURI = win.document.documentURI;
      if (documentURI.startsWith("about:certerror")) {
        throw new InsecureCertificateError();
      } else if (otherErrorsExpr.exec(documentURI)) {
        throw new UnknownError("Reached error page: " + documentURI);
      }
    }

    checkTimer.initWithCallback(
        checkLoad.bind(this), 100, Ci.nsITimer.TYPE_ONE_SHOT);
  };

  if (this.context == Context.Chrome) {
    let foundFrame = null;

    // just focus
    if (typeof id == "undefined" && !byFrame) {
      this.curFrame = null;
      if (focus) {
        this.mainFrame.focus();
      }
      checkTimer.initWithCallback(
          checkLoad.bind(this), 100, Ci.nsITimer.TYPE_ONE_SHOT);
      return;
    }

    // by element (HTMLIFrameElement)
    if (byFrame) {
      let wantedFrame = this.curBrowser.seenEls.get(byFrame);

      // Deal with an embedded xul:browser case
      if (wantedFrame.tagName == "xul:browser" ||
          wantedFrame.tagName == "browser") {
        curWindow = wantedFrame.contentWindow;
        this.curFrame = curWindow;
        if (focus) {
          this.curFrame.focus();
        }
        checkTimer.initWithCallback(
            checkLoad.bind(this), 100, Ci.nsITimer.TYPE_ONE_SHOT);
        return;
      }

      // Check if the frame is XBL anonymous
      let parent = curWindow.document.getBindingParent(wantedFrame);
      // Shadow nodes also show up in getAnonymousNodes, we should
      // ignore them.
      if (parent &&
          !(parent.shadowRoot && parent.shadowRoot.contains(wantedFrame))) {
        const doc = curWindow.document;
        let anonNodes = [...doc.getAnonymousNodes(parent) || []];
        if (anonNodes.length > 0) {
          let el = wantedFrame;
          while (el) {
            if (anonNodes.indexOf(el) > -1) {
              curWindow = wantedFrame.contentWindow;
              this.curFrame = curWindow;
              if (focus) {
                this.curFrame.focus();
              }
              checkTimer.initWithCallback(
                  checkLoad.bind(this), 100, Ci.nsITimer.TYPE_ONE_SHOT);
              return;
            }
            el = el.parentNode;
          }
        }
      }

      // else, assume iframe
      let frames = curWindow.document.getElementsByTagName("iframe");
      let numFrames = frames.length;
      for (let i = 0; i < numFrames; i++) {
        let wrappedEl = new XPCNativeWrapper(frames[i]);
        let wrappedWanted = new XPCNativeWrapper(wantedFrame);
        if (wrappedEl == wrappedWanted) {
          curWindow = frames[i].contentWindow;
          this.curFrame = curWindow;
          if (focus) {
            this.curFrame.focus();
          }
          checkTimer.initWithCallback(
              checkLoad.bind(this), 100, Ci.nsITimer.TYPE_ONE_SHOT);
          return;
        }
      }
    }

    switch (typeof id) {
      case "string":
        let foundById = null;
        let frames = curWindow.document.getElementsByTagName("iframe");
        let numFrames = frames.length;
        for (let i = 0; i < numFrames; i++) {
          // give precedence to name
          let frame = frames[i];
          if (frame.getAttribute("name") == id) {
            foundFrame = i;
            curWindow = frame.contentWindow;
            break;
          } else if (foundById === null && frame.id == id) {
            foundById = i;
          }
        }
        if (foundFrame === null && foundById !== null) {
          foundFrame = foundById;
          curWindow = frames[foundById].contentWindow;
        }
        break;

      case "number":
        if (typeof curWindow.frames[id] != "undefined") {
          foundFrame = id;
          let frameEl = curWindow.frames[foundFrame].frameElement;
          curWindow = frameEl.contentWindow;
        }
        break;
    }

    if (foundFrame !== null) {
      this.curFrame = curWindow;
      if (focus) {
        this.curFrame.focus();
      }
      checkTimer.initWithCallback(
          checkLoad.bind(this), 100, Ci.nsITimer.TYPE_ONE_SHOT);
    } else {
      throw new NoSuchFrameError(`Unable to locate frame: ${id}`);
    }

  } else if (this.context == Context.Content) {
    cmd.commandID = cmd.id;
    await this.listener.switchToFrame(cmd.parameters);
  }
};

GeckoDriver.prototype.getTimeouts = function() {
  return this.timeouts;
};

/**
 * Set timeout for page loading, searching, and scripts.
 *
 * @param {Object.<string, number>}
 *     Dictionary of timeout types and their new value, where all timeout
 *     types are optional.
 *
 * @throws {InvalidArgumentError}
 *     If timeout type key is unknown, or the value provided with it is
 *     not an integer.
 */
GeckoDriver.prototype.setTimeouts = function(cmd) {
  // merge with existing timeouts
  let merged = Object.assign(this.timeouts.toJSON(), cmd.parameters);
  this.timeouts = session.Timeouts.fromJSON(merged);
};

/** Single tap. */
GeckoDriver.prototype.singleTap = async function(cmd) {
  assert.window(this.getCurrentWindow());

  let {id, x, y} = cmd.parameters;
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      throw new UnsupportedOperationError(
          "Command 'singleTap' is not yet available in chrome context");

    case Context.Content:
      await this.listener.singleTap(webEl, x, y);
      break;
  }
};

/**
 * Perform a series of grouped actions at the specified points in time.
 *
 * @param {Array.<?>} actions
 *     Array of objects that each represent an action sequence.
 *
 * @throws {UnsupportedOperationError}
 *     Not yet available in current context.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.performActions = async function(cmd) {
  assert.content(this.context,
      "Command 'performActions' is not yet available in chrome context");
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let actions = cmd.parameters.actions;
  await this.listener.performActions({"actions": actions});
};

/**
 * Release all the keys and pointer buttons that are currently depressed.
 *
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.releaseActions = async function() {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  await this.listener.releaseActions();
};

/**
 * An action chain.
 *
 * @param {Object} value
 *     A nested array where the inner array represents each event,
 *     and the outer array represents a collection of events.
 *
 * @return {number}
 *     Last touch ID.
 *
 * @throws {UnsupportedOperationError}
 *     Not applicable to application.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.actionChain = async function(cmd, resp) {
  const win = assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let {chain, nextId} = cmd.parameters;

  switch (this.context) {
    case Context.Chrome:
      // be conservative until this has a use case and is established
      // to work as expected in Fennec
      assert.firefox();

      resp.body.value = await this.legacyactions.dispatchActions(
          chain, nextId, {frame: win}, this.curBrowser.seenEls);
      break;

    case Context.Content:
      resp.body.value = await this.listener.actionChain(chain, nextId);
      break;
  }
};

/**
 * A multi-action chain.
 *
 * @param {Object} value
 *     A nested array where the inner array represents eache vent,
 *     the middle array represents a collection of events for each
 *     finger, and the outer array represents all fingers.
 *
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.multiAction = async function(cmd) {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let {value, max_length} = cmd.parameters; // eslint-disable-line camelcase
  await this.listener.multiAction(value, max_length);
};

/**
 * Find an element using the indicated search strategy.
 *
 * @param {string} using
 *     Indicates which search method to use.
 * @param {string} value
 *     Value the client is looking for.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.findElement = async function(cmd, resp) {
  const win = assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let {using, value} = cmd.parameters;
  let startNode;
  if (typeof cmd.parameters.element != "undefined") {
    startNode = WebElement.fromUUID(cmd.parameters.element, this.context);
  }

  let opts = {
    startNode,
    timeout: this.timeouts.implicit,
    all: false,
  };

  switch (this.context) {
    case Context.Chrome:
      if (!SUPPORTED_STRATEGIES.has(using)) {
        throw new InvalidSelectorError(`Strategy not supported: ${using}`);
      }

      let container = {frame: win};
      if (opts.startNode) {
        opts.startNode = this.curBrowser.seenEls.get(opts.startNode);
      }
      let el = await element.find(container, using, value, opts);
      let webEl = this.curBrowser.seenEls.add(el);
      resp.body.value = webEl;
      break;

    case Context.Content:
      resp.body.value = await this.listener.findElementContent(
          using, value, opts);
      break;
  }
};

/**
 * Find elements using the indicated search strategy.
 *
 * @param {string} using
 *     Indicates which search method to use.
 * @param {string} value
 *     Value the client is looking for.
 */
GeckoDriver.prototype.findElements = async function(cmd, resp) {
  const win = assert.window(this.getCurrentWindow());

  let {using, value} = cmd.parameters;
  let startNode;
  if (typeof cmd.parameters.element != "undefined") {
    startNode = WebElement.fromUUID(cmd.parameters.element, this.context);
  }

  let opts = {
    startNode,
    timeout: this.timeouts.implicit,
    all: true,
  };

  switch (this.context) {
    case Context.Chrome:
      if (!SUPPORTED_STRATEGIES.has(using)) {
        throw new InvalidSelectorError(`Strategy not supported: ${using}`);
      }

      let container = {frame: win};
      if (startNode) {
        opts.startNode = this.curBrowser.seenEls.get(opts.startNode);
      }
      let els = await element.find(container, using, value, opts);
      let webEls = this.curBrowser.seenEls.addAll(els);
      resp.body = webEls;
      break;

    case Context.Content:
      resp.body = await this.listener.findElementsContent(using, value, opts);
      break;
  }
};

/**
 * Return the active element in the document.
 *
 * @return {WebElement}
 *     Active element of the current browsing context's document
 *     element, if the document element is non-null.
 *
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {NoSuchElementError}
 *     If the document does not have an active element, i.e. if
 *     its document element has been deleted.
 */
GeckoDriver.prototype.getActiveElement = async function() {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  return this.listener.getActiveElement();
};

/**
 * Send click event to element.
 *
 * @param {string} id
 *     Reference ID to the element that will be clicked.
 *
 * @throws {InvalidArgumentError}
 *     If element <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.clickElement = async function(cmd) {
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      let el = this.curBrowser.seenEls.get(webEl);
      await interaction.clickElement(el, this.a11yChecks);
      break;

    case Context.Content:
      let click = this.listener.clickElement(
          {webElRef: webEl.toJSON(), pageTimeout: this.timeouts.pageLoad});

      // If a reload of the frame script interrupts our page load, this will
      // never return. We need to re-issue this request to correctly poll for
      // readyState and send errors.
      this.curBrowser.pendingCommands.push(() => {
        let parameters = {
          // TODO(ato): Bug 1242595
          commandID: this.listener.activeMessageId,
          pageTimeout: this.timeouts.pageLoad,
          startTime: new Date().getTime(),
        };
        this.curBrowser.messageManager.sendAsyncMessage(
            "Marionette:waitForPageLoaded", parameters);
      });

      await click;
      break;
  }
};

/**
 * Get a given attribute of an element.
 *
 * @param {string} id
 *     Web element reference ID to the element that will be inspected.
 * @param {string} name
 *     Name of the attribute which value to retrieve.
 *
 * @return {string}
 *     Value of the attribute.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> or <var>name</var> are not strings.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementAttribute = async function(cmd, resp) {
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let id = assert.string(cmd.parameters.id);
  let name = assert.string(cmd.parameters.name);
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      let el = this.curBrowser.seenEls.get(webEl);
      resp.body.value = el.getAttribute(name);
      break;

    case Context.Content:
      resp.body.value = await this.listener.getElementAttribute(webEl, name);
      break;
  }
};

/**
 * Returns the value of a property associated with given element.
 *
 * @param {string} id
 *     Web element reference ID to the element that will be inspected.
 * @param {string} name
 *     Name of the property which value to retrieve.
 *
 * @return {string}
 *     Value of the property.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> or <var>name</var> are not strings.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementProperty = async function(cmd, resp) {
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let id = assert.string(cmd.parameters.id);
  let name = assert.string(cmd.parameters.name);
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      let el = this.curBrowser.seenEls.get(webEl);
      resp.body.value = el[name];
      break;

    case Context.Content:
      resp.body.value = await this.listener.getElementProperty(webEl, name);
      break;
  }
};

/**
 * Get the text of an element, if any.  Includes the text of all child
 * elements.
 *
 * @param {string} id
 *     Reference ID to the element that will be inspected.
 *
 * @return {string}
 *     Element's text "as rendered".
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementText = async function(cmd, resp) {
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      // for chrome, we look at text nodes, and any node with a "label" field
      let el = this.curBrowser.seenEls.get(webEl);
      let lines = [];
      this.getVisibleText(el, lines);
      resp.body.value = lines.join("\n");
      break;

    case Context.Content:
      resp.body.value = await this.listener.getElementText(webEl);
      break;
  }
};

/**
 * Get the tag name of the element.
 *
 * @param {string} id
 *     Reference ID to the element that will be inspected.
 *
 * @return {string}
 *     Local tag name of element.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementTagName = async function(cmd, resp) {
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      let el = this.curBrowser.seenEls.get(webEl);
      resp.body.value = el.tagName.toLowerCase();
      break;

    case Context.Content:
      resp.body.value = await this.listener.getElementTagName(webEl);
      break;
  }
};

/**
 * Check if element is displayed.
 *
 * @param {string} id
 *     Reference ID to the element that will be inspected.
 *
 * @return {boolean}
 *     True if displayed, false otherwise.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.isElementDisplayed = async function(cmd, resp) {
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      let el = this.curBrowser.seenEls.get(webEl);
      resp.body.value = await interaction.isElementDisplayed(
          el, this.a11yChecks);
      break;

    case Context.Content:
      resp.body.value = await this.listener.isElementDisplayed(webEl);
      break;
  }
};

/**
 * Return the property of the computed style of an element.
 *
 * @param {string} id
 *     Reference ID to the element that will be checked.
 * @param {string} propertyName
 *     CSS rule that is being requested.
 *
 * @return {string}
 *     Value of |propertyName|.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> or <var>propertyName</var> are not strings.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementValueOfCssProperty = async function(
    cmd, resp) {
  const win = assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let id = assert.string(cmd.parameters.id);
  let prop = assert.string(cmd.parameters.propertyName);
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      let el = this.curBrowser.seenEls.get(webEl);
      let sty = win.document.defaultView.getComputedStyle(el);
      resp.body.value = sty.getPropertyValue(prop);
      break;

    case Context.Content:
      resp.body.value = await this.listener
          .getElementValueOfCssProperty(webEl, prop);
      break;
  }
};

/**
 * Check if element is enabled.
 *
 * @param {string} id
 *     Reference ID to the element that will be checked.
 *
 * @return {boolean}
 *     True if enabled, false if disabled.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.isElementEnabled = async function(cmd, resp) {
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      // Selenium atom doesn't quite work here
      let el = this.curBrowser.seenEls.get(webEl);
      resp.body.value = await interaction.isElementEnabled(
          el, this.a11yChecks);
      break;

    case Context.Content:
      resp.body.value = await this.listener.isElementEnabled(webEl);
      break;
  }
};

/**
 * Check if element is selected.
 *
 * @param {string} id
 *     Reference ID to the element that will be checked.
 *
 * @return {boolean}
 *     True if selected, false if unselected.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.isElementSelected = async function(cmd, resp) {
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      // Selenium atom doesn't quite work here
      let el = this.curBrowser.seenEls.get(webEl);
      resp.body.value = await interaction.isElementSelected(
          el, this.a11yChecks);
      break;

    case Context.Content:
      resp.body.value = await this.listener.isElementSelected(webEl);
      break;
  }
};

/**
 * @throws {InvalidArgumentError}
 *     If <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementRect = async function(cmd, resp) {
  const win = assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      let el = this.curBrowser.seenEls.get(webEl);
      let rect = el.getBoundingClientRect();
      resp.body = {
        x: rect.x + win.pageXOffset,
        y: rect.y + win.pageYOffset,
        width: rect.width,
        height: rect.height,
      };
      break;

    case Context.Content:
      resp.body = await this.listener.getElementRect(webEl);
      break;
  }
};

/**
 * Send key presses to element after focusing on it.
 *
 * @param {string} id
 *     Reference ID to the element that will be checked.
 * @param {string} text
 *     Value to send to the element.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> or <var>text</var> are not strings.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.sendKeysToElement = async function(cmd) {
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let id = assert.string(cmd.parameters.id);
  let text = assert.string(cmd.parameters.text);
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      let el = this.curBrowser.seenEls.get(webEl);
      await interaction.sendKeysToElement(el, text, this.a11yChecks);
      break;

    case Context.Content:
      await this.listener.sendKeysToElement(webEl, text);
      break;
  }
};

/**
 * Clear the text of an element.
 *
 * @param {string} id
 *     Reference ID to the element that will be cleared.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.clearElement = async function(cmd) {
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      // the selenium atom doesn't work here
      let el = this.curBrowser.seenEls.get(webEl);
      if (el.nodeName == "textbox") {
        el.value = "";
      } else if (el.nodeName == "checkbox") {
        el.checked = false;
      }
      break;

    case Context.Content:
      await this.listener.clearElement(webEl);
      break;
  }
};

/**
 * Switch to shadow root of the given host element.
 *
 * @param {string} id
 *     Reference ID to the element.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 */
GeckoDriver.prototype.switchToShadowRoot = async function(cmd) {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);
  await this.listener.switchToShadowRoot(webEl);
};

/**
 * Add a single cookie to the cookie store associated with the active
 * document's address.
 *
 * @param {Map.<string, (string|number|boolean)> cookie
 *     Cookie object.
 *
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {InvalidCookieDomainError}
 *     If <var>cookie</var> is for a different domain than the active
 *     document's host.
 */
GeckoDriver.prototype.addCookie = function(cmd) {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let {protocol, hostname} = this.currentURL;

  const networkSchemes = ["ftp:", "http:", "https:"];
  if (!networkSchemes.includes(protocol)) {
    throw new InvalidCookieDomainError("Document is cookie-averse");
  }

  let newCookie = cookie.fromJSON(cmd.parameters.cookie);

  cookie.add(newCookie, {restrictToHost: hostname});
};

/**
 * Get all the cookies for the current domain.
 *
 * This is the equivalent of calling <code>document.cookie</code> and
 * parsing the result.
 *
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getCookies = function(cmd, resp) {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let {hostname, pathname} = this.currentURL;
  resp.body = [...cookie.iter(hostname, pathname)];
};

/**
 * Delete all cookies that are visible to a document.
 *
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.deleteAllCookies = function() {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let {hostname, pathname} = this.currentURL;
  for (let toDelete of cookie.iter(hostname, pathname)) {
    cookie.remove(toDelete);
  }
};

/**
 * Delete a cookie by name.
 *
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.deleteCookie = function(cmd) {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  let {hostname, pathname} = this.currentURL;
  let name = assert.string(cmd.parameters.name);
  for (let c of cookie.iter(hostname, pathname)) {
    if (c.name === name) {
      cookie.remove(c);
    }
  }
};

/**
 * Close the currently selected tab/window.
 *
 * With multiple open tabs present the currently selected tab will
 * be closed.  Otherwise the window itself will be closed. If it is the
 * last window currently open, the window will not be closed to prevent
 * a shutdown of the application. Instead the returned list of window
 * handles is empty.
 *
 * @return {Array.<string>}
 *     Unique window handles of remaining windows.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.close = async function() {
  assert.window(this.getCurrentWindow(Context.Content));
  this._assertAndDismissModal();

  let nwins = 0;

  for (let win of this.windows) {
    // For browser windows count the tabs. Otherwise take the window itself.
    let tabbrowser = browser.getTabBrowser(win);
    if (tabbrowser && tabbrowser.tabs) {
      nwins += tabbrowser.tabs.length;
    } else {
      nwins += 1;
    }
  }

  // If there is only one window left, do not close it. Instead return
  // a faked empty array of window handles.  This will instruct geckodriver
  // to terminate the application.
  if (nwins === 1) {
    return [];
  }

  await this.curBrowser.closeTab();
  return this.windowHandles.map(String);
};

/**
 * Close the currently selected chrome window.
 *
 * If it is the last window currently open, the chrome window will not be
 * closed to prevent a shutdown of the application. Instead the returned
 * list of chrome window handles is empty.
 *
 * @return {Array.<string>}
 *     Unique chrome window handles of remaining chrome windows.
 */
GeckoDriver.prototype.closeChromeWindow = async function() {
  assert.firefox();
  assert.window(this.getCurrentWindow(Context.Chrome));

  let nwins = 0;

  // eslint-disable-next-line
  for (let _ of this.windows) {
    nwins++;
  }

  // If there is only one window left, do not close it.  Instead return
  // a faked empty array of window handles. This will instruct geckodriver
  // to terminate the application.
  if (nwins == 1) {
    return [];
  }

  // reset frame to the top-most frame
  this.curFrame = null;

  await this.curBrowser.closeWindow();
  return this.chromeWindowHandles.map(String);
};

/** Delete Marionette session. */
GeckoDriver.prototype.deleteSession = function() {
  if (this.curBrowser !== null) {
    // frame scripts can be safely reused
    Preferences.set(CONTENT_LISTENER_PREF, false);

    // delete session in each frame in each browser
    for (let win in this.browsers) {
      let browser = this.browsers[win];
      browser.knownFrames.forEach(() => {
        globalMessageManager.broadcastAsyncMessage("Marionette:deleteSession");
      });
    }

    for (let win of this.windows) {
      if (win.messageManager) {
        win.messageManager.removeDelayedFrameScript(FRAME_SCRIPT);
      } else {
        logger.error(
            `Could not remove listener from page ${win.location.href}`);
      }
    }
  }

  // reset frame to the top-most frame
  this.curFrame = null;
  if (this.mainFrame) {
    try {
      this.mainFrame.focus();
    } catch (e) {
      this.mainFrame = null;
    }
  }

  if (this.observing !== null) {
    for (let topic in this.observing) {
      Services.obs.removeObserver(this.observing[topic], topic);
    }
    this.observing = null;
  }

  modal.removeHandler(this.dialogHandler);

  this.sandboxes.clear();
  cert.uninstallOverride();

  this.sessionID = null;
  this.capabilities = new session.Capabilities();
};

/**
 * Takes a screenshot of a web element, current frame, or viewport.
 *
 * The screen capture is returned as a lossless PNG image encoded as
 * a base 64 string.
 *
 * If called in the content context, the |id| argument is not null and
 * refers to a present and visible web element's ID, the capture area will
 * be limited to the bounding box of that element.  Otherwise, the capture
 * area will be the bounding box of the current frame.
 *
 * If called in the chrome context, the screenshot will always represent
 * the entire viewport.
 *
 * @param {string=} id
 *     Optional web element reference to take a screenshot of.
 *     If undefined, a screenshot will be taken of the document element.
 * @param {Array.<string>=} highlights
 *     List of web elements to highlight.
 * @param {boolean} full
 *     True to take a screenshot of the entire document element. Is not
 *     considered if <var>id</var> is not defined. Defaults to true.
 * @param {boolean=} hash
 *     True if the user requests a hash of the image data.
 * @param {boolean=} scroll
 *     Scroll to element if |id| is provided.  If undefined, it will
 *     scroll to the element.
 *
 * @return {string}
 *     If <var>hash</var> is false, PNG image encoded as Base64 encoded
 *     string.  If <var>hash</var> is true, hex digest of the SHA-256
 *     hash of the Base64 encoded string.
 */
GeckoDriver.prototype.takeScreenshot = function(cmd) {
  let win = assert.window(this.getCurrentWindow());

  let {id, highlights, full, hash} = cmd.parameters;
  highlights = highlights || [];
  let format = hash ? capture.Format.Hash : capture.Format.Base64;

  switch (this.context) {
    case Context.Chrome:
      let highlightEls = highlights
          .map(ref => WebElement.fromUUID(ref, Context.Chrome))
          .map(webEl => this.curBrowser.seenEls.get(webEl));

      // viewport
      let canvas;
      if (!id && !full) {
        canvas = capture.viewport(win, highlightEls);

      // element or full document element
      } else {
        let node;
        if (id) {
          let webEl = WebElement.fromUUID(id, Context.Chrome);
          node = this.curBrowser.seenEls.get(webEl);
        } else {
          node = win.document.documentElement;
        }

        canvas = capture.element(node, highlightEls);
      }

      switch (format) {
        case capture.Format.Hash:
          return capture.toHash(canvas);

        case capture.Format.Base64:
          return capture.toBase64(canvas);
      }
      break;

    case Context.Content:
      return this.listener.takeScreenshot(format, cmd.parameters);
  }

  throw new TypeError(`Unknown context: ${this.context}`);
};

/**
 * Get the current browser orientation.
 *
 * Will return one of the valid primary orientation values
 * portrait-primary, landscape-primary, portrait-secondary, or
 * landscape-secondary.
 */
GeckoDriver.prototype.getScreenOrientation = function(cmd, resp) {
  assert.fennec();
  let win = assert.window(this.getCurrentWindow());

  resp.body.value = win.screen.mozOrientation;
};

/**
 * Set the current browser orientation.
 *
 * The supplied orientation should be given as one of the valid
 * orientation values.  If the orientation is unknown, an error will
 * be raised.
 *
 * Valid orientations are "portrait" and "landscape", which fall
 * back to "portrait-primary" and "landscape-primary" respectively,
 * and "portrait-secondary" as well as "landscape-secondary".
 */
GeckoDriver.prototype.setScreenOrientation = function(cmd) {
  assert.fennec();
  let win = assert.window(this.getCurrentWindow());

  const ors = [
    "portrait", "landscape",
    "portrait-primary", "landscape-primary",
    "portrait-secondary", "landscape-secondary",
  ];

  let or = String(cmd.parameters.orientation);
  assert.string(or);
  let mozOr = or.toLowerCase();
  if (!ors.includes(mozOr)) {
    throw new InvalidArgumentError(`Unknown screen orientation: ${or}`);
  }

  if (!win.screen.mozLockOrientation(mozOr)) {
    throw new WebDriverError(`Unable to set screen orientation: ${or}`);
  }
};

/**
 * Synchronously minimizes the user agent window as if the user pressed
 * the minimize button.
 *
 * No action is taken if the window is already minimized.
 *
 * Not supported on Fennec.
 *
 * @return {Object.<string, number>}
 *     Window rect and window state.
 *
 * @throws {UnsupportedOperationError}
 *     Not available for current application.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.minimizeWindow = async function() {
  assert.firefox();
  const win = assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  if (WindowState.from(win.windowState) == WindowState.Fullscreen) {
    await exitFullscreen(win);
  }

  if (WindowState.from(win.windowState) != WindowState.Minimized) {
    await new Promise(resolve => {
      this.curBrowser.eventObserver.addEventListener("visibilitychange", resolve, {once: true});
      win.minimize();
    });
  }

  return this.curBrowser.rect;
};

/**
 * Synchronously maximizes the user agent window as if the user pressed
 * the maximize button.
 *
 * No action is taken if the window is already maximized.
 *
 * Not supported on Fennec.
 *
 * @return {Object.<string, number>}
 *     Window rect.
 *
 * @throws {UnsupportedOperationError}
 *     Not available for current application.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.maximizeWindow = async function() {
  assert.firefox();
  const win = assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  switch (WindowState.from(win.windowState)) {
    case WindowState.Fullscreen:
      await exitFullscreen(win);
      break;

    case WindowState.Minimized:
      await restoreWindow(win, this.curBrowser.eventObserver);
      break;
  }

  const origSize = {
    outerWidth: win.outerWidth,
    outerHeight: win.outerHeight,
  };

  // Wait for the window size to change.
  async function windowSizeChange() {
    return new PollPromise((resolve, reject) => {
      let curSize = {
        outerWidth: win.outerWidth,
        outerHeight: win.outerHeight,
      };
      if (curSize.outerWidth != origSize.outerWidth ||
          curSize.outerHeight != origSize.outerHeight) {
        resolve();
      } else {
        reject();
      }
    });
  }

  if (WindowState.from(win.windowState) != win.Maximized) {
    await new TimedPromise(resolve => {
      win.addEventListener("sizemodechange", resolve, {once: true});
      win.maximize();
    }, {throws: null});

    // Transitioning into a window state is asynchronous on Linux,
    // and we cannot rely on sizemodechange to accurately tell us when
    // the transition has completed.
    //
    // To counter for this we wait for the window size to change, which
    // it usually will.  On platforms where the transition is synchronous,
    // the wait will have the cost of one iteration because the size
    // will have changed as part of the transition.  Where the platform is
    // asynchronous, the cost may be greater as we have to poll
    // continuously until we see a change, but it ensures conformity in
    // behaviour.
    //
    // Certain window managers, however, do not have a concept of
    // maximised windows and here sizemodechange may never fire.  Indeed,
    // if the window covers the maximum available screen real estate,
    // the window size may also not change.  In this circumstance,
    // which admittedly is a somewhat bizarre edge case, we assume that
    // the timeout of waiting for sizemodechange to fire is sufficient
    // to give the window enough time to transition itself into whatever
    // form or shape the WM is programmed to give it.
    await windowSizeChange();
  }

  return this.curBrowser.rect;
};

/**
 * Synchronously sets the user agent window to full screen as if the user
 * had done "View > Enter Full Screen".
 *
 * No action is taken if the window is already in full screen mode.
 *
 * Not supported on Fennec.
 *
 * @return {Map.<string, number>}
 *     Window rect.
 *
 * @throws {UnsupportedOperationError}
 *     Not available for current application.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.fullscreenWindow = async function() {
  assert.firefox();
  const win = assert.window(this.getCurrentWindow());
  this._assertAndDismissModal();

  if (WindowState.from(win.windowState) == WindowState.Minimized) {
    await restoreWindow(win, this.curBrowser.eventObserver);
  }

  if (WindowState.from(win.windowState) != WindowState.Fullscreen) {
    await new Promise(resolve => {
      win.addEventListener("sizemodechange", resolve, {once: true});
      win.fullScreen = true;
    });
  }

  return this.curBrowser.rect;
};

/**
 * Dismisses a currently displayed tab modal, or returns no such alert if
 * no modal is displayed.
 */
GeckoDriver.prototype.dismissDialog = function() {
  assert.window(this.getCurrentWindow());
  this._checkIfAlertIsPresent();

  let {button0, button1} = this.dialog.ui;
  (button1 ? button1 : button0).click();
  this.dialog = null;
};

/**
 * Accepts a currently displayed tab modal, or returns no such alert if
 * no modal is displayed.
 */
GeckoDriver.prototype.acceptDialog = function() {
  assert.window(this.getCurrentWindow());
  this._checkIfAlertIsPresent();

  let {button0} = this.dialog.ui;
  button0.click();
  this.dialog = null;
};

/**
 * Returns the message shown in a currently displayed modal, or returns
 * a no such alert error if no modal is currently displayed.
 */
GeckoDriver.prototype.getTextFromDialog = function(cmd, resp) {
  assert.window(this.getCurrentWindow());
  this._checkIfAlertIsPresent();

  let {infoBody} = this.dialog.ui;
  resp.body.value = infoBody.textContent;
};

/**
 * Set the user prompt's value field.
 *
 * Sends keys to the input field of a currently displayed modal, or
 * returns a no such alert error if no modal is currently displayed. If
 * a tab modal is currently displayed but has no means for text input,
 * an element not visible error is returned.
 *
 * @param {string} text
 *     Input to the user prompt's value field.
 *
 * @throws {ElementNotInteractableError}
 *     If the current user prompt is an alert or confirm.
 * @throws {NoSuchAlertError}
 *     If there is no current user prompt.
 * @throws {UnsupportedOperationError}
 *     If the current user prompt is something other than an alert,
 *     confirm, or a prompt.
 */
GeckoDriver.prototype.sendKeysToDialog = async function(cmd) {
  assert.window(this.getCurrentWindow());
  this._checkIfAlertIsPresent();

  // see toolkit/components/prompts/content/commonDialog.js
  let {loginTextbox} = this.dialog.ui;
  await interaction.sendKeysToElement(
      loginTextbox, cmd.parameters.text, this.a11yChecks);
};

GeckoDriver.prototype._checkIfAlertIsPresent = function() {
  if (!this.dialog || !this.dialog.ui) {
    throw new NoAlertOpenError("No modal dialog is currently open");
  }
};

GeckoDriver.prototype._assertAndDismissModal = function() {
  try {
    assert.noUserPrompt(this.dialog);
  } catch (e) {
    this.dismissDialog();
    throw e;
  }
};

/**
 * Enables or disables accepting new socket connections.
 *
 * By calling this method with `false` the server will not accept any
 * further connections, but existing connections will not be forcible
 * closed. Use `true` to re-enable accepting connections.
 *
 * Please note that when closing the connection via the client you can
 * end-up in a non-recoverable state if it hasn't been enabled before.
 *
 * This method is used for custom in application shutdowns via
 * marionette.quit() or marionette.restart(), like File -> Quit.
 *
 * @param {boolean} state
 *     True if the server should accept new socket connections.
 */
GeckoDriver.prototype.acceptConnections = function(cmd) {
  assert.boolean(cmd.parameters.value);
  this._server.acceptConnections = cmd.parameters.value;
};

/**
 * Quits the application with the provided flags.
 *
 * Marionette will stop accepting new connections before ending the
 * current session, and finally attempting to quit the application.
 *
 * Optional {@link nsIAppStartup} flags may be provided as
 * an array of masks, and these will be combined by ORing
 * them with a bitmask.  The available masks are defined in
 * https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XPCOM/Reference/Interface/nsIAppStartup.
 *
 * Crucially, only one of the *Quit flags can be specified. The |eRestart|
 * flag may be bit-wise combined with one of the *Quit flags to cause
 * the application to restart after it quits.
 *
 * @param {Array.<string>=} flags
 *     Constant name of masks to pass to |Services.startup.quit|.
 *     If empty or undefined, |nsIAppStartup.eAttemptQuit| is used.
 *
 * @return {string}
 *     Explaining the reason why the application quit.  This can be
 *     in response to a normal shutdown or restart, yielding "shutdown"
 *     or "restart", respectively.
 *
 * @throws {InvalidArgumentError}
 *     If <var>flags</var> contains unknown or incompatible flags,
 *     for example multiple Quit flags.
 */
GeckoDriver.prototype.quit = async function(cmd, resp) {
  const quits = ["eConsiderQuit", "eAttemptQuit", "eForceQuit"];

  let flags = [];
  if (typeof cmd.parameters.flags != "undefined") {
    flags = assert.array(cmd.parameters.flags);
  }

  // bug 1298921
  assert.firefox();

  let quitSeen;
  let mode = 0;
  if (flags.length > 0) {
    for (let k of flags) {
      assert.in(k, Ci.nsIAppStartup);

      if (quits.includes(k)) {
        if (quitSeen) {
          throw new InvalidArgumentError(
              `${k} cannot be combined with ${quitSeen}`);
        }
        quitSeen = k;
      }

      mode |= Ci.nsIAppStartup[k];
    }
  } else {
    mode = Ci.nsIAppStartup.eAttemptQuit;
  }

  this._server.acceptConnections = false;
  this.deleteSession();

  // delay response until the application is about to quit
  let quitApplication = new Promise(resolve => {
    Services.obs.addObserver(
        (subject, topic, data) => resolve(data),
        "quit-application");
  });

  Services.startup.quit(mode);

  resp.body.cause = await quitApplication;
  resp.send();
};

GeckoDriver.prototype.installAddon = function(cmd) {
  assert.firefox();

  let path = cmd.parameters.path;
  let temp = cmd.parameters.temporary || false;
  if (typeof path == "undefined" || typeof path != "string" ||
      typeof temp != "boolean") {
    throw new InvalidArgumentError();
  }

  return addon.install(path, temp);
};

GeckoDriver.prototype.uninstallAddon = function(cmd) {
  assert.firefox();

  let id = cmd.parameters.id;
  if (typeof id == "undefined" || typeof id != "string") {
    throw new InvalidArgumentError();
  }

  return addon.uninstall(id);
};

/** Receives all messages from content messageManager. */
/* eslint-disable consistent-return */
GeckoDriver.prototype.receiveMessage = function(message) {
  switch (message.name) {
    case "Marionette:switchedToFrame":
      if (message.json.restorePrevious) {
        this.currentFrameElement = this.previousFrameElement;
      } else {
        // we don't arbitrarily save previousFrameElement, since
        // we allow frame switching after modals appear, which would
        // override this value and we'd lose our reference
        if (message.json.storePrevious) {
          this.previousFrameElement =
              new ChromeWebElement(this.currentFrameElement);
        }
        if (message.json.frameValue) {
          this.currentFrameElement =
              new ChromeWebElement(message.json.frameValue);
        } else {
          this.currentFrameElement = null;
        }
      }
      break;

    case "Marionette:Register":
      let wid = message.json.value;
      let be = message.target;
      let outerWindowID = this.registerBrowser(wid, be);
      return {outerWindowID};

    case "Marionette:ListenersAttached":
      if (message.json.outerWindowID === this.curBrowser.curFrameId) {
        // If the frame script gets reloaded we need to call newSession.
        // In the case of desktop this just sets up a small amount of state
        // that doesn't change over the course of a session.
        this.sendAsync("newSession");
        this.curBrowser.flushPendingCommands();
      }
      break;

    case "Marionette:WebDriver:GetCapabilities":
      return this.capabilities.toJSON();

    case "Marionette:GetLogLevel":
      return logger.level;
  }
};
/* eslint-enable consistent-return */

GeckoDriver.prototype.responseCompleted = function() {
  if (this.curBrowser !== null) {
    this.curBrowser.pendingCommands = [];
  }
};

/**
 * Retrieve the localized string for the specified entity id.
 *
 * Example:
 *     localizeEntity(["chrome://branding/locale/brand.dtd"], "brandShortName")
 *
 * @param {Array.<string>} urls
 *     Array of .dtd URLs.
 * @param {string} id
 *     The ID of the entity to retrieve the localized string for.
 *
 * @return {string}
 *     The localized string for the requested entity.
 */
GeckoDriver.prototype.localizeEntity = function(cmd, resp) {
  let {urls, id} = cmd.parameters;

  if (!Array.isArray(urls)) {
    throw new InvalidArgumentError("Value of `urls` should be of type 'Array'");
  }
  if (typeof id != "string") {
    throw new InvalidArgumentError("Value of `id` should be of type 'string'");
  }

  resp.body.value = l10n.localizeEntity(urls, id);
};

/**
 * Retrieve the localized string for the specified property id.
 *
 * Example:
 *
 *     localizeProperty(
 *         ["chrome://global/locale/findbar.properties"], "FastFind");
 *
 * @param {Array.<string>} urls
 *     Array of .properties URLs.
 * @param {string} id
 *     The ID of the property to retrieve the localized string for.
 *
 * @return {string}
 *     The localized string for the requested property.
 */
GeckoDriver.prototype.localizeProperty = function(cmd, resp) {
  let {urls, id} = cmd.parameters;

  if (!Array.isArray(urls)) {
    throw new InvalidArgumentError("Value of `urls` should be of type 'Array'");
  }
  if (typeof id != "string") {
    throw new InvalidArgumentError("Value of `id` should be of type 'string'");
  }

  resp.body.value = l10n.localizeProperty(urls, id);
};

/**
 * Initialize the reftest mode
 */
GeckoDriver.prototype.setupReftest = async function(cmd) {
  if (this._reftest) {
    throw new UnsupportedOperationError(
        "Called reftest:setup with a reftest session already active");
  }

  if (this.context !== Context.Chrome) {
    throw new UnsupportedOperationError(
        "Must set chrome context before running reftests");
  }

  let {urlCount = {}, screenshot = "unexpected"} = cmd.parameters;
  if (!["always", "fail", "unexpected"].includes(screenshot)) {
    throw new InvalidArgumentError(
        "Value of `screenshot` should be 'always', 'fail' or 'unexpected'");
  }

  this._reftest = new reftest.Runner(this);
  await this._reftest.setup(urlCount, screenshot);
};


/** Run a reftest. */
GeckoDriver.prototype.runReftest = async function(cmd, resp) {
  let {test, references, expected, timeout} = cmd.parameters;

  if (!this._reftest) {
    throw new UnsupportedOperationError(
        "Called reftest:run before reftest:start");
  }

  assert.string(test);
  assert.string(expected);
  assert.array(references);

  resp.body.value = await this._reftest.run(
      test, references, expected, timeout);
};

/**
 * End a reftest run.
 *
 * Closes the reftest window (without changing the current window handle),
 * and removes cached canvases.
 */
GeckoDriver.prototype.teardownReftest = function() {
  if (!this._reftest) {
    throw new UnsupportedOperationError(
        "Called reftest:teardown before reftest:start");
  }

  this._reftest.abort();
  this._reftest = null;
};

GeckoDriver.prototype.commands = {
  // Marionette service
  "Marionette:SetContext": GeckoDriver.prototype.setContext,
  "setContext": GeckoDriver.prototype.setContext,  // deprecated, remove in Firefox 60
  "Marionette:GetContext": GeckoDriver.prototype.getContext,
  "getContext": GeckoDriver.prototype.getContext,
  "Marionette:AcceptConnections": GeckoDriver.prototype.acceptConnections,
  "acceptConnections": GeckoDriver.prototype.acceptConnections,  // deprecated, remove in Firefox 60
  "Marionette:Quit": GeckoDriver.prototype.quit,
  "quit": GeckoDriver.prototype.quit,  // deprecated, remove in Firefox 60
  "quitApplication": GeckoDriver.prototype.quit,  // deprecated, remove in Firefox 60

  // Addon service
  "Addon:Install": GeckoDriver.prototype.installAddon,
  "addon:install": GeckoDriver.prototype.installAddon,  // deprecated, remove in Firefox 60
  "Addon:Uninstall": GeckoDriver.prototype.uninstallAddon,
  "addon:uninstall": GeckoDriver.prototype.uninstallAddon,  // deprecated, remove in Firefox 60

  // L10n service
  "L10n:LocalizeEntity": GeckoDriver.prototype.localizeEntity,
  "localization:l10n:localizeEntity": GeckoDriver.prototype.localizeEntity,  // deprecated, remove in Firefox 60
  "L10n:LocalizeProperty": GeckoDriver.prototype.localizeProperty,
  "localization:l10n:localizeProperty": GeckoDriver.prototype.localizeProperty,  // deprecated, remove in Firefox 60

  // Reftest service
  "reftest:setup": GeckoDriver.prototype.setupReftest,
  "reftest:run": GeckoDriver.prototype.runReftest,
  "reftest:teardown": GeckoDriver.prototype.teardownReftest,

  // WebDriver service
  "WebDriver:AcceptDialog": GeckoDriver.prototype.acceptDialog,
  "WebDriver:AddCookie": GeckoDriver.prototype.addCookie,
  "WebDriver:Back": GeckoDriver.prototype.goBack,
  "WebDriver:CloseChromeWindow": GeckoDriver.prototype.closeChromeWindow,
  "WebDriver:CloseWindow": GeckoDriver.prototype.close,
  "WebDriver:DeleteAllCookies": GeckoDriver.prototype.deleteAllCookies,
  "WebDriver:DeleteCookie": GeckoDriver.prototype.deleteCookie,
  "WebDriver:DeleteSession": GeckoDriver.prototype.deleteSession,
  "WebDriver:DismissAlert": GeckoDriver.prototype.dismissDialog,
  "WebDriver:ElementClear": GeckoDriver.prototype.clearElement,
  "WebDriver:ElementClick": GeckoDriver.prototype.clickElement,
  "WebDriver:ElementSendKeys": GeckoDriver.prototype.sendKeysToElement,
  "WebDriver:ExecuteAsyncScript": GeckoDriver.prototype.executeAsyncScript,
  "WebDriver:ExecuteScript": GeckoDriver.prototype.executeScript,
  "WebDriver:FindElement": GeckoDriver.prototype.findElement,
  "WebDriver:FindElements": GeckoDriver.prototype.findElements,
  "WebDriver:Forward": GeckoDriver.prototype.goForward,
  "WebDriver:FullscreenWindow": GeckoDriver.prototype.fullscreenWindow,
  "WebDriver:GetActiveElement": GeckoDriver.prototype.getActiveElement,
  "WebDriver:GetActiveFrame": GeckoDriver.prototype.getActiveFrame,
  "WebDriver:GetAlertText": GeckoDriver.prototype.getTextFromDialog,
  "WebDriver:GetCapabilities": GeckoDriver.prototype.getSessionCapabilities,
  "WebDriver:GetChromeWindowHandle": GeckoDriver.prototype.getChromeWindowHandle,
  "WebDriver:GetChromeWindowHandles": GeckoDriver.prototype.getChromeWindowHandles,
  "WebDriver:GetCookies": GeckoDriver.prototype.getCookies,
  "WebDriver:GetCurrentChromeWindowHandle": GeckoDriver.prototype.getChromeWindowHandle,
  "WebDriver:GetCurrentURL": GeckoDriver.prototype.getCurrentUrl,
  "WebDriver:GetElementAttribute": GeckoDriver.prototype.getElementAttribute,
  "WebDriver:GetElementCSSValue": GeckoDriver.prototype.getElementValueOfCssProperty,
  "WebDriver:GetElementProperty": GeckoDriver.prototype.getElementProperty,
  "WebDriver:GetElementRect": GeckoDriver.prototype.getElementRect,
  "WebDriver:GetElementTagName": GeckoDriver.prototype.getElementTagName,
  "WebDriver:GetElementText": GeckoDriver.prototype.getElementText,
  "WebDriver:GetPageSource": GeckoDriver.prototype.getPageSource,
  "WebDriver:GetScreenOrientation": GeckoDriver.prototype.getScreenOrientation,
  "WebDriver:GetTimeouts": GeckoDriver.prototype.getTimeouts,
  "WebDriver:GetTitle": GeckoDriver.prototype.getTitle,
  "WebDriver:GetWindowHandle": GeckoDriver.prototype.getWindowHandle,
  "WebDriver:GetWindowHandles": GeckoDriver.prototype.getWindowHandles,
  "WebDriver:GetWindowRect": GeckoDriver.prototype.getWindowRect,
  "WebDriver:GetWindowType": GeckoDriver.prototype.getWindowType,
  "WebDriver:IsElementDisplayed": GeckoDriver.prototype.isElementDisplayed,
  "WebDriver:IsElementEnabled": GeckoDriver.prototype.isElementEnabled,
  "WebDriver:IsElementSelected": GeckoDriver.prototype.isElementSelected,
  "WebDriver:MinimizeWindow": GeckoDriver.prototype.minimizeWindow,
  "WebDriver:MaximizeWindow": GeckoDriver.prototype.maximizeWindow,
  "WebDriver:Navigate": GeckoDriver.prototype.get,
  "WebDriver:NewSession": GeckoDriver.prototype.newSession,
  "WebDriver:PerformActions": GeckoDriver.prototype.performActions,
  "WebDriver:Refresh":  GeckoDriver.prototype.refresh,
  "WebDriver:ReleaseActions": GeckoDriver.prototype.releaseActions,
  "WebDriver:SendAlertText": GeckoDriver.prototype.sendKeysToDialog,
  "WebDriver:SetScreenOrientation": GeckoDriver.prototype.setScreenOrientation,
  "WebDriver:SetTimeouts": GeckoDriver.prototype.setTimeouts,
  "WebDriver:SetWindowRect": GeckoDriver.prototype.setWindowRect,
  "WebDriver:SwitchToFrame": GeckoDriver.prototype.switchToFrame,
  "WebDriver:SwitchToParentFrame": GeckoDriver.prototype.switchToParentFrame,
  "WebDriver:SwitchToShadowRoot": GeckoDriver.prototype.switchToShadowRoot,
  "WebDriver:SwitchToWindow": GeckoDriver.prototype.switchToWindow,
  "WebDriver:TakeScreenshot": GeckoDriver.prototype.takeScreenshot,

  // deprecated WebDriver commands, remove in Firefox 60
  "acceptDialog": GeckoDriver.prototype.acceptDialog,
  "actionChain": GeckoDriver.prototype.actionChain,
  "addCookie": GeckoDriver.prototype.addCookie,
  "clearElement": GeckoDriver.prototype.clearElement,
  "clickElement": GeckoDriver.prototype.clickElement,
  "closeChromeWindow": GeckoDriver.prototype.closeChromeWindow,
  "close": GeckoDriver.prototype.close,
  "deleteAllCookies": GeckoDriver.prototype.deleteAllCookies,
  "deleteCookie": GeckoDriver.prototype.deleteCookie,
  "deleteSession": GeckoDriver.prototype.deleteSession,
  "dismissDialog": GeckoDriver.prototype.dismissDialog,
  "executeAsyncScript": GeckoDriver.prototype.executeAsyncScript,
  "executeScript": GeckoDriver.prototype.executeScript,
  "findElement": GeckoDriver.prototype.findElement,
  "findElements": GeckoDriver.prototype.findElements,
  "fullscreen": GeckoDriver.prototype.fullscreenWindow,
  "getActiveElement": GeckoDriver.prototype.getActiveElement,
  "getActiveFrame": GeckoDriver.prototype.getActiveFrame,
  "getChromeWindowHandle": GeckoDriver.prototype.getChromeWindowHandle,
  "getChromeWindowHandles": GeckoDriver.prototype.getChromeWindowHandles,
  "getCookies": GeckoDriver.prototype.getCookies,
  "getCurrentChromeWindowHandle": GeckoDriver.prototype.getChromeWindowHandle,
  "getCurrentUrl": GeckoDriver.prototype.getCurrentUrl,
  "getElementAttribute": GeckoDriver.prototype.getElementAttribute,
  "getElementProperty": GeckoDriver.prototype.getElementProperty,
  "getElementRect": GeckoDriver.prototype.getElementRect,
  "getElementTagName": GeckoDriver.prototype.getElementTagName,
  "getElementText": GeckoDriver.prototype.getElementText,
  "getElementValueOfCssProperty": GeckoDriver.prototype.getElementValueOfCssProperty,
  "get": GeckoDriver.prototype.get,
  "getPageSource": GeckoDriver.prototype.getPageSource,
  "getScreenOrientation": GeckoDriver.prototype.getScreenOrientation,
  "getSessionCapabilities": GeckoDriver.prototype.getSessionCapabilities,
  "getTextFromDialog": GeckoDriver.prototype.getTextFromDialog,
  "getTimeouts": GeckoDriver.prototype.getTimeouts,
  "getTitle": GeckoDriver.prototype.getTitle,
  "getWindowHandle": GeckoDriver.prototype.getWindowHandle,
  "getWindowHandles": GeckoDriver.prototype.getWindowHandles,
  "getWindowPosition": GeckoDriver.prototype.getWindowRect, // redirect for compatibility
  "getWindowRect": GeckoDriver.prototype.getWindowRect,
  "getWindowSize": GeckoDriver.prototype.getWindowRect, // redirect for compatibility
  "getWindowType": GeckoDriver.prototype.getWindowType,
  "goBack": GeckoDriver.prototype.goBack,
  "goForward": GeckoDriver.prototype.goForward,
  "isElementDisplayed": GeckoDriver.prototype.isElementDisplayed,
  "isElementEnabled": GeckoDriver.prototype.isElementEnabled,
  "isElementSelected": GeckoDriver.prototype.isElementSelected,
  "maximizeWindow": GeckoDriver.prototype.maximizeWindow,
  "multiAction": GeckoDriver.prototype.multiAction,
  "newSession": GeckoDriver.prototype.newSession,
  "performActions": GeckoDriver.prototype.performActions,
  "refresh":  GeckoDriver.prototype.refresh,
  "releaseActions": GeckoDriver.prototype.releaseActions,
  "sendKeysToDialog": GeckoDriver.prototype.sendKeysToDialog,
  "sendKeysToElement": GeckoDriver.prototype.sendKeysToElement,
  "setScreenOrientation": GeckoDriver.prototype.setScreenOrientation,
  "setTimeouts": GeckoDriver.prototype.setTimeouts,
  "setWindowPosition": GeckoDriver.prototype.setWindowRect, // redirect for compatibility
  "setWindowRect": GeckoDriver.prototype.setWindowRect,
  "setWindowSize": GeckoDriver.prototype.setWindowRect, // redirect for compatibility
  "singleTap": GeckoDriver.prototype.singleTap,
  "switchToFrame": GeckoDriver.prototype.switchToFrame,
  "switchToParentFrame": GeckoDriver.prototype.switchToParentFrame,
  "switchToShadowRoot": GeckoDriver.prototype.switchToShadowRoot,
  "switchToWindow": GeckoDriver.prototype.switchToWindow,
  "takeScreenshot": GeckoDriver.prototype.takeScreenshot,
};

function getOuterWindowId(win) {
  return win.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils)
      .outerWindowID;
}

/**
 * Exit fullscreen and wait for <var>window</var> to resize.
 *
 * @param {ChromeWindow} window
 *     Window to exit fullscreen.
 */
async function exitFullscreen(window) {
  return new Promise(resolve => {
    window.addEventListener("sizemodechange", whenIdle(window, resolve), {once: true});
    window.fullScreen = false;
  });
}

/**
 * Restore window and wait for the window state to change.
 *
 * @param {ChromeWindow} chromWindow
 *     Window to restore.
 * @param {WebElementEventTarget} contentWindow
 *     Content window to listen for events in.
 */
async function restoreWindow(chromeWindow, contentWindow) {
  return new Promise(resolve => {
    contentWindow.addEventListener("visibilitychange", resolve, {once: true});
    chromeWindow.restore();
  });
}

/**
 * Throttle <var>callback</var> until the main thread is idle and
 * <var>window</var> has performed an animation frame.
 *
 * @param {ChromeWindow} window
 *     Window to request the animation frame from.
 * @param {function()} callback
 *     Called when done.
 *
 * @return {function()}
 *     Anonymous function that when invoked will wait for the main
 *     thread to clear up and request an animation frame before calling
 *     <var>callback</var>.
 */
function whenIdle(window, callback) {
  return () => Services.tm.idleDispatchToMainThread(() => {
    window.requestAnimationFrame(callback);
  });
}
