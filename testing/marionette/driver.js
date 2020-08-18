/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/* global XPCNativeWrapper */

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { accessibility } = ChromeUtils.import(
  "chrome://marionette/content/accessibility.js"
);
const { Addon } = ChromeUtils.import("chrome://marionette/content/addon.js");
const { assert } = ChromeUtils.import("chrome://marionette/content/assert.js");
const { atom } = ChromeUtils.import("chrome://marionette/content/atom.js");
const { browser, Context, WindowState } = ChromeUtils.import(
  "chrome://marionette/content/browser.js"
);
const { Capabilities, Timeouts, UnhandledPromptBehavior } = ChromeUtils.import(
  "chrome://marionette/content/capabilities.js"
);
const { capture } = ChromeUtils.import(
  "chrome://marionette/content/capture.js"
);
const { allowAllCerts } = ChromeUtils.import(
  "chrome://marionette/content/cert.js"
);
const { cookie } = ChromeUtils.import("chrome://marionette/content/cookie.js");
const { WebElementEventTarget } = ChromeUtils.import(
  "chrome://marionette/content/dom.js"
);
const { ChromeWebElement, element, WebElement } = ChromeUtils.import(
  "chrome://marionette/content/element.js"
);
const {
  ElementNotInteractableError,
  InsecureCertificateError,
  InvalidArgumentError,
  InvalidCookieDomainError,
  InvalidSelectorError,
  NoSuchAlertError,
  NoSuchFrameError,
  NoSuchWindowError,
  SessionNotCreatedError,
  UnexpectedAlertOpenError,
  UnknownError,
  UnsupportedOperationError,
  WebDriverError,
} = ChromeUtils.import("chrome://marionette/content/error.js");
const { Sandboxes, evaluate } = ChromeUtils.import(
  "chrome://marionette/content/evaluate.js"
);
const { pprint } = ChromeUtils.import("chrome://marionette/content/format.js");
const { interaction } = ChromeUtils.import(
  "chrome://marionette/content/interaction.js"
);
const { l10n } = ChromeUtils.import("chrome://marionette/content/l10n.js");
const { legacyaction } = ChromeUtils.import(
  "chrome://marionette/content/legacyaction.js"
);
const { Log } = ChromeUtils.import("chrome://marionette/content/log.js");
const { modal } = ChromeUtils.import("chrome://marionette/content/modal.js");
const { navigate } = ChromeUtils.import(
  "chrome://marionette/content/navigate.js"
);
const { MarionettePrefs } = ChromeUtils.import(
  "chrome://marionette/content/prefs.js",
  null
);
const { print } = ChromeUtils.import("chrome://marionette/content/print.js");
const { proxy } = ChromeUtils.import("chrome://marionette/content/proxy.js");
const { reftest } = ChromeUtils.import(
  "chrome://marionette/content/reftest.js"
);
const {
  DebounceCallback,
  IdlePromise,
  PollPromise,
  TimedPromise,
  waitForEvent,
  waitForObserverTopic,
} = ChromeUtils.import("chrome://marionette/content/sync.js");

XPCOMUtils.defineLazyGetter(this, "logger", Log.get);
XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

this.EXPORTED_SYMBOLS = ["GeckoDriver"];

const APP_ID_FIREFOX = "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}";
const APP_ID_THUNDERBIRD = "{3550f703-e582-4d05-9a08-453d09bdfdc6}";

const FRAME_SCRIPT = "chrome://marionette/content/listener.js";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const SUPPORTED_STRATEGIES = new Set([
  element.Strategy.ClassName,
  element.Strategy.Selector,
  element.Strategy.ID,
  element.Strategy.Name,
  element.Strategy.LinkText,
  element.Strategy.PartialLinkText,
  element.Strategy.TagName,
  element.Strategy.XPath,
]);

// Timeout used to abort fullscreen, maximize, and minimize
// commands if no window manager is present.
const TIMEOUT_NO_WINDOW_MANAGER = 5000;

const globalMessageManager = Services.mm;

/**
 * The Marionette WebDriver services provides a standard conforming
 * implementation of the W3C WebDriver specification.
 *
 * @see {@link https://w3c.github.io/webdriver/webdriver-spec.html}
 * @namespace driver
 */

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
 * @param {MarionetteServer} server
 *     The instance of Marionette server.
 */
this.GeckoDriver = function(server) {
  this.appId = Services.appinfo.ID;
  this.appName = Services.appinfo.name.toLowerCase();
  this._server = server;

  this.sessionID = null;
  this.wins = new browser.Windows();
  this.browsers = {};
  // points to current browser
  this.curBrowser = null;
  // top-most chrome window
  this.mainFrame = null;
  // chrome iframe that currently has focus
  this.curFrame = null;
  // browsing context of current content frame
  this.currentFrameBrowsingContext = null;
  this.observing = null;
  this._browserIds = new WeakMap();

  // Use content context by default
  this.context = Context.Content;

  this.sandboxes = new Sandboxes(() => this.getCurrentWindow());
  this.legacyactions = new legacyaction.Chain();

  this.capabilities = new Capabilities();

  this.mm = globalMessageManager;
  this.listener = proxy.toListener(
    this.sendAsync.bind(this),
    () => this.curBrowser
  );

  // used for modal dialogs or tab modal alerts
  this.dialog = null;
  this.dialogObserver = null;
};

Object.defineProperty(GeckoDriver.prototype, "a11yChecks", {
  get() {
    return this.capabilities.get("moz:accessibilityChecks");
  },
});

/**
 * The current context decides if commands are executed in chrome- or
 * content space.
 */
Object.defineProperty(GeckoDriver.prototype, "context", {
  get() {
    return this._context;
  },

  set(context) {
    this._context = Context.fromString(context);
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
    return Services.wm.getEnumerator(null);
  },
});

Object.defineProperty(GeckoDriver.prototype, "windowType", {
  get() {
    return this.curBrowser.window.document.documentElement.getAttribute(
      "windowtype"
    );
  },
});

Object.defineProperty(GeckoDriver.prototype, "windowHandles", {
  get() {
    let hs = [];

    for (let win of this.windows) {
      let tabBrowser = browser.getTabBrowser(win);

      // Only return handles for browser windows
      if (tabBrowser && tabBrowser.tabs) {
        for (let tab of tabBrowser.tabs) {
          let winId = this.getIdForBrowser(browser.getBrowserForTab(tab));
          if (winId !== null) {
            hs.push(winId);
          }
        }
      }
    }

    return hs;
  },
});

Object.defineProperty(GeckoDriver.prototype, "chromeWindowHandles", {
  get() {
    let hs = [];

    for (let win of this.windows) {
      hs.push(getWindowId(win));
    }

    return hs;
  },
});

GeckoDriver.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIObserver",
  "nsISupportsWeakReference",
]);

GeckoDriver.prototype.init = function() {
  this.mm.addMessageListener("Marionette:WebDriver:GetCapabilities", this);
  this.mm.addMessageListener("Marionette:ListenersAttached", this);
  this.mm.addMessageListener("Marionette:Register", this);
  this.mm.addMessageListener("Marionette:switchedToFrame", this);
};

GeckoDriver.prototype.uninit = function() {
  this.mm.removeMessageListener("Marionette:WebDriver:GetCapabilities", this);
  this.mm.removeMessageListener("Marionette:ListenersAttached", this);
  this.mm.removeMessageListener("Marionette:Register", this);
  this.mm.removeMessageListener("Marionette:switchedToFrame", this);
};

/**
 * Callback used to observe the creation of new modal or tab modal dialogs
 * during the session's lifetime.
 */
GeckoDriver.prototype.handleModalDialog = function(action, dialog, win) {
  // Only care about modals of the currently selected window.
  if (win !== this.curBrowser.window) {
    return;
  }

  if (action === modal.ACTION_OPENED) {
    this.dialog = new modal.Dialog(() => this.curBrowser, dialog);
  } else if (action === modal.ACTION_CLOSED) {
    this.dialog = null;
  }
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

  if (payload === null) {
    payload = {};
  }

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
        "No such content frame; perhaps the listener was not registered?"
      );
    }
  });
};

