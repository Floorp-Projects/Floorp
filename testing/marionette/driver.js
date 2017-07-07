/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/* global XPCNativeWrapper */

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

var loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
    .getService(Ci.mozIJSSubScriptLoader);

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("chrome://marionette/content/accessibility.js");
Cu.import("chrome://marionette/content/addon.js");
Cu.import("chrome://marionette/content/assert.js");
Cu.import("chrome://marionette/content/atom.js");
Cu.import("chrome://marionette/content/browser.js");
Cu.import("chrome://marionette/content/capture.js");
Cu.import("chrome://marionette/content/cert.js");
Cu.import("chrome://marionette/content/cookie.js");
Cu.import("chrome://marionette/content/element.js");
const {
  ElementNotInteractableError,
  error,
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
Cu.import("chrome://marionette/content/event.js");
Cu.import("chrome://marionette/content/interaction.js");
Cu.import("chrome://marionette/content/l10n.js");
Cu.import("chrome://marionette/content/legacyaction.js");
Cu.import("chrome://marionette/content/modal.js");
Cu.import("chrome://marionette/content/proxy.js");
Cu.import("chrome://marionette/content/reftest.js");
Cu.import("chrome://marionette/content/session.js");
Cu.import("chrome://marionette/content/wait.js");

Cu.importGlobalProperties(["URL"]);

this.EXPORTED_SYMBOLS = ["GeckoDriver", "Context"];

var FRAME_SCRIPT = "chrome://marionette/content/listener.js";

const CLICK_TO_START_PREF = "marionette.debugging.clicktostart";
const CONTENT_LISTENER_PREF = "marionette.contentListener";

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

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

// This is used to prevent newSession from returning before the telephony
// API's are ready; see bug 792647.  This assumes that marionette-server.js
// will be loaded before the 'system-message-listener-ready' message
// is fired.  If this stops being true, this approach will have to change.
var systemMessageListenerReady = false;
Services.obs.addObserver(function() {
  systemMessageListenerReady = true;
}, "system-message-listener-ready");

this.Context = {
  CHROME: "chrome",
  CONTENT: "content",
};

this.Context.fromString = function(s) {
  s = s.toUpperCase();
  if (s in this) {
    return this[s];
  }
  return null;
};

/**
* Helper function for converting an nsISimpleEnumerator to
* a javascript iterator
* @param{nsISimpleEnumerator} enumerator
*    enumerator to convert
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
 * Throughout this prototype, functions with the argument {@code cmd}'s
 * documentation refers to the contents of the {@code cmd.parameters}
 * object.
 *
 * @param {string} appName
 *     Description of the product, for example "Firefox".
 * @param {MarionetteServer} server
 *     The instance of Marionette server.
 */
this.GeckoDriver = function(appName, server) {
  this.appName = appName;
  this._server = server;

  this.sessionId = null;
  this.wins = new browser.Windows();
  this.browsers = {};
  // points to current browser
  this.curBrowser = null;
  // topmost chrome frame
  this.mainFrame = null;
  // chrome iframe that currently has focus
  this.curFrame = null;
  this.mozBrowserClose = null;
  this.currentFrameElement = null;
  // frame ID of the current remote frame, used for mozbrowserclose events
  this.oopFrameId = null;
  this.observing = null;
  this._browserIds = new WeakMap();

  // The curent context decides if commands should affect chrome- or
  // content space.
  this.context = Context.CONTENT;

  this.sandboxes = new Sandboxes(() => this.getCurrentWindow());
  this.legacyactions = new legacyaction.Chain();

  this.timer = null;
  this.inactivityTimer = null;

  this.testName = null;

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
      case Context.CHROME:
        let chromeWin = this.getCurrentWindow();
        return new URL(chromeWin.location.href);

      case Context.CONTENT:
        return new URL(this.curBrowser.currentURI.spec);

      default:
        throw TypeError(`Unknown context: ${this.context}`);
    }
  },
});

Object.defineProperty(GeckoDriver.prototype, "title", {
  get() {
    switch (this.context) {
      case Context.CHROME:
        let chromeWin = this.getCurrentWindow();
        return chromeWin.document.documentElement.getAttribute("title");

      case Context.CONTENT:
        return this.curBrowser.currentTitle;

      default:
        throw TypeError(`Unknown context: ${this.context}`);
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
 * Switches to the global ChromeMessageBroadcaster, potentially replacing
 * a frame-specific ChromeMessageSender.  Has no effect if the global
 * ChromeMessageBroadcaster is already in use.  If this replaces a
 * frame-specific ChromeMessageSender, it removes the message listeners
 * from that sender, and then puts the corresponding frame script "to
 * sleep", which removes most of the message listeners from it as well.
 */
GeckoDriver.prototype.switchToGlobalMessageManager = function() {
  if (this.curBrowser &&
      this.curBrowser.frameManager.currentRemoteFrame !== null) {
    this.curBrowser.frameManager.removeMessageManagerListeners(this.mm);
    this.sendAsync("sleepSession");
    this.curBrowser.frameManager.currentRemoteFrame = null;
  }
  this.mm = globalMessageManager;
};

/**
 * Helper method to send async messages to the content listener.
 * Correct usage is to pass in the name of a function in listener.js,
 * a serialisable object, and optionally the current command's ID
 * when not using the modern dispatching technique.
 *
 * @param {string} name
 *     Suffix of the targetted message listener
 *     ({@code Marionette:<suffix>}).
 * @param {Object=} msg
 *     Optional JSON serialisable object to send to the listener.
 * @param {number=} commandID
 *     Optional command ID to ensure synchronisity.
 */
GeckoDriver.prototype.sendAsync = function(name, data, commandID) {
  name = "Marionette:" + name;
  let payload = copy(data);

  // TODO(ato): When proxy.AsyncMessageChannel
  // is used for all chrome <-> content communication
  // this can be removed.
  if (commandID) {
    payload.command_id = commandID;
  }

  if (!this.curBrowser.frameManager.currentRemoteFrame) {
    this.broadcastDelayedAsyncMessage_(name, payload);
  } else {
    this.sendTargettedAsyncMessage_(name, payload);
  }
};

// eslint-disable-next-line
GeckoDriver.prototype.broadcastDelayedAsyncMessage_ = function(name, payload) {
  this.curBrowser.executeWhenReady(() => {
    if (this.curBrowser.curFrameId) {
      const target = name + this.curBrowser.curFrameId;
      this.mm.broadcastAsyncMessage(target, payload);
    } else {
      throw new NoSuchWindowError(
          "No such content frame; perhaps the listener was not registered?");
    }
  });
};

GeckoDriver.prototype.sendTargettedAsyncMessage_ = function(name, payload) {
  const curRemoteFrame = this.curBrowser.frameManager.currentRemoteFrame;
  const target = name + curRemoteFrame.targetFrameId;

  try {
    this.mm.sendAsyncMessage(target, payload);
  } catch (e) {
    switch (e.result) {
      case Cr.NS_ERROR_FAILURE:
      case Cr.NS_ERROR_NOT_INITIALIZED:
        throw new NoSuchWindowError();

      default:
        throw new WebDriverError(e);
    }
  }
};

/**
 * Get the session's current top-level browsing context.
 *
 * It will return the outer {@ChromeWindow} previously selected by window
 * handle through {@code #switchToWindow}, or the first window that was
 * registered.
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
    case Context.CHROME:
      if (this.curFrame !== null) {
        win = this.curFrame;
      } else if (this.curBrowser !== null) {
        win = this.curBrowser.window;
      }
      break;

    case Context.CONTENT:
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
      this.switchToGlobalMessageManager();
      throw new NoSuchWindowError("The window closed during action: " + action);
    }
  };
  win.addEventListener("mozbrowserclose", this.mozBrowserClose, true);
};

/**
 * Create a new browsing context for window and add to known browsers.
 *
 * @param {nsIDOMWindow} win
 *     Window for which we will create a browsing context.
 *
 * @return {string}
 *     Returns the unique server-assigned ID of the window.
 */
GeckoDriver.prototype.addBrowser = function(win) {
  let bc = new browser.Context(win, this);
  let winId = getOuterWindowId(win);

  this.browsers[winId] = bc;
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
 * @param {nsIDOMWindow} win
 *     Window whose browser we need to access.
 * @param {boolean=false} isNewSession
 *     True if this is the first time we're talking to this browser.
 */
GeckoDriver.prototype.startBrowser = function(win, isNewSession = false) {
  this.mainFrame = win;
  this.curFrame = null;
  this.addBrowser(win);
  this.curBrowser.isNewSession = isNewSession;
  this.whenBrowserStarted(win, isNewSession);
};

/**
 * Callback invoked after a new session has been started in a browser.
 * Loads the Marionette frame script into the browser if needed.
 *
 * @param {nsIDOMWindow} win
 *     Window whose browser we need to access.
 * @param {boolean} isNewSession
 *     True if this is the first time we're talking to this browser.
 */
GeckoDriver.prototype.whenBrowserStarted = function(win, isNewSession) {
  let mm = win.window.messageManager;
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
    logger.error(
        `Could not load listener into content for page ${win.location.href}`);
  }
};

/**
 * Recursively get all labeled text.
 *
 * @param {nsIDOMElement} el
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

  // go in here if we're already in a remote frame
  if (this.curBrowser.frameManager.currentRemoteFrame !== null &&
      (!listenerWindow || this.mm == this.curBrowser.frameManager
          .currentRemoteFrame.messageManager.get())) {
    // The outerWindowID from an OOP frame will not be meaningful to
    // the parent process here, since each process maintains its own
    // independent window list.  So, it will either be null (!listenerWindow)
    // if we're already in a remote frame, or it will point to some
    // random window, which will hopefully cause an href mismatch.
    // Currently this only happens in B2G for OOP frames registered in
    // Marionette:switchToFrame, so we'll acknowledge the switchToFrame
    // message here.
    //
    // TODO: Should have a better way of determining that this message
    // is from a remote frame.
    this.curBrowser.frameManager.currentRemoteFrame.targetFrameId = id;
  }

  // We want to ignore frames that are XUL browsers that aren't in the "main"
  // tabbrowser, but accept things on Fennec (which doesn't have a
  // xul:tabbrowser), and accept HTML iframes (because tests depend on it),
  // as well as XUL frames. Ideally this should be cleaned up and we should
  // keep track of browsers a different way.
  if (this.appName != "Firefox" || be.namespaceURI != XUL_NS ||
      be.nodeName != "browser" || be.getTabBrowser()) {
    // curBrowser holds all the registered frames in knownFrames
    this.curBrowser.register(id, be);
  }

  this.wins.set(id, listenerWindow);
  if (nullPrevious && (this.curBrowser.curFrameId !== null)) {
    this.sendAsync(
        "newSession",
        this.capabilities.toJSON(),
        this.newSessionCommandId);
    if (this.curBrowser.isNewSession) {
      this.newSessionCommandId = null;
    }
  }

  return [id, this.capabilities.toJSON()];
};

GeckoDriver.prototype.registerPromise = function() {
  const li = "Marionette:register";

  return new Promise(resolve => {
    let cb = msg => {
      let wid = msg.json.value;
      let be = msg.target;
      let rv = this.registerBrowser(wid, be);

      if (this.curBrowser.frameRegsPending > 0) {
        this.curBrowser.frameRegsPending--;
      }

      if (this.curBrowser.frameRegsPending === 0) {
        this.mm.removeMessageListener(li, cb);
        resolve();
      }

      // this is a sync message and listeners expect the ID back
      return rv;
    };
    this.mm.addMessageListener(li, cb);
  });
};

GeckoDriver.prototype.listeningPromise = function() {
  const li = "Marionette:listenersAttached";

  return new Promise(resolve => {
    let cb = msg => {
      if (msg.json.listenerId === this.curBrowser.curFrameId) {
        this.mm.removeMessageListener(li, cb);
        resolve();
      }
    };
    this.mm.addMessageListener(li, cb);
  });
};

/** Create a new session. */
GeckoDriver.prototype.newSession = function* (cmd, resp) {
  if (this.sessionId) {
    throw new SessionNotCreatedError("Maximum number of active sessions");
  }

  this.sessionId = cmd.parameters.sessionId ||
      cmd.parameters.session_id ||
      element.generateUUID();
  this.newSessionCommandId = cmd.id;

  try {
    this.capabilities = session.Capabilities.fromJSON(
        cmd.parameters.capabilities, {merge: true});
    logger.config("Matched capabilities: " +
        JSON.stringify(this.capabilities));
  } catch (e) {
    throw new SessionNotCreatedError(e);
  }

  if (!this.secureTLS) {
    logger.warn("TLS certificate errors will be ignored for this session");
    let acceptAllCerts = new cert.InsecureSweepingOverride();
    cert.installOverride(acceptAllCerts);
  }

  if (this.proxy.init()) {
    logger.info("Proxy settings initialised: " + JSON.stringify(this.proxy));
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
  } else if (this.appName != "Firefox" && this.curBrowser === null) {
    // if there is a content listener, then we just wake it up
    let win = this.getCurrentWindow();
    this.addBrowser(win);
    this.whenBrowserStarted(win, false);
    this.mm.broadcastAsyncMessage("Marionette:restart", {});
  } else {
    throw new WebDriverError("Session already running");
  }
  this.switchToGlobalMessageManager();

  yield registerBrowsers;
  yield browserListening;

  if (this.curBrowser.tab) {
    this.curBrowser.contentBrowser.focus();
  }

  // Setup global listener for modal dialogs, and check if there is already
  // one open for the currently selected browser window.
  modal.addHandler(this.dialogHandler);
  this.dialog = modal.findModalDialogs(this.curBrowser);

  return {
    sessionId: this.sessionId,
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
 * Sets the context of the subsequent commands to be either "chrome" or
 * "content".
 *
 * @param {string} value
 *     Name of the context to be switched to.  Must be one of "chrome" or
 *     "content".
 */
GeckoDriver.prototype.setContext = function(cmd, resp) {
  let val = cmd.parameters.value;
  let ctx = Context.fromString(val);
  if (ctx === null) {
    throw new WebDriverError(`Invalid context: ${val}`);
  }
  this.context = ctx;
};

/** Gets the context of the server, either "chrome" or "content". */
GeckoDriver.prototype.getContext = function(cmd, resp) {
  resp.body.value = this.context.toString();
};

/**
 * Executes a JavaScript function in the context of the current browsing
 * context, if in content space, or in chrome space otherwise, and returns
 * the return value of the function.
 *
 * It is important to note that if the {@code sandboxName} parameter
 * is left undefined, the script will be evaluated in a mutable sandbox,
 * causing any change it makes on the global state of the document to have
 * lasting side-effects.
 *
 * @param {string} script
 *     Script to evaluate as a function body.
 * @param {Array.<(string|boolean|number|object|WebElement)>} args
 *     Arguments exposed to the script in {@code arguments}.  The array
 *     items must be serialisable to the WebDriver protocol.
 * @param {number} scriptTimeout
 *     Duration in milliseconds of when to interrupt and abort the
 *     script evaluation.
 * @param {string=} sandbox
 *     Name of the sandbox to evaluate the script in.  The sandbox is
 *     cached for later re-use on the same Window object if
 *     {@code newSandbox} is false.  If he parameter is undefined,
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
 *     Attach an {@code onerror} event handler on the Window object.
 *     It does not differentiate content errors from chrome errors.
 * @param {boolean=} directInject
 *     Evaluate the script without wrapping it in a function.
 *
 * @return {(string|boolean|number|object|WebElement)}
 *     Return value from the script, or null which signifies either the
 *     JavaScript notion of null or undefined.
 *
 * @throws ScriptTimeoutError
 *     If the script was interrupted due to reaching the {@code
 *     scriptTimeout} or default timeout.
 * @throws JavaScriptError
 *     If an Error was thrown whilst evaluating the script.
 */
GeckoDriver.prototype.executeScript = function*(cmd, resp) {
  assert.window(this.getCurrentWindow());

  let {script, args, scriptTimeout} = cmd.parameters;
  scriptTimeout = scriptTimeout || this.timeouts.script;

  let opts = {
    sandboxName: cmd.parameters.sandbox,
    newSandbox: !!(typeof cmd.parameters.newSandbox == "undefined") ||
        cmd.parameters.newSandbox,
    filename: cmd.parameters.filename,
    line: cmd.parameters.line,
    debug: cmd.parameters.debug_script,
  };

  resp.body.value = yield this.execute_(script, args, scriptTimeout, opts);
};

/**
 * Executes a JavaScript function in the context of the current browsing
 * context, if in content space, or in chrome space otherwise, and returns
 * the object passed to the callback.
 *
 * The callback is always the last argument to the {@code arguments}
 * list passed to the function scope of the script.  It can be retrieved
 * as such:
 *
 *     let callback = arguments[arguments.length - 1];
 *     callback("foo");
 *     // "foo" is returned
 *
 * It is important to note that if the {@code sandboxName} parameter
 * is left undefined, the script will be evaluated in a mutable sandbox,
 * causing any change it makes on the global state of the document to have
 * lasting side-effects.
 *
 * @param {string} script
 *     Script to evaluate as a function body.
 * @param {Array.<(string|boolean|number|object|WebElement)>} args
 *     Arguments exposed to the script in {@code arguments}.  The array
 *     items must be serialisable to the WebDriver protocol.
 * @param {number} scriptTimeout
 *     Duration in milliseconds of when to interrupt and abort the
 *     script evaluation.
 * @param {string=} sandbox
 *     Name of the sandbox to evaluate the script in.  The sandbox is
 *     cached for later re-use on the same Window object if
 *     {@code newSandbox} is false.  If the parameter is undefined,
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
 *     Attach an {@code onerror} event handler on the Window object.
 *     It does not differentiate content errors from chrome errors.
 * @param {boolean=} directInject
 *     Evaluate the script without wrapping it in a function.
 *
 * @return {(string|boolean|number|object|WebElement)}
 *     Return value from the script, or null which signifies either the
 *     JavaScript notion of null or undefined.
 *
 * @throws ScriptTimeoutError
 *     If the script was interrupted due to reaching the {@code
 *     scriptTimeout} or default timeout.
 * @throws JavaScriptError
 *     If an Error was thrown whilst evaluating the script.
 */
GeckoDriver.prototype.executeAsyncScript = function* (cmd, resp) {
  assert.window(this.getCurrentWindow());

  let {script, args, scriptTimeout} = cmd.parameters;
  scriptTimeout = scriptTimeout || this.timeouts.script;

  let opts = {
    sandboxName: cmd.parameters.sandbox,
    newSandbox: !!(typeof cmd.parameters.newSandbox == "undefined") ||
        cmd.parameters.newSandbox,
    filename: cmd.parameters.filename,
    line: cmd.parameters.line,
    debug: cmd.parameters.debug_script,
    async: true,
  };

  resp.body.value = yield this.execute_(script, args, scriptTimeout, opts);
};

GeckoDriver.prototype.execute_ = function(script, args, timeout, opts = {}) {
  switch (this.context) {
    case Context.CONTENT:
      // evaluate in content with lasting side-effects
      if (!opts.sandboxName) {
        return this.listener.execute(script, args, timeout, opts)
            .then(evaluate.toJSON);
      }

      // evaluate in content with sandbox
      return this.listener.executeInSandbox(script, args, timeout, opts)
          .then(evaluate.toJSON);

    case Context.CHROME:
      let sb = this.sandboxes.get(opts.sandboxName, opts.newSandbox);
      opts.timeout = timeout;
      let wargs = evaluate.fromJSON(args, this.curBrowser.seenEls, sb.window);
      return evaluate.sandbox(sb, script, wargs, opts)
          .then(res => evaluate.toJSON(res, this.curBrowser.seenEls));

    default:
      throw new TypeError(`Unknown context: ${this.context}`);
  }
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
GeckoDriver.prototype.get = function* (cmd, resp) {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let url = cmd.parameters.url;

  let get = this.listener.get({url, pageTimeout: this.timeouts.pageLoad});

  // If a reload of the frame script interrupts our page load, this will
  // never return. We need to re-issue this request to correctly poll for
  // readyState and send errors.
  this.curBrowser.pendingCommands.push(() => {
    let parameters = {
      // TODO(ato): Bug 1242595
      command_id: this.listener.activeMessageId,
      pageTimeout: this.timeouts.pageLoad,
      startTime: new Date().getTime(),
    };
    this.mm.broadcastAsyncMessage(
        "Marionette:waitForPageLoaded" + this.curBrowser.curFrameId,
        parameters);
  });

  yield get;

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
GeckoDriver.prototype.getCurrentUrl = function(cmd) {
  assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

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
GeckoDriver.prototype.getTitle = function* (cmd, resp) {
  assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  return this.title;
};

/** Gets the current type of the window. */
GeckoDriver.prototype.getWindowType = function(cmd, resp) {
  let win = assert.window(this.getCurrentWindow());

  resp.body.value = win.document.documentElement.getAttribute("windowtype");
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
GeckoDriver.prototype.getPageSource = function* (cmd, resp) {
  const win = assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  switch (this.context) {
    case Context.CHROME:
      let s = new win.XMLSerializer();
      resp.body.value = s.serializeToString(win.document);
      break;

    case Context.CONTENT:
      resp.body.value = yield this.listener.getPageSource();
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
GeckoDriver.prototype.goBack = function* (cmd, resp) {
  assert.content(this.context);
  assert.contentBrowser(this.curBrowser);
  assert.noUserPrompt(this.dialog);

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
      command_id: this.listener.activeMessageId,
      lastSeenURL: lastURL.toString(),
      pageTimeout: this.timeouts.pageLoad,
      startTime: new Date().getTime(),
    };
    this.mm.broadcastAsyncMessage(
        "Marionette:waitForPageLoaded" + this.curBrowser.curFrameId,
        parameters);
  });

  yield goBack;
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
GeckoDriver.prototype.goForward = function* (cmd, resp) {
  assert.content(this.context);
  assert.contentBrowser(this.curBrowser);
  assert.noUserPrompt(this.dialog);

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
      command_id: this.listener.activeMessageId,
      lastSeenURL: lastURL.toString(),
      pageTimeout: this.timeouts.pageLoad,
      startTime: new Date().getTime(),
    };
    this.mm.broadcastAsyncMessage(
        "Marionette:waitForPageLoaded" + this.curBrowser.curFrameId,
        parameters);
  });

  yield goForward;
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
GeckoDriver.prototype.refresh = function* (cmd, resp) {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let refresh = this.listener.refresh(
      {pageTimeout: this.timeouts.pageLoad})

  // If a reload of the frame script interrupts our page load, this will
  // never return. We need to re-issue this request to correctly poll for
  // readyState and send errors.
  this.curBrowser.pendingCommands.push(() => {
    let parameters = {
      // TODO(ato): Bug 1242595
      command_id: this.listener.activeMessageId,
      pageTimeout: this.timeouts.pageLoad,
      startTime: new Date().getTime(),
    };
    this.mm.broadcastAsyncMessage(
        "Marionette:waitForPageLoaded" + this.curBrowser.curFrameId,
        parameters);
  });

  yield refresh;
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
GeckoDriver.prototype.getWindowHandle = function(cmd, resp) {
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
GeckoDriver.prototype.getWindowHandles = function(cmd, resp) {
  return this.windowHandles.map(String);
}

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
  assert.window(this.getCurrentWindow(Context.CHROME));

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
GeckoDriver.prototype.getChromeWindowHandles = function(cmd, resp) {
  return this.chromeWindowHandles.map(String);
}

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
GeckoDriver.prototype.getWindowRect = function(cmd, resp) {
  assert.noUserPrompt(this.dialog);
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
GeckoDriver.prototype.setWindowRect = async function(cmd, resp) {
  assert.firefox()
  const win = assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let {x, y, width, height} = cmd.parameters;
  let origRect = this.curBrowser.rect;

  // Throttle resize event by forcing the event queue to flush and delay
  // until the main thread is idle.
  function optimisedResize(resolve) {
    return () => Services.tm.idleDispatchToMainThread(() => {
      win.requestAnimationFrame(resolve);
    });
  }

  // Exit fullscreen and wait for window to resize.
  async function exitFullscreen() {
    return new Promise(resolve => {
      win.addEventListener("sizemodechange", optimisedResize(resolve), {once: true});
      win.fullScreen = false;
    });
  }

  // Synchronous resize to |width| and |height| dimensions.
  async function resizeWindow(width, height) {
    return new Promise(resolve => {
      win.addEventListener("resize", optimisedResize(resolve), {once: true});
      win.resizeTo(width, height);
    });
  }

  // Wait until window size has changed.  We can't wait for the
  // user-requested window size as this may not be achievable on the
  // current system.
  const windowResizeChange = async () => {
    return wait.until((resolve, reject) => {
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
    return wait.until((resolve, reject) => {
      if ((x == win.screenX && y == win.screenY) ||
          (win.screenX != origRect.x || win.screenY != origRect.y)) {
        resolve();
      } else {
        reject();
      }
    });
  }

  if (win.windowState == win.STATE_FULLSCREEN) {
    await exitFullscreen();
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
GeckoDriver.prototype.switchToWindow = function* (cmd, resp) {
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
    yield this.setWindowHandle(found, focus);
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
GeckoDriver.prototype.setWindowHandle = function* (
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
      yield registerBrowsers;
      yield browserListening;
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
    case Context.CHROME:
      // no frame means top-level
      resp.body.value = null;
      if (this.curFrame) {
        let elRef = this.curBrowser.seenEls
            .add(this.curFrame.frameElement);
        let el = element.makeWebElement(elRef);
        resp.body.value = el;
      }
      break;

    case Context.CONTENT:
      resp.body.value = null;
      if (this.currentFrameElement !== null) {
        let el = element.makeWebElement(this.currentFrameElement);
        resp.body.value = el;
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
GeckoDriver.prototype.switchToParentFrame = function* (cmd, resp) {
  assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  yield this.listener.switchToParentFrame();
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
GeckoDriver.prototype.switchToFrame = function* (cmd, resp) {
  assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let {id, element, focus} = cmd.parameters;

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

  if (this.context == Context.CHROME) {
    let foundFrame = null;

    // just focus
    if (typeof id == "undefined" && typeof element == "undefined") {
      this.curFrame = null;
      if (focus) {
        this.mainFrame.focus();
      }
      checkTimer.initWithCallback(
          checkLoad.bind(this), 100, Ci.nsITimer.TYPE_ONE_SHOT);
      return;
    }

    // by element
    if (this.curBrowser.seenEls.has(element)) {
      // HTMLIFrameElement
      let wantedFrame = this.curBrowser.seenEls.get(
          element, {frame: curWindow});
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
      case "string" :
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

  } else if (this.context == Context.CONTENT) {
    if (!id && !element &&
        this.curBrowser.frameManager.currentRemoteFrame !== null) {
      // We're currently using a ChromeMessageSender for a remote frame,
      // so this request indicates we need to switch back to the top-level
      // (parent) frame.  We'll first switch to the parent's (global)
      // ChromeMessageBroadcaster, so we send the message to the right
      // listener.
      this.switchToGlobalMessageManager();
    }
    cmd.command_id = cmd.id;

    let res = yield this.listener.switchToFrame(cmd.parameters);
    if (res) {
      let {win: winId, frame: frameId} = res;
      this.mm = this.curBrowser.frameManager.getFrameMM(winId, frameId);

      let registerBrowsers = this.registerPromise();
      let browserListening = this.listeningPromise();

      this.oopFrameId =
          this.curBrowser.frameManager.switchToFrame(winId, frameId);

      yield registerBrowsers;
      yield browserListening;
    }
  }
};

GeckoDriver.prototype.getTimeouts = function(cmd, resp) {
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
GeckoDriver.prototype.setTimeouts = function(cmd, resp) {
  // merge with existing timeouts
  let merged = Object.assign(this.timeouts.toJSON(), cmd.parameters);
  this.timeouts = session.Timeouts.fromJSON(merged);
};

/** Single tap. */
GeckoDriver.prototype.singleTap = function*(cmd, resp) {
  assert.window(this.getCurrentWindow());

  let {id, x, y} = cmd.parameters;

  switch (this.context) {
    case Context.CHROME:
      throw new UnsupportedOperationError(
          "Command 'singleTap' is not yet available in chrome context");

    case Context.CONTENT:
      this.addFrameCloseListener("tap");
      yield this.listener.singleTap(id, x, y);
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
GeckoDriver.prototype.performActions = function* (cmd, resp) {
  assert.content(this.context,
      "Command 'performActions' is not yet available in chrome context");
  assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let actions = cmd.parameters.actions;
  yield this.listener.performActions({"actions": actions});
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
GeckoDriver.prototype.releaseActions = function*(cmd, resp) {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  yield this.listener.releaseActions();
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
GeckoDriver.prototype.actionChain = function*(cmd, resp) {
  const win = assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let {chain, nextId} = cmd.parameters;

  switch (this.context) {
    case Context.CHROME:
      // be conservative until this has a use case and is established
      // to work as expected in Fennec
      assert.firefox();

      resp.body.value = yield this.legacyactions.dispatchActions(
          chain, nextId, {frame: win}, this.curBrowser.seenEls);
      break;

    case Context.CONTENT:
      this.addFrameCloseListener("action chain");
      resp.body.value = yield this.listener.actionChain(chain, nextId);
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
GeckoDriver.prototype.multiAction = function* (cmd, resp) {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let {value, max_length} = cmd.parameters;

  this.addFrameCloseListener("multi action chain");
  yield this.listener.multiAction(value, max_length);
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
GeckoDriver.prototype.findElement = function* (cmd, resp) {
  const win = assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let strategy = cmd.parameters.using;
  let expr = cmd.parameters.value;
  let opts = {
    startNode: cmd.parameters.element,
    timeout: this.timeouts.implicit,
    all: false,
  };

  switch (this.context) {
    case Context.CHROME:
      if (!SUPPORTED_STRATEGIES.has(strategy)) {
        throw new InvalidSelectorError(`Strategy not supported: ${strategy}`);
      }

      let container = {frame: win};
      if (opts.startNode) {
        opts.startNode = this.curBrowser.seenEls.get(
            opts.startNode, container);
      }
      let el = yield element.find(container, strategy, expr, opts);
      let elRef = this.curBrowser.seenEls.add(el);
      let webEl = element.makeWebElement(elRef);

      resp.body.value = webEl;
      break;

    case Context.CONTENT:
      resp.body.value = yield this.listener.findElementContent(
          strategy,
          expr,
          opts);
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
GeckoDriver.prototype.findElements = function*(cmd, resp) {
  let win = assert.window(this.getCurrentWindow());

  let strategy = cmd.parameters.using;
  let expr = cmd.parameters.value;
  let opts = {
    startNode: cmd.parameters.element,
    timeout: this.timeouts.implicit,
    all: true,
  };

  switch (this.context) {
    case Context.CHROME:
      if (!SUPPORTED_STRATEGIES.has(strategy)) {
        throw new InvalidSelectorError(`Strategy not supported: ${strategy}`);
      }

      let container = {frame: win};
      if (opts.startNode) {
        opts.startNode = this.curBrowser.seenEls.get(
            opts.startNode, container);
      }
      let els = yield element.find(container, strategy, expr, opts);

      let elRefs = this.curBrowser.seenEls.addAll(els);
      let webEls = elRefs.map(element.makeWebElement);
      resp.body = webEls;
      break;

    case Context.CONTENT:
      resp.body = yield this.listener.findElementsContent(
          cmd.parameters.using,
          cmd.parameters.value,
          opts);
      break;
  }
};

/**
 * Return the active element on the page.
 *
 * @return {WebElement}
 *     Active element of the current browsing context's document element.
 *
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getActiveElement = function* (cmd, resp) {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  resp.body.value = yield this.listener.getActiveElement();
};

/**
 * Send click event to element.
 *
 * @param {string} id
 *     Reference ID to the element that will be clicked.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.clickElement = function* (cmd, resp) {
  const win = assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let id = cmd.parameters.id;

  switch (this.context) {
    case Context.CHROME:
      let el = this.curBrowser.seenEls.get(id, {frame: win});
      yield interaction.clickElement(el, this.a11yChecks);
      break;

    case Context.CONTENT:
      // We need to protect against the click causing an OOP frame
      // to close.  This fires the mozbrowserclose event when it closes
      // so we need to listen for it and then just send an error back.
      // The person making the call should be aware something is not right
      // and handle accordingly.
      this.addFrameCloseListener("click");

      let click = this.listener.clickElement(
          {id, pageTimeout: this.timeouts.pageLoad});

      // If a reload of the frame script interrupts our page load, this will
      // never return. We need to re-issue this request to correctly poll for
      // readyState and send errors.
      this.curBrowser.pendingCommands.push(() => {
        let parameters = {
          // TODO(ato): Bug 1242595
          command_id: this.listener.activeMessageId,
          pageTimeout: this.timeouts.pageLoad,
          startTime: new Date().getTime(),
        };
        this.mm.broadcastAsyncMessage(
            "Marionette:waitForPageLoaded" + this.curBrowser.curFrameId,
            parameters);
      });

      yield click;
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
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementAttribute = function*(cmd, resp) {
  const win = assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let {id, name} = cmd.parameters;

  switch (this.context) {
    case Context.CHROME:
      let el = this.curBrowser.seenEls.get(id, {frame: win});
      resp.body.value = el.getAttribute(name);
      break;

    case Context.CONTENT:
      resp.body.value = yield this.listener.getElementAttribute(id, name);
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
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementProperty = function*(cmd, resp) {
  const win = assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let {id, name} = cmd.parameters;

  switch (this.context) {
    case Context.CHROME:
      let el = this.curBrowser.seenEls.get(id, {frame: win});
      resp.body.value = el[name];
      break;

    case Context.CONTENT:
      resp.body.value = yield this.listener.getElementProperty(id, name);
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
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementText = function*(cmd, resp) {
  const win = assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let id = cmd.parameters.id;

  switch (this.context) {
    case Context.CHROME:
      // for chrome, we look at text nodes, and any node with a "label" field
      let el = this.curBrowser.seenEls.get(id, {frame: win});
      let lines = [];
      this.getVisibleText(el, lines);
      resp.body.value = lines.join("\n");
      break;

    case Context.CONTENT:
      resp.body.value = yield this.listener.getElementText(id);
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
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementTagName = function*(cmd, resp) {
  const win = assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let id = cmd.parameters.id;

  switch (this.context) {
    case Context.CHROME:
      let el = this.curBrowser.seenEls.get(id, {frame: win});
      resp.body.value = el.tagName.toLowerCase();
      break;

    case Context.CONTENT:
      resp.body.value = yield this.listener.getElementTagName(id);
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
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.isElementDisplayed = function*(cmd, resp) {
  const win = assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let id = cmd.parameters.id;

  switch (this.context) {
    case Context.CHROME:
      let el = this.curBrowser.seenEls.get(id, {frame: win});
      resp.body.value = yield interaction.isElementDisplayed(
          el, this.a11yChecks);
      break;

    case Context.CONTENT:
      resp.body.value = yield this.listener.isElementDisplayed(id);
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
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementValueOfCssProperty = function*(cmd, resp) {
  const win = assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let {id, propertyName: prop} = cmd.parameters;

  switch (this.context) {
    case Context.CHROME:
      let el = this.curBrowser.seenEls.get(id, {frame: win});
      let sty = win.document.defaultView.getComputedStyle(el);
      resp.body.value = sty.getPropertyValue(prop);
      break;

    case Context.CONTENT:
      resp.body.value = yield this.listener
          .getElementValueOfCssProperty(id, prop);
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
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.isElementEnabled = function*(cmd, resp) {
  const win = assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let id = cmd.parameters.id;

  switch (this.context) {
    case Context.CHROME:
      // Selenium atom doesn't quite work here
      let el = this.curBrowser.seenEls.get(id, {frame: win});
      resp.body.value = yield interaction.isElementEnabled(
          el, this.a11yChecks);
      break;

    case Context.CONTENT:
      resp.body.value = yield this.listener.isElementEnabled(id);
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
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.isElementSelected = function*(cmd, resp) {
  const win = assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let id = cmd.parameters.id;

  switch (this.context) {
    case Context.CHROME:
      // Selenium atom doesn't quite work here
      let el = this.curBrowser.seenEls.get(id, {frame: win});
      resp.body.value = yield interaction.isElementSelected(
          el, this.a11yChecks);
      break;

    case Context.CONTENT:
      resp.body.value = yield this.listener.isElementSelected(id);
      break;
  }
};

/**
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementRect = function*(cmd, resp) {
  const win = assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let id = cmd.parameters.id;

  switch (this.context) {
    case Context.CHROME:
      let el = this.curBrowser.seenEls.get(id, {frame: win});
      let rect = el.getBoundingClientRect();
      resp.body = {
        x: rect.x + win.pageXOffset,
        y: rect.y + win.pageYOffset,
        width: rect.width,
        height: rect.height,
      };
      break;

    case Context.CONTENT:
      resp.body = yield this.listener.getElementRect(id);
      break;
  }
};

/**
 * Send key presses to element after focusing on it.
 *
 * @param {string} id
 *     Reference ID to the element that will be checked.
 * @param {string} value
 *     Value to send to the element.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.sendKeysToElement = function*(cmd, resp) {
  const win = assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let {id, text} = cmd.parameters;
  assert.string(text);

  switch (this.context) {
    case Context.CHROME:
      let el = this.curBrowser.seenEls.get(id, {frame: win});
      yield interaction.sendKeysToElement(
          el, text, true, this.a11yChecks);
      break;

    case Context.CONTENT:
      yield this.listener.sendKeysToElement(id, text);
      break;
  }
};

/**
 * Clear the text of an element.
 *
 * @param {string} id
 *     Reference ID to the element that will be cleared.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.clearElement = function*(cmd, resp) {
  const win = assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let id = cmd.parameters.id;

  switch (this.context) {
    case Context.CHROME:
      // the selenium atom doesn't work here
      let el = this.curBrowser.seenEls.get(id, {frame: win});
      if (el.nodeName == "textbox") {
        el.value = "";
      } else if (el.nodeName == "checkbox") {
        el.checked = false;
      }
      break;

    case Context.CONTENT:
      yield this.listener.clearElement(id);
      break;
  }
};

/**
 * Switch to shadow root of the given host element.
 *
 * @param {string} id element id.
 */
GeckoDriver.prototype.switchToShadowRoot = function*(cmd, resp) {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());

  let id = cmd.parameters.id;
  yield this.listener.switchToShadowRoot(id);
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
 *     If |cookie| is for a different domain than the active document's
 *     host.
 */
GeckoDriver.prototype.addCookie = function(cmd, resp) {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let {protocol, hostname} = this.currentURL;

  const networkSchemes = ["ftp:", "http:", "https:"];
  if (!networkSchemes.includes(protocol)) {
    throw new InvalidCookieDomainError("Document is cookie-averse");
  }

  let newCookie = cookie.fromJSON(cmd.parameters.cookie);
  if (typeof newCookie.domain == "undefined") {
    newCookie.domain = hostname;
  }

  cookie.add(newCookie, {restrictToHost: hostname});
};

/**
 * Get all the cookies for the current domain.
 *
 * This is the equivalent of calling {@code document.cookie} and parsing
 * the result.
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
  assert.noUserPrompt(this.dialog);

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
GeckoDriver.prototype.deleteAllCookies = function(cmd, resp) {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

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
GeckoDriver.prototype.deleteCookie = function(cmd, resp) {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let {hostname, pathname} = this.currentURL;
  let candidateName = assert.string(cmd.parameters.name);
  for (let toDelete of cookie.iter(hostname, pathname)) {
    if (toDelete.name === candidateName) {
      return cookie.remove(toDelete);
    }
  }

  throw UnknownError("Unable to find cookie");
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
GeckoDriver.prototype.close = function(cmd, resp) {
  assert.contentBrowser(this.curBrowser);
  assert.noUserPrompt(this.dialog);

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

  if (this.mm != globalMessageManager) {
    this.mm.removeDelayedFrameScript(FRAME_SCRIPT);
  }

  return this.curBrowser.closeTab()
      .then(() => this.windowHandles.map(String));
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
GeckoDriver.prototype.closeChromeWindow = function(cmd, resp) {
  assert.firefox();
  assert.window(this.getCurrentWindow(Context.CHROME));

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

  if (this.mm != globalMessageManager) {
    this.mm.removeDelayedFrameScript(FRAME_SCRIPT);
  }

  return this.curBrowser.closeWindow()
      .then(() => this.chromeWindowHandles.map(String));
};

/** Delete Marionette session. */
GeckoDriver.prototype.deleteSession = function(cmd, resp) {
  if (this.curBrowser !== null) {
    // frame scripts can be safely reused
    Preferences.set(CONTENT_LISTENER_PREF, false);

    // delete session in each frame in each browser
    for (let win in this.browsers) {
      let browser = this.browsers[win];
      for (let i in browser.knownFrames) {
        globalMessageManager.broadcastAsyncMessage(
            "Marionette:deleteSession" + browser.knownFrames[i], {});
      }
    }

    for (let win of this.windows) {
      if (win.messageManager) {
        win.messageManager.removeDelayedFrameScript(FRAME_SCRIPT);
      } else {
        logger.error(
            `Could not remove listener from page ${win.location.href}`);
      }
    }

    this.curBrowser.frameManager.removeMessageManagerListeners(
        globalMessageManager);
  }

  this.switchToGlobalMessageManager();

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

  this.sessionId = null;
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
 *     considered if {@code id} is not defined. Defaults to true.
 * @param {boolean=} hash
 *     True if the user requests a hash of the image data.
 * @param {boolean=} scroll
 *     Scroll to element if |id| is provided.  If undefined, it will
 *     scroll to the element.
 *
 * @return {string}
 *     If |hash| is false, PNG image encoded as Base64 encoded string.
 *     If |hash| is True, hex digest of the SHA-256 hash of the base64
 *     encoded string.
 */
GeckoDriver.prototype.takeScreenshot = function(cmd, resp) {
  let win = assert.window(this.getCurrentWindow());

  let {id, highlights, full, hash} = cmd.parameters;
  highlights = highlights || [];
  let format = hash ? capture.Format.Hash : capture.Format.Base64;

  switch (this.context) {
    case Context.CHROME:
      let container = {frame: win.document.defaultView};

      let highlightEls = highlights.map(
          ref => this.curBrowser.seenEls.get(ref, container));

      // viewport
      let canvas;
      if (!id && !full) {
        canvas = capture.viewport(container.frame, highlightEls);

      // element or full document element
      } else {
        let node;
        if (id) {
          node = this.curBrowser.seenEls.get(id, container);
        } else {
          node = container.frame.document.documentElement;
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

    case Context.CONTENT:
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
GeckoDriver.prototype.setScreenOrientation = function(cmd, resp) {
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
 * Synchronously maximizes the user agent window as if the user pressed
 * the maximize button, or restores it if it is already maximized.
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
GeckoDriver.prototype.maximizeWindow = function* (cmd, resp) {
  assert.firefox();
  const win = assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  yield new Promise(resolve => {
    win.addEventListener("resize", resolve, {once: true});

    if (win.windowState == win.STATE_MAXIMIZED) {
      win.restore();
    } else {
      win.maximize();
    }
  });

  resp.body = {
    x: win.screenX,
    y: win.screenY,
    width: win.outerWidth,
    height: win.outerHeight,
  };
};

/**
 * Synchronously sets the user agent window to full screen as if the user
 * had done "View > Enter Full Screen", or restores it if it is already
 * in full screen.
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
GeckoDriver.prototype.fullscreen = function* (cmd, resp) {
  assert.firefox();
  const win = assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  yield new Promise(resolve => {
    win.addEventListener("sizemodechange", resolve, {once: true});

    win.fullScreen = !win.fullScreen;
  });

  resp.body = {
    x: win.screenX,
    y: win.screenY,
    width: win.outerWidth,
    height: win.outerHeight,
  };
};

/**
 * Dismisses a currently displayed tab modal, or returns no such alert if
 * no modal is displayed.
 */
GeckoDriver.prototype.dismissDialog = function(cmd, resp) {
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
GeckoDriver.prototype.acceptDialog = function(cmd, resp) {
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
GeckoDriver.prototype.sendKeysToDialog = function(cmd, resp) {
  let win = assert.window(this.getCurrentWindow());
  this._checkIfAlertIsPresent();

  // see toolkit/components/prompts/content/commonDialog.js
  let {loginContainer, loginTextbox} = this.dialog.ui;
  if (loginContainer.hidden) {
    throw new ElementNotInteractableError(
        "This prompt does not accept text input");
  }

  event.sendKeysToElement(
      cmd.parameters.text,
      loginTextbox,
      {ignoreVisibility: true},
      this.dialog.window ? this.dialog.window : win);
};

GeckoDriver.prototype._checkIfAlertIsPresent = function() {
  if (!this.dialog || !this.dialog.ui) {
    throw new NoAlertOpenError("No modal dialog is currently open");
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
GeckoDriver.prototype.acceptConnections = function(cmd, resp) {
  assert.boolean(cmd.parameters.value);
  this._server.acceptConnections = cmd.parameters.value;
}

/**
 * Quits the application with the provided flags.
 *
 * Marionette will stop accepting new connections before ending the
 * current session, and finally attempting to quit the application.
 *
 * Optional {@code nsIAppStartup} flags may be provided as
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
 *     If |flags| contains unknown or incompatible flags, for example
 *     multiple Quit flags.
 */
GeckoDriver.prototype.quit = function* (cmd, resp) {
  const quits = ["eConsiderQuit", "eAttemptQuit", "eForceQuit"];

  let flags = [];
  if (typeof cmd.parameters.flags != "undefined") {
    flags = assert.array(cmd.parameters.flags);
  }

  // bug 1298921
  assert.firefox()

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

  yield quitApplication
      .then(cause => resp.body.cause = cause)
      .then(() => resp.send());
};

GeckoDriver.prototype.installAddon = function(cmd, resp) {
  assert.firefox()

  let path = cmd.parameters.path;
  let temp = cmd.parameters.temporary || false;
  if (typeof path == "undefined" || typeof path != "string" ||
      typeof temp != "boolean") {
    throw InvalidArgumentError();
  }

  return addon.install(path, temp);
};

GeckoDriver.prototype.uninstallAddon = function(cmd, resp) {
  assert.firefox()

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
    case "Marionette:ok":
    case "Marionette:done":
    case "Marionette:error":
      // check if we need to remove the mozbrowserclose listener
      if (this.mozBrowserClose !== null) {
        let win = this.getCurrentWindow();
        win.removeEventListener("mozbrowserclose", this.mozBrowserClose, true);
        this.mozBrowserClose = null;
      }
      break;

    case "Marionette:log":
      // log server-side messages
      logger.info(message.json.message);
      break;

    case "Marionette:switchToModalOrigin":
      this.curBrowser.frameManager.switchToModalOrigin(message);
      this.mm = this.curBrowser.frameManager
          .currentRemoteFrame.messageManager.get();
      break;

    case "Marionette:switchedToFrame":
      if (message.json.restorePrevious) {
        this.currentFrameElement = this.previousFrameElement;
      } else {
        // we don't arbitrarily save previousFrameElement, since
        // we allow frame switching after modals appear, which would
        // override this value and we'd lose our reference
        if (message.json.storePrevious) {
          this.previousFrameElement = this.currentFrameElement;
        }
        this.currentFrameElement = message.json.frameValue;
      }
      break;

    case "Marionette:emitTouchEvent":
      globalMessageManager.broadcastAsyncMessage(
          "MarionetteMainListener:emitTouchEvent", message.json);
      break;

    case "Marionette:register":
      let wid = message.json.value;
      let be = message.target;
      let rv = this.registerBrowser(wid, be);
      return rv;

    case "Marionette:listenersAttached":
      if (message.json.listenerId === this.curBrowser.curFrameId) {
        // If the frame script gets reloaded we need to call newSession.
        // In the case of desktop this just sets up a small amount of state
        // that doesn't change over the course of a session.
        this.sendAsync("newSession", this.capabilities.toJSON());
        this.curBrowser.flushPendingCommands();
      }
      break;
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
 *     localizeEntity(["chrome://global/locale/about.dtd"], "about.version")
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
}

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
}

/**
 * Initialize the reftest mode
 */
GeckoDriver.prototype.setupReftest = function* (cmd, resp) {
  if (this._reftest) {
    throw new UnsupportedOperationError("Called reftest:setup with a reftest session already active");
  }

  if (this.context !== Context.CHROME) {
    throw new UnsupportedOperationError("Must set chrome context before running reftests");
  }

  let {urlCount = {}, screenshot = "unexpected"} = cmd.parameters;
  if (!["always", "fail", "unexpected"].includes(screenshot)) {
    throw new InvalidArgumentError("Value of `screenshot` should be 'always', 'fail' or 'unexpected'");
  }

  this._reftest = new reftest.Runner(this);

  yield this._reftest.setup(urlCount, screenshot);
};


/**
 * Run a reftest
 */
GeckoDriver.prototype.runReftest = function* (cmd, resp) {
  let {test, references, expected, timeout} = cmd.parameters;

  if (!this._reftest) {
    throw new UnsupportedOperationError("Called reftest:run before reftest:start");
  }

  assert.string(test);
  assert.string(expected);
  assert.array(references);

  let result = yield this._reftest.run(test, references, expected, timeout);

  resp.body.value = result;
};

/**
 * End a reftest run
 *
 * Closes the reftest window (without changing the current window handle),
 * and removes cached canvases.
 */
GeckoDriver.prototype.teardownReftest = function* (cmd, resp) {
  if (!this._reftest) {
    throw new UnsupportedOperationError("Called reftest:teardown before reftest:start");
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
  "WebDriver:FullscreenWindow": GeckoDriver.prototype.fullscreen,
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
  "fullscreen": GeckoDriver.prototype.fullscreen,
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

function copy(obj) {
  if (Array.isArray(obj)) {
    return obj.slice();
  } else if (typeof obj == "object") {
    return Object.assign({}, obj);
  }
  return obj;
}

/**
 * Get the outer window ID for the specified window.
 *
 * @param {nsIDOMWindow} win
 *     Window whose browser we need to access.
 *
 * @return {string}
 *     Returns the unique window ID.
 */
function getOuterWindowId(win) {
  return win.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils)
      .outerWindowID;
}