/**
 * Get the current "MarionetteFrame" parent actor.
 *
 * @returns {MarionetteFrameParent}
 *     The parent actor.
 */
GeckoDriver.prototype.getActor = async function() {
  // TODO: Make it an `actor` property after removing async from
  // getBrowsingContext()
  const browsingContext = await this.getBrowsingContext();
  return browsingContext.currentWindowGlobal.getActor("MarionetteFrame");
};

/**
 * Get the browsing context.
 *
 * @param {boolean=} topContext
 *     If set to true use the window's top-level browsing context,
 *     otherwise the one from the currently selected frame. Defaults to false.
 *
 * @return {BrowsingContext}
 *     The browsing context.
 */
GeckoDriver.prototype.getBrowsingContext = async function(topContext = false) {
  let browsingContext = null;

  switch (this.context) {
    case Context.Chrome:
      browsingContext = this.getCurrentWindow().browsingContext;
      break;

    case Context.Content:
      const id = await this.listener.getBrowsingContextId(topContext);
      browsingContext = BrowsingContext.get(id);
      break;
  }

  return browsingContext;
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
  let context =
    typeof forcedContext == "undefined" ? this.context : forcedContext;
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
  return (
    this._reftest &&
    element &&
    element.tagName === "xul:browser" &&
    element.parentElement &&
    element.parentElement.id === "reftest"
  );
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
GeckoDriver.prototype.addBrowser = function(win) {
  let context = new browser.Context(win, this);
  let winId = getWindowId(win);

  this.browsers[winId] = context;
  this.curBrowser = this.browsers[winId];
  if (!this.wins.has(winId)) {
    // add this to seenItems so we can guarantee
    // the user will get winId as this window's id
    this.wins.set(winId, win);
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

    if (!MarionettePrefs.contentListener || !isNewSession) {
      // load listener into the remote frame
      // and any applicable new frames
      // opened after this call
      mm.loadFrameScript(FRAME_SCRIPT, true);
      MarionettePrefs.contentListener = true;
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
  // We want to ignore frames that are XUL browsers that aren't in the "main"
  // tabbrowser, but accept things on Fennec (which doesn't have a
  // xul:tabbrowser), and accept HTML iframes (because tests depend on it),
  // as well as XUL frames. Ideally this should be cleaned up and we should
  // keep track of browsers a different way.
  if (
    this.appId != APP_ID_FIREFOX ||
    be.namespaceURI != XUL_NS ||
    be.nodeName != "browser" ||
    be.getTabBrowser()
  ) {
    // curBrowser holds all the registered frames in knownFrames
    this.curBrowser.register(id, be);
  }

  this.currentFrameBrowsingContext = BrowsingContext.get(id);
  this.wins.set(id, this.currentFrameBrowsingContext.currentWindowGlobal);

  return id;
};

GeckoDriver.prototype.registerPromise = function() {
  const li = "Marionette:Register";

  return new Promise(resolve => {
    let cb = ({ json, target }) => {
      let { frameId } = json;
      this.registerBrowser(frameId, target);

      if (this.curBrowser.frameRegsPending > 0) {
        this.curBrowser.frameRegsPending--;
      }

      if (this.curBrowser.frameRegsPending === 0) {
        this.mm.removeMessageListener(li, cb);
        resolve();
      }

      return { frameId };
    };
    this.mm.addMessageListener(li, cb);
  });
};

GeckoDriver.prototype.listeningPromise = function() {
  const li = "Marionette:ListenersAttached";

  return new Promise(resolve => {
    let cb = msg => {
      if (msg.json.frameId === this.curBrowser.curFrameId) {
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
 *  <dt><code>moz:useNonSpecCompliantPointerOrigin</code> (boolean)
 *  <dd>Use the not WebDriver conforming calculation of the pointer origin
 *   when the origin is an element, and the element center point is used.
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
    this.capabilities = Capabilities.fromJSON(cmd.parameters);

    if (!this.secureTLS) {
      logger.warn("TLS certificate errors will be ignored for this session");
      allowAllCerts.enable();
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
    let windowTypes;
    switch (this.appId) {
      case APP_ID_THUNDERBIRD:
        windowTypes = ["mail:3pane"];
        break;
      default:
        // We assume that an app either has GeckoView windows, or
        // Firefox/Fennec windows, but not both.
        windowTypes = ["navigator:browser", "navigator:geckoview"];
        break;
    }
    let win;
    for (let windowType of windowTypes) {
      win = Services.wm.getMostRecentWindow(windowType);
      if (win) {
        break;
      }
    }
    if (!win) {
      // if the window isn't even created, just poll wait for it
      let checkTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      checkTimer.initWithCallback(
        waitForWindow.bind(this),
        100,
        Ci.nsITimer.TYPE_ONE_SHOT
      );
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
      if (MarionettePrefs.clickToStart) {
        Services.prompt.alert(
          win,
          "",
          "Click to start execution of marionette tests"
        );
      }
      this.startBrowser(win, true);
    }
  };

  if (!MarionettePrefs.contentListener) {
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

  if (MarionettePrefs.useActors) {
    // Register the JSWindowActor pair as used by Marionette
    ChromeUtils.registerWindowActor("MarionetteFrame", {
      kind: "JSWindowActor",
      parent: {
        moduleURI:
          "chrome://marionette/content/actors/MarionetteFrameParent.jsm",
      },
      child: {
        moduleURI:
          "chrome://marionette/content/actors/MarionetteFrameChild.jsm",
        events: {
          beforeunload: { capture: true },
          DOMContentLoaded: { mozSystemGroup: true },
          pagehide: { mozSystemGroup: true },
          pageshow: { mozSystemGroup: true },
        },
      },

      allFrames: true,
      includeChrome: true,
    });
  }

  if (this.mainFrame) {
    this.mainFrame.focus();
  }

  if (this.curBrowser.tab) {
    this.curBrowser.contentBrowser.focus();
  }

  // Setup observer for modal dialogs
  this.dialogObserver = new modal.DialogObserver(this);
  this.dialogObserver.add(this.handleModalDialog.bind(this));

  // Check if there is already an open dialog for the selected browser window.
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
GeckoDriver.prototype.getSessionCapabilities = function() {
  return { capabilities: this.capabilities };
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

  this.context = value;
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
 *
 * @return {(string|boolean|number|object|WebElement)}
 *     Return value from the script, or null which signifies either the
 *     JavaScript notion of null or undefined.
 *
 * @throws {ScriptTimeoutError}
 *     If the script was interrupted due to reaching the session's
 *     script timeout.
 * @throws {JavaScriptError}
 *     If an {@link Error} was thrown whilst evaluating the script.
 */
GeckoDriver.prototype.executeScript = async function(cmd) {
  let { script, args } = cmd.parameters;
  let opts = {
    script: cmd.parameters.script,
    args: cmd.parameters.args,
    sandboxName: cmd.parameters.sandbox,
    newSandbox: cmd.parameters.newSandbox,
    file: cmd.parameters.filename,
    line: cmd.parameters.line,
  };

  return { value: await this.execute_(script, args, opts) };
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
 *
 * @return {(string|boolean|number|object|WebElement)}
 *     Return value from the script, or null which signifies either the
 *     JavaScript notion of null or undefined.
 *
 * @throws {ScriptTimeoutError}
 *     If the script was interrupted due to reaching the session's
 *     script timeout.
 * @throws {JavaScriptError}
 *     If an Error was thrown whilst evaluating the script.
 */
GeckoDriver.prototype.executeAsyncScript = async function(cmd) {
  let { script, args } = cmd.parameters;
  let opts = {
    script: cmd.parameters.script,
    args: cmd.parameters.args,
    sandboxName: cmd.parameters.sandbox,
    newSandbox: cmd.parameters.newSandbox,
    file: cmd.parameters.filename,
    line: cmd.parameters.line,
    async: true,
  };

  return { value: await this.execute_(script, args, opts) };
};

GeckoDriver.prototype.execute_ = async function(
  script,
  args = [],
  {
    sandboxName = null,
    newSandbox = false,
    file = "",
    line = 0,
    async = false,
  } = {}
) {
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  assert.string(script, pprint`Expected "script" to be a string: ${script}`);
  assert.array(args, pprint`Expected script args to be an array: ${args}`);
  if (sandboxName !== null) {
    assert.string(
      sandboxName,
      pprint`Expected sandbox name to be a string: ${sandboxName}`
    );
  }
  assert.boolean(
    newSandbox,
    pprint`Expected newSandbox to be boolean: ${newSandbox}`
  );
  assert.string(file, pprint`Expected file to be a string: ${file}`);
  assert.number(line, pprint`Expected line to be a number: ${line}`);

  let opts = {
    timeout: this.timeouts.script,
    sandboxName,
    newSandbox,
    file,
    line,
    async,
  };

  let res, els;

  switch (this.context) {
    case Context.Content:
      // evaluate in content with lasting side-effects
      if (!sandboxName) {
        res = await this.listener.execute(script, args, opts);

        // evaluate in content with sandbox
      } else {
        res = await this.listener.executeInSandbox(script, args, opts);
      }

      break;

    case Context.Chrome:
      let sb = this.sandboxes.get(sandboxName, newSandbox);
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
GeckoDriver.prototype.navigateTo = async function(cmd) {
  assert.content(this.context);
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  let validURL;
  try {
    validURL = new URL(cmd.parameters.url);
  } catch (e) {
    throw new InvalidArgumentError(`Malformed URL: ${e.message}`);
  }

  // We need to move to the top frame before navigating
  await this.listener.switchToFrame();

  const loadEventExpected = navigate.isLoadEventExpected(
    this.currentURL,
    validURL
  );

  const navigated = this.listener.navigateTo({
    url: validURL,
    loadEventExpected,
    pageTimeout: this.timeouts.pageLoad,
  });

  // If a process change of the frame script interrupts our page load, this
  // will never return. We need to re-issue this request to correctly poll for
  // readyState and send errors.
  this.curBrowser.pendingCommands.push(() => {
    let parameters = {
      // TODO(ato): Bug 1242595
      commandID: this.listener.activeMessageId,
      pageTimeout: this.timeouts.pageLoad,
      startTime: new Date().getTime(),
    };
    this.curBrowser.messageManager.sendAsyncMessage(
      "Marionette:waitForPageLoaded",
      parameters
    );
  });

  await navigated;

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
GeckoDriver.prototype.getCurrentUrl = async function() {
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

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
GeckoDriver.prototype.getTitle = async function() {
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  return this.title;
};

/** Gets the current type of the window. */
GeckoDriver.prototype.getWindowType = function() {
  assert.open(this.getCurrentWindow());

  return this.windowType;
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
GeckoDriver.prototype.getPageSource = async function() {
  const win = assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  switch (this.context) {
    case Context.Chrome:
      let s = new win.XMLSerializer();
      return s.serializeToString(win.document);

    case Context.Content:
      return this.listener.getPageSource();

    default:
      throw new TypeError(`Unknown context: ${this.context}`);
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
  assert.open(this.curBrowser);
  await this._handleUserPrompts();

  // If there is no history, just return
  if (!this.curBrowser.contentBrowser.webNavigation.canGoBack) {
    return;
  }

  let lastURL = this.currentURL;
  let goBack = this.listener.goBack({ pageTimeout: this.timeouts.pageLoad });

  // If a process change of the frame script interrupts our page load, this
  // will never return. We need to re-issue this request to correctly poll for
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
      "Marionette:waitForPageLoaded",
      parameters
    );
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
  assert.open(this.curBrowser);
  await this._handleUserPrompts();

  // If there is no history, just return
  if (!this.curBrowser.contentBrowser.webNavigation.canGoForward) {
    return;
  }

  let lastURL = this.currentURL;
  let goForward = this.listener.goForward({
    pageTimeout: this.timeouts.pageLoad,
  });

  // If a process change of the frame script interrupts our page load, this
  // will never return. We need to re-issue this request to correctly poll for
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
      "Marionette:waitForPageLoaded",
      parameters
    );
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
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  let refresh = this.listener.refresh({ pageTimeout: this.timeouts.pageLoad });

  // If a process change of the frame script interrupts our page load, this
  // will never return. We need to re-issue this request to correctly poll for
  // readyState and send errors.
  this.curBrowser.pendingCommands.push(() => {
    let parameters = {
      // TODO(ato): Bug 1242595
      commandID: this.listener.activeMessageId,
      pageTimeout: this.timeouts.pageLoad,
      startTime: new Date().getTime(),
    };
    this.curBrowser.messageManager.sendAsyncMessage(
      "Marionette:waitForPageLoaded",
      parameters
    );
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

  let winId = browser.browsingContext.id;
  if (winId) {
    this._browserIds.set(permKey, winId);
    return winId;
  }
  return null;
};

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
  assert.open(this.curBrowser);

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
 * @throws {UnknownError}
 *     Internal browsing context reference not found
 */
GeckoDriver.prototype.getChromeWindowHandle = function() {
  assert.open(this.getCurrentWindow(Context.Chrome));

  for (let i in this.browsers) {
    if (this.curBrowser == this.browsers[i]) {
      return i;
    }
  }

  throw new UnknownError("Invalid browsing context");
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
GeckoDriver.prototype.getWindowRect = async function() {
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  return this.curBrowser.rect;
};

/**
 * Set the window position and size of the browser on the operating
 * system window manager.
 *
 * The supplied `width` and `height` values refer to the window `outerWidth`
 * and `outerHeight` values, which include browser chrome and OS-level
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
 *     Object with `x` and `y` coordinates and `width` and `height`
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
  const win = assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  let { x, y, width, height } = cmd.parameters;

  switch (WindowState.from(win.windowState)) {
    case WindowState.Fullscreen:
      await exitFullscreen(win);
      break;

    case WindowState.Maximized:
    case WindowState.Minimized:
      await restoreWindow(win);
      break;
  }

  if (width != null && height != null) {
    assert.positiveInteger(height);
    assert.positiveInteger(width);

    if (win.outerWidth != width || win.outerHeight != height) {
      win.resizeTo(width, height);
      await new IdlePromise(win);
    }
  }

  if (x != null && y != null) {
    assert.integer(x);
    assert.integer(y);

    if (win.screenX != x || win.screenY != y) {
      win.moveTo(x, y);
      await new IdlePromise(win);
    }
  }

  return this.curBrowser.rect;
};

/**
 * Switch current top-level browsing context by name or server-assigned
 * ID.  Searches for windows by name, then ID.  Content windows take
 * precedence.
 *
 * @param {string} handle
 *     Handle of the window to switch to.
 * @param {boolean=} focus
 *     A boolean value which determines whether to focus
 *     the window. Defaults to true.
 */
GeckoDriver.prototype.switchToWindow = async function(cmd) {
  const { focus = true, handle } = cmd.parameters;

  assert.string(
    handle,
    pprint`Expected "handle" to be a string, got ${handle}`
  );
  assert.boolean(focus, pprint`Expected "focus" to be a boolean, got ${focus}`);

  const id = parseInt(handle);
  const found = this.findWindow(this.windows, (win, winId) => id == winId);
  if (found) {
    await this.setWindowHandle(found, focus);
  } else {
    throw new NoSuchWindowError(`Unable to locate window: ${handle}`);
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
  for (const win of winIterable) {
    const bc = win.docShell.browsingContext;
    const tabBrowser = browser.getTabBrowser(win);

    // In case the wanted window is a chrome window, we are done.
    if (filter(win, bc.id)) {
      return { win, id: bc.id, hasTabBrowser: !!tabBrowser };

      // Otherwise check if the chrome window has a tab browser, and that it
      // contains a tab with the wanted window handle.
    } else if (tabBrowser && tabBrowser.tabs) {
      for (let i = 0; i < tabBrowser.tabs.length; ++i) {
        let contentBrowser = browser.getBrowserForTab(tabBrowser.tabs[i]);
        let contentWindowId = this.getIdForBrowser(contentBrowser);

        if (filter(win, contentWindowId)) {
          return {
            win,
            id: bc.id,
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
 * the window is unregistered, register that browser and wait for
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
  winProperties,
  focus = true
) {
  if (!(winProperties.id in this.browsers)) {
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
    // Otherwise switch to the known chrome window
    this.curBrowser = this.browsers[winProperties.id];
    this.mainFrame = this.curBrowser.window;
    this.curFrame = null;

    // .. and activate the tab if it's a content browser.
    if ("tabIndex" in winProperties) {
      await this.curBrowser.switchToTab(
        winProperties.tabIndex,
        winProperties.win,
        focus
      );
    }
  }

  if (focus) {
    await this.curBrowser.focusWindow();
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
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  await this.listener.switchToParentFrame();
};

/**
 * Switch to a given frame within the current window.
 *
 * @param {boolean=} focus
 *     Focus the frame if set to true. Defaults to false.
 * @param {(string|Object)=} element
 *     A web element reference of the frame or its element id.
 * @param {number=} id
 *     The index of the frame to switch to.
 *     If both element and id are not defined, switch to top-level frame.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.switchToFrame = async function(cmd) {
  const { element, focus = false, id } = cmd.parameters;

  const curWindow = assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  if (typeof id == "number") {
    assert.unsignedShort(id, `Expected id to be unsigned short, got ${id}`);
  }

  const checkLoad = function(win) {
    const otherErrorsExpr = /about:.+(error)|(blocked)\?/;

    return new PollPromise(resolve => {
      if (win.document.readyState == "complete") {
        resolve();
      } else if (win.document.readyState == "interactive") {
        let documentURI = win.document.documentURI;
        if (documentURI.startsWith("about:certerror")) {
          throw new InsecureCertificateError();
        } else if (otherErrorsExpr.exec(documentURI)) {
          throw new UnknownError("Reached error page: " + documentURI);
        }
      }
    });
  };

  if (this.context == Context.Chrome) {
    let foundFrame = null;

    // Bug 1495063: Elements should be passed as WebElement reference
    let byFrame;
    if (typeof element == "string") {
      byFrame = WebElement.fromUUID(element, Context.Chrome);
    } else if (element) {
      byFrame = WebElement.fromJSON(element);
    }

    if (id == null && !byFrame) {
      this.curFrame = null;
    } else if (typeof id == "number") {
      if (curWindow.frames[id]) {
        const frameEl = curWindow.frames[id].frameElement;
        this.curFrame = frameEl.contentWindow;
      } else {
        throw new NoSuchFrameError(`Unable to locate frame with index: ${id}`);
      }
    } else {
      const wantedFrame = this.curBrowser.seenEls.get(byFrame);

      // Deal with an embedded xul:browser case
      if (["browser", "xul:browser"].includes(wantedFrame.tagName)) {
        this.curFrame = wantedFrame.contentWindow;
      } else {
        const frames = curWindow.document.getElementsByTagName("iframe");
        const wrappedWanted = new XPCNativeWrapper(wantedFrame);

        foundFrame = Array.prototype.find.call(frames, frame => {
          return new XPCNativeWrapper(frame) === wrappedWanted;
        });
        if (foundFrame) {
          this.curFrame = foundFrame.contentWindow;
        } else {
          throw new NoSuchFrameError(`Unable to locate frame: ${byFrame}`);
        }
      }
    }

    const frameWindow = this.curFrame || this.mainFrame;
    await checkLoad(frameWindow);

    if (focus) {
      frameWindow.focus();
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
  this.timeouts = Timeouts.fromJSON(merged);
};

/** Single tap. */
GeckoDriver.prototype.singleTap = async function(cmd) {
  assert.open(this.getCurrentWindow());

  let { id, x, y } = cmd.parameters;
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      throw new UnsupportedOperationError(
        "Command 'singleTap' is not yet available in chrome context"
      );

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
  assert.content(
    this.context,
    "Command 'performActions' is not yet available in chrome context"
  );
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  let actions = cmd.parameters.actions;
  await this.listener.performActions({ actions });
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
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

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
GeckoDriver.prototype.actionChain = async function(cmd) {
  const win = assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  let { chain, nextId } = cmd.parameters;

  switch (this.context) {
    case Context.Chrome:
      // be conservative until this has a use case and is established
      // to work as expected in Fennec
      assert.firefox();

      return this.legacyactions.dispatchActions(
        chain,
        nextId,
        { frame: win },
        this.curBrowser.seenEls
      );

    case Context.Content:
      return this.listener.actionChain(chain, nextId);

    default:
      throw new TypeError(`Unknown context: ${this.context}`);
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
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  let { value, max_length } = cmd.parameters; // eslint-disable-line camelcase
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
GeckoDriver.prototype.findElement = async function(cmd) {
  const { element: el, using, value } = cmd.parameters;

  if (!SUPPORTED_STRATEGIES.has(using)) {
    throw new InvalidSelectorError(`Strategy not supported: ${using}`);
  }

  const win = assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  let startNode;
  if (typeof el != "undefined") {
    startNode = WebElement.fromUUID(el, this.context);
  }

  let opts = {
    startNode,
    timeout: this.timeouts.implicit,
    all: false,
  };

  if (MarionettePrefs.useActors) {
    const actor = await this.getActor();
    return actor.findElement(using, value, opts);
  }

  switch (this.context) {
    case Context.Chrome:
      let container = { frame: win };
      if (opts.startNode) {
        opts.startNode = this.curBrowser.seenEls.get(opts.startNode);
      }
      let el = await element.find(container, using, value, opts);
      return this.curBrowser.seenEls.add(el);

    case Context.Content:
      return this.listener.findElementContent(using, value, opts);

    default:
      throw new TypeError(`Unknown context: ${this.context}`);
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
GeckoDriver.prototype.findElements = async function(cmd) {
  const { element: el, using, value } = cmd.parameters;

  if (!SUPPORTED_STRATEGIES.has(using)) {
    throw new InvalidSelectorError(`Strategy not supported: ${using}`);
  }

  const win = assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  let startNode;
  if (typeof el != "undefined") {
    startNode = WebElement.fromUUID(el, this.context);
  }

  let opts = {
    startNode,
    timeout: this.timeouts.implicit,
    all: true,
  };

  if (MarionettePrefs.useActors) {
    const actor = await this.getActor();
    return actor.findElements(using, value, opts);
  }

  switch (this.context) {
    case Context.Chrome:
      let container = { frame: win };
      if (startNode) {
        opts.startNode = this.curBrowser.seenEls.get(opts.startNode);
      }
      let els = await element.find(container, using, value, opts);
      return this.curBrowser.seenEls.addAll(els);

    case Context.Content:
      return this.listener.findElementsContent(using, value, opts);

    default:
      throw new TypeError(`Unknown context: ${this.context}`);
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
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

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
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      let el = this.curBrowser.seenEls.get(webEl);
      await interaction.clickElement(el, this.a11yChecks);
      break;

    case Context.Content:
      let click = this.listener.clickElement({
        webElRef: webEl.toJSON(),
        pageTimeout: this.timeouts.pageLoad,
      });

      // If a process change of the frame script interrupts our page load,
      // this will never return. We need to re-issue this request to correctly
      // poll for readyState and send errors.
      this.curBrowser.pendingCommands.push(() => {
        let parameters = {
          // TODO(ato): Bug 1242595
          commandID: this.listener.activeMessageId,
          pageTimeout: this.timeouts.pageLoad,
          startTime: new Date().getTime(),
        };
        this.curBrowser.messageManager.sendAsyncMessage(
          "Marionette:waitForPageLoaded",
          parameters
        );
      });

      await click;
      break;

    default:
      throw new TypeError(`Unknown context: ${this.context}`);
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
GeckoDriver.prototype.getElementAttribute = async function(cmd) {
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  const id = assert.string(cmd.parameters.id);
  const name = assert.string(cmd.parameters.name);
  const webEl = WebElement.fromUUID(id, this.context);

  if (MarionettePrefs.useActors) {
    const actor = await this.getActor();
    return actor.getElementAttribute(webEl, name);
  }

  switch (this.context) {
    case Context.Chrome:
      let el = this.curBrowser.seenEls.get(webEl);
      return el.getAttribute(name);

    case Context.Content:
      return this.listener.getElementAttribute(webEl, name);

    default:
      throw new TypeError(`Unknown context: ${this.context}`);
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
GeckoDriver.prototype.getElementProperty = async function(cmd) {
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  const id = assert.string(cmd.parameters.id);
  const name = assert.string(cmd.parameters.name);
  const webEl = WebElement.fromUUID(id, this.context);

  if (MarionettePrefs.useActors) {
    const actor = await this.getActor();
    return actor.getElementProperty(webEl, name);
  }

  switch (this.context) {
    case Context.Chrome:
      let el = this.curBrowser.seenEls.get(webEl);
      return evaluate.toJSON(el[name], this.curBrowser.seenEls);

    case Context.Content:
      return this.listener.getElementProperty(webEl, name);

    default:
      throw new TypeError(`Unknown context: ${this.context}`);
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
GeckoDriver.prototype.getElementText = async function(cmd) {
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      // for chrome, we look at text nodes, and any node with a "label" field
      let el = this.curBrowser.seenEls.get(webEl);
      let lines = [];
      this.getVisibleText(el, lines);
      return lines.join("\n");

    case Context.Content:
      return this.listener.getElementText(webEl);

    default:
      throw new TypeError(`Unknown context: ${this.context}`);
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
GeckoDriver.prototype.getElementTagName = async function(cmd) {
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      let el = this.curBrowser.seenEls.get(webEl);
      return el.tagName.toLowerCase();

    case Context.Content:
      return this.listener.getElementTagName(webEl);

    default:
      throw new TypeError(`Unknown context: ${this.context}`);
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
GeckoDriver.prototype.isElementDisplayed = async function(cmd) {
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      let el = this.curBrowser.seenEls.get(webEl);
      return interaction.isElementDisplayed(el, this.a11yChecks);

    case Context.Content:
      return this.listener.isElementDisplayed(webEl);

    default:
      throw new TypeError(`Unknown context: ${this.context}`);
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
GeckoDriver.prototype.getElementValueOfCssProperty = async function(cmd) {
  const win = assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  let id = assert.string(cmd.parameters.id);
  let prop = assert.string(cmd.parameters.propertyName);
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      let el = this.curBrowser.seenEls.get(webEl);
      let sty = win.document.defaultView.getComputedStyle(el);
      return sty.getPropertyValue(prop);

    case Context.Content:
      return this.listener.getElementValueOfCssProperty(webEl, prop);

    default:
      throw new TypeError(`Unknown context: ${this.context}`);
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
GeckoDriver.prototype.isElementEnabled = async function(cmd) {
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      // Selenium atom doesn't quite work here
      let el = this.curBrowser.seenEls.get(webEl);
      return interaction.isElementEnabled(el, this.a11yChecks);

    case Context.Content:
      return this.listener.isElementEnabled(webEl);

    default:
      throw new TypeError(`Unknown context: ${this.context}`);
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
GeckoDriver.prototype.isElementSelected = async function(cmd) {
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      // Selenium atom doesn't quite work here
      let el = this.curBrowser.seenEls.get(webEl);
      return interaction.isElementSelected(el, this.a11yChecks);

    case Context.Content:
      return this.listener.isElementSelected(webEl);

    default:
      throw new TypeError(`Unknown context: ${this.context}`);
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
GeckoDriver.prototype.getElementRect = async function(cmd) {
  const win = assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      let el = this.curBrowser.seenEls.get(webEl);
      let rect = el.getBoundingClientRect();
      return {
        x: rect.x + win.pageXOffset,
        y: rect.y + win.pageYOffset,
        width: rect.width,
        height: rect.height,
      };

    case Context.Content:
      return this.listener.getElementRect(webEl);

    default:
      throw new TypeError(`Unknown context: ${this.context}`);
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
 *     If `id` or `text` are not strings.
 * @throws {NoSuchElementError}
 *     If element represented by reference `id` is unknown.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.sendKeysToElement = async function(cmd) {
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  let id = assert.string(cmd.parameters.id);
  let text = assert.string(cmd.parameters.text);
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      let el = this.curBrowser.seenEls.get(webEl);
      await interaction.sendKeysToElement(el, text, {
        accessibilityChecks: this.a11yChecks,
      });
      break;

    case Context.Content:
      await this.listener.sendKeysToElement(webEl, text);
      break;

    default:
      throw new TypeError(`Unknown context: ${this.context}`);
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
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  switch (this.context) {
    case Context.Chrome:
      // the selenium atom doesn't work here
      let el = this.curBrowser.seenEls.get(webEl);
      if (el.nodeName == "input" && el.type == "text") {
        el.value = "";
      } else if (el.nodeName == "checkbox") {
        el.checked = false;
      }
      break;

    case Context.Content:
      await this.listener.clearElement(webEl);
      break;

    default:
      throw new TypeError(`Unknown context: ${this.context}`);
  }
};

/**
 * Switch to shadow root of the given host element.
 *
 * @param {string=} id
 *     Reference ID to the element.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 */
GeckoDriver.prototype.switchToShadowRoot = async function(cmd) {
  assert.content(this.context);
  assert.open(this.getCurrentWindow());

  let id = cmd.parameters.id;
  let webEl = null;
  if (id != null) {
    assert.string(id);
    webEl = WebElement.fromUUID(id, this.context);
  }
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
GeckoDriver.prototype.addCookie = async function(cmd) {
  assert.content(this.context);
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  let { protocol, hostname } = this.currentURL;

  const networkSchemes = ["ftp:", "http:", "https:"];
  if (!networkSchemes.includes(protocol)) {
    throw new InvalidCookieDomainError("Document is cookie-averse");
  }

  let newCookie = cookie.fromJSON(cmd.parameters.cookie);

  cookie.add(newCookie, { restrictToHost: hostname, protocol });
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
GeckoDriver.prototype.getCookies = async function() {
  assert.content(this.context);
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  let { hostname, pathname } = this.currentURL;
  return [...cookie.iter(hostname, pathname)];
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
GeckoDriver.prototype.deleteAllCookies = async function() {
  assert.content(this.context);
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  let { hostname, pathname } = this.currentURL;
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
GeckoDriver.prototype.deleteCookie = async function(cmd) {
  assert.content(this.context);
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  let { hostname, pathname } = this.currentURL;
  let name = assert.string(cmd.parameters.name);
  for (let c of cookie.iter(hostname, pathname)) {
    if (c.name === name) {
      cookie.remove(c);
    }
  }
};

/**
 * Open a new top-level browsing context.
 *
 * @param {string=} type
 *     Optional type of the new top-level browsing context. Can be one of
 *     `tab` or `window`. Defaults to `tab`.
 * @param {boolean=} focus
 *     Optional flag if the new top-level browsing context should be opened
 *     in foreground (focused) or background (not focused). Defaults to false.
 * @param {boolean=} private
 *     Optional flag, which gets only evaluated for type `window`. True if the
 *     new top-level browsing context should be a private window.
 *     Defaults to false.
 *
 * @return {Object.<string, string>}
 *     Handle and type of the new browsing context.
 */
GeckoDriver.prototype.newWindow = async function(cmd) {
  assert.open(this.getCurrentWindow(Context.Content));
  await this._handleUserPrompts();

  let focus = false;
  if (typeof cmd.parameters.focus != "undefined") {
    focus = assert.boolean(
      cmd.parameters.focus,
      pprint`Expected "focus" to be a boolean, got ${cmd.parameters.focus}`
    );
  }

  let isPrivate = false;
  if (typeof cmd.parameters.private != "undefined") {
    isPrivate = assert.boolean(
      cmd.parameters.private,
      pprint`Expected "private" to be a boolean, got ${cmd.parameters.private}`
    );
  }

  let type;
  if (typeof cmd.parameters.type != "undefined") {
    type = assert.string(
      cmd.parameters.type,
      pprint`Expected "type" to be a string, got ${cmd.parameters.type}`
    );
  }

  // If an invalid or no type has been specified default to a tab.
  if (typeof type == "undefined" || !["tab", "window"].includes(type)) {
    type = "tab";
  }

  let contentBrowser;

  switch (type) {
    case "window":
      let win = await this.curBrowser.openBrowserWindow(focus, isPrivate);
      contentBrowser = browser.getTabBrowser(win).selectedBrowser;
      break;

    default:
      // To not fail if a new type gets added in the future, make opening
      // a new tab the default action.
      let tab = await this.curBrowser.openTab(focus);
      contentBrowser = browser.getBrowserForTab(tab);
  }

  // Even with the framescript registered, the browser might not be known to
  // the parent process yet. Wait until it is available.
  // TODO: Fix by using `Browser:Init` or equivalent on bug 1311041
  let windowId = await new PollPromise((resolve, reject) => {
    let id = this.getIdForBrowser(contentBrowser);
    this.windowHandles.includes(id) ? resolve(id) : reject();
  });

  return { handle: windowId.toString(), type };
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
  assert.open(this.getCurrentWindow(Context.Content));
  await this._handleUserPrompts();

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
  assert.open(this.getCurrentWindow(Context.Chrome));

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
    MarionettePrefs.contentListener = false;

    globalMessageManager.broadcastAsyncMessage("Marionette:Session:Delete");
    globalMessageManager.broadcastAsyncMessage("Marionette:Deregister");

    for (let win of this.windows) {
      if (win.messageManager) {
        win.messageManager.removeDelayedFrameScript(FRAME_SCRIPT);
      } else {
        logger.error(
          `Could not remove listener from page ${win.location.href}`
        );
      }
    }
  }

  if (MarionettePrefs.useActors) {
    ChromeUtils.unregisterWindowActor("MarionetteFrame");
  }

  // reset frame to the top-most frame, and clear reference to chrome window
  this.curFrame = null;
  this.mainFrame = null;

  if (this.observing !== null) {
    for (let topic in this.observing) {
      Services.obs.removeObserver(this.observing[topic], topic);
    }
    this.observing = null;
  }

  if (this.dialogObserver) {
    this.dialogObserver.cleanup();
    this.dialogObserver = null;
  }

  this.sandboxes.clear();
  allowAllCerts.disable();

  this.sessionID = null;
  this.capabilities = new Capabilities();
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
 * @param {boolean=} full
 *     True to take a screenshot of the entire document element. Is only
 *     considered if <var>id</var> is not defined. Defaults to true.
 * @param {boolean=} hash
 *     True if the user requests a hash of the image data. Defaults to false.
 * @param {boolean=} scroll
 *     Scroll to element if |id| is provided. Defaults to true.
 *
 * @return {string}
 *     If <var>hash</var> is false, PNG image encoded as Base64 encoded
 *     string.  If <var>hash</var> is true, hex digest of the SHA-256
 *     hash of the Base64 encoded string.
 */
GeckoDriver.prototype.takeScreenshot = async function(cmd) {
  let win = assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  let { id, full, hash, scroll } = cmd.parameters;
  let format = hash ? capture.Format.Hash : capture.Format.Base64;

  full = typeof full == "undefined" ? true : full;
  scroll = typeof scroll == "undefined" ? true : scroll;

  let webEl = id ? WebElement.fromUUID(id, this.context) : null;

  // Only consider full screenshot if no element has been specified
  full = webEl ? false : full;

  let rect;
  switch (this.context) {
    case Context.Chrome:
      if (id) {
        let el = this.curBrowser.seenEls.get(webEl, win);
        rect = el.getBoundingClientRect();
      } else if (full) {
        const docEl = win.document.documentElement;
        rect = new DOMRect(0, 0, docEl.scrollWidth, docEl.scrollHeight);
      } else {
        // viewport
        rect = new win.DOMRect(
          win.pageXOffset,
          win.pageYOffset,
          win.innerWidth,
          win.innerHeight
        );
      }
      break;

    case Context.Content:
      rect = await this.listener.getScreenshotRect({ el: webEl, full, scroll });
      break;
  }

  // If no element has been specified use the top-level browsing context.
  // Otherwise use the browsing context from the currently selected frame.
  const browsingContext = await this.getBrowsingContext(!webEl);

  let canvas = await capture.canvas(
    win,
    browsingContext,
    rect.x,
    rect.y,
    rect.width,
    rect.height
  );

  switch (format) {
    case capture.Format.Hash:
      return capture.toHash(canvas);

    case capture.Format.Base64:
      return capture.toBase64(canvas);
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
GeckoDriver.prototype.getScreenOrientation = function() {
  assert.fennec();
  let win = assert.open(this.getCurrentWindow());

  return win.screen.mozOrientation;
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
  let win = assert.open(this.getCurrentWindow());

  const ors = [
    "portrait",
    "landscape",
    "portrait-primary",
    "landscape-primary",
    "portrait-secondary",
    "landscape-secondary",
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
  const win = assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  switch (WindowState.from(win.windowState)) {
    case WindowState.Fullscreen:
      await exitFullscreen(win);
      break;

    case WindowState.Maximized:
      await restoreWindow(win);
      break;
  }

  if (WindowState.from(win.windowState) != WindowState.Minimized) {
    let cb;
    let observer = new WebElementEventTarget(this.curBrowser.messageManager);
    // Use a timed promise to abort if no window manager is present
    await new TimedPromise(
      resolve => {
        cb = new DebounceCallback(resolve);
        observer.addEventListener("visibilitychange", cb);
        win.minimize();
      },
      { throws: null, timeout: TIMEOUT_NO_WINDOW_MANAGER }
    );
    observer.removeEventListener("visibilitychange", cb);
    await new IdlePromise(win);
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
  const win = assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  switch (WindowState.from(win.windowState)) {
    case WindowState.Fullscreen:
      await exitFullscreen(win);
      break;

    case WindowState.Minimized:
      await restoreWindow(win);
      break;
  }

  if (WindowState.from(win.windowState) != WindowState.Maximized) {
    let cb;
    // Use a timed promise to abort if no window manager is present
    await new TimedPromise(
      resolve => {
        cb = new DebounceCallback(resolve);
        win.addEventListener("sizemodechange", cb);
        win.maximize();
      },
      { throws: null, timeout: TIMEOUT_NO_WINDOW_MANAGER }
    );
    win.removeEventListener("sizemodechange", cb);
    await new IdlePromise(win);
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
  const win = assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  switch (WindowState.from(win.windowState)) {
    case WindowState.Maximized:
    case WindowState.Minimized:
      await restoreWindow(win);
      break;
  }

  if (WindowState.from(win.windowState) != WindowState.Fullscreen) {
    let cb;
    // Use a timed promise to abort if no window manager is present
    await new TimedPromise(
      resolve => {
        cb = new DebounceCallback(resolve);
        win.addEventListener("sizemodechange", cb);
        win.fullScreen = true;
      },
      { throws: null, timeout: TIMEOUT_NO_WINDOW_MANAGER }
    );
    win.removeEventListener("sizemodechange", cb);
  }
  await new IdlePromise(win);

  return this.curBrowser.rect;
};

/**
 * Dismisses a currently displayed tab modal, or returns no such alert if
 * no modal is displayed.
 */
GeckoDriver.prototype.dismissDialog = async function() {
  let win = assert.open(this.getCurrentWindow());
  this._checkIfAlertIsPresent();

  let dialogClosed = waitForEvent(win, "DOMModalDialogClosed");

  let { button0, button1 } = this.dialog.ui;
  (button1 ? button1 : button0).click();

  await dialogClosed;
  await new IdlePromise(win);
};

/**
 * Accepts a currently displayed tab modal, or returns no such alert if
 * no modal is displayed.
 */
GeckoDriver.prototype.acceptDialog = async function() {
  let win = assert.open(this.getCurrentWindow());
  this._checkIfAlertIsPresent();

  let dialogClosed = waitForEvent(win, "DOMModalDialogClosed");

  let { button0 } = this.dialog.ui;
  button0.click();

  await dialogClosed;
  await new IdlePromise(win);
};

/**
 * Returns the message shown in a currently displayed modal, or returns
 * a no such alert error if no modal is currently displayed.
 */
GeckoDriver.prototype.getTextFromDialog = function() {
  assert.open(this.getCurrentWindow());
  this._checkIfAlertIsPresent();

  return this.dialog.ui.infoBody.textContent;
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
  assert.open(this.getCurrentWindow());
  this._checkIfAlertIsPresent();

  let text = assert.string(cmd.parameters.text);
  let promptType = this.dialog.args.promptType;

  switch (promptType) {
    case "alert":
    case "confirm":
      throw new ElementNotInteractableError(
        `User prompt of type ${promptType} is not interactable`
      );
    case "prompt":
      break;
    default:
      await this.dismissDialog();
      throw new UnsupportedOperationError(
        `User prompt of type ${promptType} is not supported`
      );
  }

  // see toolkit/components/prompts/content/commonDialog.js
  let { loginTextbox } = this.dialog.ui;
  loginTextbox.value = text;
};

GeckoDriver.prototype._checkIfAlertIsPresent = function() {
  if (!this.dialog || !this.dialog.ui) {
    throw new NoSuchAlertError();
  }
};

GeckoDriver.prototype._handleUserPrompts = async function() {
  if (!this.dialog || !this.dialog.ui) {
    return;
  }

  let { textContent } = this.dialog.ui.infoBody;

  let behavior = this.capabilities.get("unhandledPromptBehavior");
  switch (behavior) {
    case UnhandledPromptBehavior.Accept:
      await this.acceptDialog();
      break;

    case UnhandledPromptBehavior.AcceptAndNotify:
      await this.acceptDialog();
      throw new UnexpectedAlertOpenError(
        `Accepted user prompt dialog: ${textContent}`
      );

    case UnhandledPromptBehavior.Dismiss:
      await this.dismissDialog();
      break;

    case UnhandledPromptBehavior.DismissAndNotify:
      await this.dismissDialog();
      throw new UnexpectedAlertOpenError(
        `Dismissed user prompt dialog: ${textContent}`
      );

    case UnhandledPromptBehavior.Ignore:
      throw new UnexpectedAlertOpenError(
        "Encountered unhandled user prompt dialog"
      );

    default:
      throw new TypeError(`Unknown unhandledPromptBehavior "${behavior}"`);
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
GeckoDriver.prototype.quit = async function(cmd) {
  const quits = ["eConsiderQuit", "eAttemptQuit", "eForceQuit"];

  let flags = [];
  if (typeof cmd.parameters.flags != "undefined") {
    flags = assert.array(cmd.parameters.flags);
  }

  let quitSeen;
  let mode = 0;
  if (flags.length > 0) {
    for (let k of flags) {
      assert.in(k, Ci.nsIAppStartup);

      if (quits.includes(k)) {
        if (quitSeen) {
          throw new InvalidArgumentError(
            `${k} cannot be combined with ${quitSeen}`
          );
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
  let quitApplication = waitForObserverTopic("quit-application");
  Services.startup.quit(mode);

  return { cause: (await quitApplication).data };
};

GeckoDriver.prototype.installAddon = function(cmd) {
  assert.desktop();

  let path = cmd.parameters.path;
  let temp = cmd.parameters.temporary || false;
  if (
    typeof path == "undefined" ||
    typeof path != "string" ||
    typeof temp != "boolean"
  ) {
    throw new InvalidArgumentError();
  }

  return Addon.install(path, temp);
};

GeckoDriver.prototype.uninstallAddon = function(cmd) {
  assert.firefox();

  let id = cmd.parameters.id;
  if (typeof id == "undefined" || typeof id != "string") {
    throw new InvalidArgumentError();
  }

  return Addon.uninstall(id);
};

/** Receives all messages from content messageManager. */
/* eslint-disable consistent-return */
GeckoDriver.prototype.receiveMessage = function(message) {
  switch (message.name) {
    case "Marionette:switchedToFrame":
      this.currentFrameBrowsingContext = BrowsingContext.get(
        message.json.browsingContextId
      );
      break;

    case "Marionette:Register":
      let { frameId } = message.json;
      this.registerBrowser(frameId, message.target);
      return { frameId };

    case "Marionette:ListenersAttached":
      if (message.json.frameId === this.curBrowser.curFrameId) {
        this.curBrowser.flushPendingCommands();
      }
      break;

    case "Marionette:WebDriver:GetCapabilities":
      return this.capabilities.toJSON();
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
GeckoDriver.prototype.localizeEntity = function(cmd) {
  let { urls, id } = cmd.parameters;

  if (!Array.isArray(urls)) {
    throw new InvalidArgumentError("Value of `urls` should be of type 'Array'");
  }
  if (typeof id != "string") {
    throw new InvalidArgumentError("Value of `id` should be of type 'string'");
  }

  return l10n.localizeEntity(urls, id);
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
GeckoDriver.prototype.localizeProperty = function(cmd) {
  let { urls, id } = cmd.parameters;

  if (!Array.isArray(urls)) {
    throw new InvalidArgumentError("Value of `urls` should be of type 'Array'");
  }
  if (typeof id != "string") {
    throw new InvalidArgumentError("Value of `id` should be of type 'string'");
  }

  return l10n.localizeProperty(urls, id);
};

/**
 * Initialize the reftest mode
 */
GeckoDriver.prototype.setupReftest = async function(cmd) {
  if (this._reftest) {
    throw new UnsupportedOperationError(
      "Called reftest:setup with a reftest session already active"
    );
  }

  if (this.context !== Context.Chrome) {
    throw new UnsupportedOperationError(
      "Must set chrome context before running reftests"
    );
  }

  let {
    urlCount = {},
    screenshot = "unexpected",
    isPrint = false,
  } = cmd.parameters;
  if (!["always", "fail", "unexpected"].includes(screenshot)) {
    throw new InvalidArgumentError(
      "Value of `screenshot` should be 'always', 'fail' or 'unexpected'"
    );
  }

  this._reftest = new reftest.Runner(this);
  this._reftest.setup(urlCount, screenshot, isPrint);
};

/** Run a reftest. */
GeckoDriver.prototype.runReftest = async function(cmd) {
  let {
    test,
    references,
    expected,
    timeout,
    width,
    height,
    pageRanges,
  } = cmd.parameters;

  if (!this._reftest) {
    throw new UnsupportedOperationError(
      "Called reftest:run before reftest:start"
    );
  }

  assert.string(test);
  assert.string(expected);
  assert.array(references);

  return {
    value: await this._reftest.run(
      test,
      references,
      expected,
      timeout,
      pageRanges,
      width,
      height
    ),
  };
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
      "Called reftest:teardown before reftest:start"
    );
  }

  this._reftest.abort();
  this._reftest = null;
};

/**
 * Print page as PDF.
 *
 * @param {boolean=} landscape
 *     Paper orientation. Defaults to false.
 * @param {number=} margin.bottom
 *     Bottom margin in cm. Defaults to 1cm (~0.4 inches).
 * @param {number=} margin.left
 *     Left margin in cm. Defaults to 1cm (~0.4 inches).
 * @param {number=} margin.right
 *     Right margin in cm. Defaults to 1cm (~0.4 inches).
 * @param {number=} margin.top
 *     Top margin in cm. Defaults to 1cm (~0.4 inches).
 * @param {string=} pageRanges (not supported)
 *     Paper ranges to print, e.g., '1-5, 8, 11-13'.
 *     Defaults to the empty string, which means print all pages.
 * @param {number=} page.height
 *     Paper height in cm. Defaults to US letter height (11 inches / 27.94cm)
 * @param {number=} page.width
 *     Paper width in cm. Defaults to US letter width (8.5 inches / 21.59cm)
 * @param {boolean=} shrinkToFit
 *     Whether or not to override page size as defined by CSS.
 *     Defaults to true, in which case the content will be scaled
 *     to fit the paper size.
 * @param {boolean=} printBackground
 *     Print background graphics. Defaults to false.
 * @param {number=} scale
 *     Scale of the webpage rendering. Defaults to 1.
 *
 * @return {string}
 *     Base64 encoded PDF representing printed document
 */
GeckoDriver.prototype.print = async function(cmd) {
  assert.content(this.context);
  assert.open(this.getCurrentWindow());
  await this._handleUserPrompts();

  const settings = print.addDefaultSettings(cmd.parameters);
  for (let prop of ["top", "bottom", "left", "right"]) {
    assert.positiveNumber(
      settings.margin[prop],
      pprint`margin.${prop} is not a positive number`
    );
  }
  for (let prop of ["width", "height"]) {
    assert.positiveNumber(
      settings.page[prop],
      pprint`page.${prop} is not a positive number`
    );
  }
  assert.positiveNumber(
    settings.scale,
    `scale ${settings.scale} is not a positive number`
  );
  assert.that(
    s => s >= print.minScaleValue && settings.scale <= print.maxScaleValue,
    `scale ${settings.scale} is outside the range ${print.minScaleValue}-${print.maxScaleValue}`
  )(settings.scale);
  assert.boolean(settings.shrinkToFit);
  assert.boolean(settings.landscape);
  assert.boolean(settings.printBackground);

  const linkedBrowser = this.curBrowser.tab.linkedBrowser;
  const filePath = await print.printToFile(
    linkedBrowser,
    linkedBrowser.outerWindowID,
    settings
  );

  // return all data as a base64 encoded string
  let bytes;
  const file = await OS.File.open(filePath);
  try {
    bytes = await file.read();
  } finally {
    file.close();
    await OS.File.remove(filePath);
  }

  // Each UCS2 character has an upper byte of 0 and a lower byte matching
  // the binary data
  return {
    value: btoa(String.fromCharCode.apply(null, bytes)),
  };
};

GeckoDriver.prototype.commands = {
  // Marionette service
  "Marionette:AcceptConnections": GeckoDriver.prototype.acceptConnections,
  "Marionette:GetContext": GeckoDriver.prototype.getContext,
  "Marionette:GetScreenOrientation": GeckoDriver.prototype.getScreenOrientation,
  "Marionette:GetWindowType": GeckoDriver.prototype.getWindowType,
  "Marionette:Quit": GeckoDriver.prototype.quit,
  "Marionette:SetContext": GeckoDriver.prototype.setContext,
  "Marionette:SetScreenOrientation": GeckoDriver.prototype.setScreenOrientation,
  "Marionette:ActionChain": GeckoDriver.prototype.actionChain, // bug 1354578, legacy actions
  "Marionette:MultiAction": GeckoDriver.prototype.multiAction, // bug 1354578, legacy actions
  "Marionette:SingleTap": GeckoDriver.prototype.singleTap,

  // Addon service
  "Addon:Install": GeckoDriver.prototype.installAddon,
  "Addon:Uninstall": GeckoDriver.prototype.uninstallAddon,

  // L10n service
  "L10n:LocalizeEntity": GeckoDriver.prototype.localizeEntity,
  "L10n:LocalizeProperty": GeckoDriver.prototype.localizeProperty,

  // Reftest service
  "reftest:setup": GeckoDriver.prototype.setupReftest,
  "reftest:run": GeckoDriver.prototype.runReftest,
  "reftest:teardown": GeckoDriver.prototype.teardownReftest,

  // WebDriver service
  "WebDriver:AcceptAlert": GeckoDriver.prototype.acceptDialog,
  "WebDriver:AcceptDialog": GeckoDriver.prototype.acceptDialog, // deprecated, but used in geckodriver (see also bug 1495063)
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
  "WebDriver:GetAlertText": GeckoDriver.prototype.getTextFromDialog,
  "WebDriver:GetCapabilities": GeckoDriver.prototype.getSessionCapabilities,
  "WebDriver:GetChromeWindowHandle":
    GeckoDriver.prototype.getChromeWindowHandle,
  "WebDriver:GetChromeWindowHandles":
    GeckoDriver.prototype.getChromeWindowHandles,
  "WebDriver:GetCookies": GeckoDriver.prototype.getCookies,
  "WebDriver:GetCurrentChromeWindowHandle":
    GeckoDriver.prototype.getChromeWindowHandle,
  "WebDriver:GetCurrentURL": GeckoDriver.prototype.getCurrentUrl,
  "WebDriver:GetElementAttribute": GeckoDriver.prototype.getElementAttribute,
  "WebDriver:GetElementCSSValue":
    GeckoDriver.prototype.getElementValueOfCssProperty,
  "WebDriver:GetElementProperty": GeckoDriver.prototype.getElementProperty,
  "WebDriver:GetElementRect": GeckoDriver.prototype.getElementRect,
  "WebDriver:GetElementTagName": GeckoDriver.prototype.getElementTagName,
  "WebDriver:GetElementText": GeckoDriver.prototype.getElementText,
  "WebDriver:GetPageSource": GeckoDriver.prototype.getPageSource,
  "WebDriver:GetTimeouts": GeckoDriver.prototype.getTimeouts,
  "WebDriver:GetTitle": GeckoDriver.prototype.getTitle,
  "WebDriver:GetWindowHandle": GeckoDriver.prototype.getWindowHandle,
  "WebDriver:GetWindowHandles": GeckoDriver.prototype.getWindowHandles,
  "WebDriver:GetWindowRect": GeckoDriver.prototype.getWindowRect,
  "WebDriver:IsElementDisplayed": GeckoDriver.prototype.isElementDisplayed,
  "WebDriver:IsElementEnabled": GeckoDriver.prototype.isElementEnabled,
  "WebDriver:IsElementSelected": GeckoDriver.prototype.isElementSelected,
  "WebDriver:MinimizeWindow": GeckoDriver.prototype.minimizeWindow,
  "WebDriver:MaximizeWindow": GeckoDriver.prototype.maximizeWindow,
  "WebDriver:Navigate": GeckoDriver.prototype.navigateTo,
  "WebDriver:NewSession": GeckoDriver.prototype.newSession,
  "WebDriver:NewWindow": GeckoDriver.prototype.newWindow,
  "WebDriver:PerformActions": GeckoDriver.prototype.performActions,
  "WebDriver:Print": GeckoDriver.prototype.print,
  "WebDriver:Refresh": GeckoDriver.prototype.refresh,
  "WebDriver:ReleaseActions": GeckoDriver.prototype.releaseActions,
  "WebDriver:SendAlertText": GeckoDriver.prototype.sendKeysToDialog,
  "WebDriver:SetTimeouts": GeckoDriver.prototype.setTimeouts,
  "WebDriver:SetWindowRect": GeckoDriver.prototype.setWindowRect,
  "WebDriver:SwitchToFrame": GeckoDriver.prototype.switchToFrame,
  "WebDriver:SwitchToParentFrame": GeckoDriver.prototype.switchToParentFrame,
  "WebDriver:SwitchToShadowRoot": GeckoDriver.prototype.switchToShadowRoot,
  "WebDriver:SwitchToWindow": GeckoDriver.prototype.switchToWindow,
  "WebDriver:TakeScreenshot": GeckoDriver.prototype.takeScreenshot,
};

function getWindowId(win) {
  return win.docShell.browsingContext.id;
}

async function exitFullscreen(win) {
  let cb;
  // Use a timed promise to abort if no window manager is present
  await new TimedPromise(
    resolve => {
      cb = new DebounceCallback(resolve);
      win.addEventListener("sizemodechange", cb);
      win.fullScreen = false;
    },
    { throws: null, timeout: TIMEOUT_NO_WINDOW_MANAGER }
  );
  win.removeEventListener("sizemodechange", cb);
}

async function restoreWindow(win) {
  win.restore();
  // Use a poll promise to abort if no window manager is present
  await new PollPromise(
    (resolve, reject) => {
      if (WindowState.from(win.windowState) == WindowState.Normal) {
        resolve();
      } else {
        reject();
      }
    },
    { timeout: TIMEOUT_NO_WINDOW_MANAGER }
  );
}
