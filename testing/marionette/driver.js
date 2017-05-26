/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

var loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
    .getService(Ci.mozIJSSubScriptLoader);

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(
    this, "cookieManager", "@mozilla.org/cookiemanager;1", "nsICookieManager2");

Cu.import("chrome://marionette/content/accessibility.js");
Cu.import("chrome://marionette/content/addon.js");
Cu.import("chrome://marionette/content/assert.js");
Cu.import("chrome://marionette/content/atom.js");
Cu.import("chrome://marionette/content/browser.js");
Cu.import("chrome://marionette/content/capture.js");
Cu.import("chrome://marionette/content/cert.js");
Cu.import("chrome://marionette/content/element.js");
Cu.import("chrome://marionette/content/error.js");
Cu.import("chrome://marionette/content/evaluate.js");
Cu.import("chrome://marionette/content/event.js");
Cu.import("chrome://marionette/content/interaction.js");
Cu.import("chrome://marionette/content/l10n.js");
Cu.import("chrome://marionette/content/legacyaction.js");
Cu.import("chrome://marionette/content/logging.js");
Cu.import("chrome://marionette/content/modal.js");
Cu.import("chrome://marionette/content/proxy.js");
Cu.import("chrome://marionette/content/session.js");
Cu.import("chrome://marionette/content/simpletest.js");
Cu.import("chrome://marionette/content/wait.js");

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

this.Context.fromString = function (s) {
  s = s.toUpperCase();
  if (s in this) {
    return this[s];
  }
  return null;
};

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
this.GeckoDriver = function (appName, server) {
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

  this.importedScripts = new evaluate.ScriptStorageService(
      [Context.CHROME, Context.CONTENT]);
  this.sandboxes = new Sandboxes(() => this.getCurrentWindow());
  this.legacyactions = new legacyaction.Chain();

  this.timer = null;
  this.inactivityTimer = null;

  this.marionetteLog = new logging.ContentLogger();
  this.testName = null;

  this.capabilities = new session.Capabilities();

  this.mm = globalMessageManager;
  this.listener = proxy.toListener(() => this.mm, this.sendAsync.bind(this));

  // points to an alert instance if a modal dialog is present
  this.dialog = null;
  this.dialogHandler = this.globalModalDialogHandler.bind(this);
};

Object.defineProperty(GeckoDriver.prototype, "a11yChecks", {
  get: function () {
    return this.capabilities.get("moz:accessibilityChecks");
  }
});

Object.defineProperty(GeckoDriver.prototype, "proxy", {
  get: function () {
    return this.capabilities.get("proxy");
  }
});

Object.defineProperty(GeckoDriver.prototype, "secureTLS", {
  get: function () {
    return !this.capabilities.get("acceptInsecureCerts");
  }
});

Object.defineProperty(GeckoDriver.prototype, "timeouts", {
  get: function () {
    return this.capabilities.get("timeouts");
  },

  set: function (newTimeouts) {
    this.capabilities.set("timeouts", newTimeouts);
  },
});

Object.defineProperty(GeckoDriver.prototype, "windowHandles", {
  get: function () {
    let hs = [];
    let winEn = Services.wm.getEnumerator(null);

    while (winEn.hasMoreElements()) {
      let win = winEn.getNext();
      let tabBrowser = browser.getTabBrowser(win);

      if (tabBrowser) {
        tabBrowser.tabs.forEach(tab => {
          let winId = this.getIdForBrowser(browser.getBrowserForTab(tab));
          if (winId !== null) {
            hs.push(winId);
          }
        });
      } else {
        // For other chrome windows beside the browser window, only add the window itself.
        hs.push(getOuterWindowId(win));
      }
    }

    return hs;
  },
});

Object.defineProperty(GeckoDriver.prototype, "chromeWindowHandles", {
  get : function () {
    let hs = [];
    let winEn = Services.wm.getEnumerator(null);

    while (winEn.hasMoreElements()) {
      hs.push(getOuterWindowId(winEn.getNext()));
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
GeckoDriver.prototype.globalModalDialogHandler = function (subject, topic) {
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
  if (this.curBrowser && this.curBrowser.frameManager.currentRemoteFrame !== null) {
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
GeckoDriver.prototype.sendAsync = function (name, data, commandID) {
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

GeckoDriver.prototype.broadcastDelayedAsyncMessage_ = function (name, payload) {
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

GeckoDriver.prototype.sendTargettedAsyncMessage_ = function (name, payload) {
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
 * It will return the outer {@ChromeWindow} previously selected by window handle
 * through {@code #switchToWindow}, or the first window that was registered.
 *
 * @param {Context=} forcedContext
 *     Optional name of the context to use for finding the window. It will be required
 *     if a command always needs a specific context, whether which context is
 *     currently set. Defaults to the current context.
 *
 * @return {ChromeWindow}
 *     The current top-level browsing context.
 */
GeckoDriver.prototype.getCurrentWindow = function (forcedContext = undefined) {
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

      } else if (this.curBrowser !== null) {
        if (browser.getTabBrowser(this.curBrowser.window)) {
          // For browser windows we have to check if the current tab still exists.
          if (this.curBrowser.tab && this.curBrowser.contentBrowser) {
            win = this.curBrowser.window;
          }
        } else {
          // For non-browser windows just return the window.
          win = this.curBrowser.window;
        }
      }

      break;
  }

  return win;
};

GeckoDriver.prototype.addFrameCloseListener = function (action) {
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
GeckoDriver.prototype.addBrowser = function (win) {
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
GeckoDriver.prototype.startBrowser = function (win, isNewSession = false) {
  this.mainFrame = win;
  this.curFrame = null;
  this.addBrowser(win);
  this.curBrowser.isNewSession = isNewSession;
  this.curBrowser.startSession(isNewSession, win, this.whenBrowserStarted.bind(this));
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
GeckoDriver.prototype.whenBrowserStarted = function (win, isNewSession) {
  let mm = win.window.messageManager;
  if (mm) {
    if (!isNewSession) {
      // Loading the frame script corresponds to a situation we need to
      // return to the server. If the messageManager is a message broadcaster
      // with no children, we don't have a hope of coming back from this call,
      // so send the ack here. Otherwise, make a note of how many child scripts
      // will be loaded so we known when it's safe to return.
      // Child managers may not have child scripts yet (e.g. socialapi), only
      // count child managers that have children, but only count the top level
      // children as they are the ones that we expect a response from.
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
GeckoDriver.prototype.getVisibleText = function (el, lines) {
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
GeckoDriver.prototype.registerBrowser = function (id, be) {
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

  let reg = {};

  // We want to ignore frames that are XUL browsers that aren't in the "main"
  // tabbrowser, but accept things on Fennec (which doesn't have a
  // xul:tabbrowser), and accept HTML iframes (because tests depend on it),
  // as well as XUL frames. Ideally this should be cleaned up and we should
  // keep track of browsers a different way.
  if (this.appName != "Firefox" || be.namespaceURI != XUL_NS ||
      be.nodeName != "browser" || be.getTabBrowser()) {
    // curBrowser holds all the registered frames in knownFrames
    reg.id = id;
    reg.remotenessChange = this.curBrowser.register(id, be);
  }

  this.wins.set(reg.id, listenerWindow);
  if (nullPrevious && (this.curBrowser.curFrameId !== null)) {
    this.sendAsync(
        "newSession",
        this.capabilities.toJSON(),
        this.newSessionCommandId);
    if (this.curBrowser.isNewSession) {
      this.newSessionCommandId = null;
    }
  }

  return [reg, this.capabilities.toJSON()];
};

GeckoDriver.prototype.registerPromise = function () {
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

GeckoDriver.prototype.listeningPromise = function () {
  const li = "Marionette:listenersAttached";
  return new Promise(resolve => {
    let cb = () => {
      this.mm.removeMessageListener(li, cb);
      resolve();
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
    this.addBrowser(this.getCurrentWindow());
    this.curBrowser.startSession(this.whenBrowserStarted.bind(this));
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
GeckoDriver.prototype.getSessionCapabilities = function (cmd, resp) {
  resp.body.capabilities = this.capabilities;
};

/**
 * Log message.  Accepts user defined log-level.
 *
 * @param {string} value
 *     Log message.
 * @param {string} level
 *     Arbitrary log level.
 */
GeckoDriver.prototype.log = function (cmd, resp) {
  // if level is null, we want to use ContentLogger#send's default
  this.marionetteLog.log(
      cmd.parameters.value,
      cmd.parameters.level || undefined);
};

/** Return all logged messages. */
GeckoDriver.prototype.getLogs = function (cmd, resp) {
  resp.body = this.marionetteLog.get();
};

/**
 * Sets the context of the subsequent commands to be either "chrome" or
 * "content".
 *
 * @param {string} value
 *     Name of the context to be switched to.  Must be one of "chrome" or
 *     "content".
 */
GeckoDriver.prototype.setContext = function (cmd, resp) {
  let val = cmd.parameters.value;
  let ctx = Context.fromString(val);
  if (ctx === null) {
    throw new WebDriverError(`Invalid context: ${val}`);
  }
  this.context = ctx;
};

/** Gets the context of the server, either "chrome" or "content". */
GeckoDriver.prototype.getContext = function (cmd, resp) {
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

GeckoDriver.prototype.execute_ = function (script, args, timeout, opts = {}) {
  switch (this.context) {
    case Context.CONTENT:
      // evaluate in content with lasting side-effects
      if (!opts.sandboxName) {
        return this.listener.execute(script, args, timeout, opts);

      // evaluate in content with sandbox
      } else {
        return this.listener.executeInSandbox(script, args, timeout, opts);
      }

    case Context.CHROME:
      let sb = this.sandboxes.get(opts.sandboxName, opts.newSandbox);
      if (opts.sandboxName) {
        sb = sandbox.augment(sb, new logging.Adapter(this.marionetteLog));
        sb = sandbox.augment(sb, {global: sb});
      }

      opts.timeout = timeout;
      script = this.importedScripts.for(Context.CHROME).concat(script);
      let wargs = evaluate.fromJSON(args, this.curBrowser.seenEls, sb.window);
      let evaluatePromise = evaluate.sandbox(sb, script, wargs, opts);
      return evaluatePromise.then(res => evaluate.toJSON(res, this.curBrowser.seenEls));
  }
};

/**
 * Execute pure JavaScript.  Used to execute simpletest harness tests,
 * which are like mochitests only injected using Marionette.
 *
 * Scripts are expected to call the {@code finish} global when done.
 */
GeckoDriver.prototype.executeJSScript = function* (cmd, resp) {
  let win = assert.window(this.getCurrentWindow());

  let {script, args, scriptTimeout} = cmd.parameters;
  scriptTimeout = scriptTimeout || this.timeouts.script;

  let opts = {
    filename: cmd.parameters.filename,
    line: cmd.parameters.line,
    async: cmd.parameters.async,
  };

  switch (this.context) {
    case Context.CHROME:
      let wargs = evaluate.fromJSON(args, this.curBrowser.seenEls, win);
      let harness = new simpletest.Harness(
          win,
          Context.CHROME,
          this.marionetteLog,
          scriptTimeout,
          function() {},
          this.testName);

      let sb = sandbox.createSimpleTest(win, harness);
      // TODO(ato): Not sure this is needed:
      sb = sandbox.augment(sb, new logging.Adapter(this.marionetteLog));

      let res = yield evaluate.sandbox(sb, script, wargs, opts);
      resp.body.value = evaluate.toJSON(res, this.curBrowser.seenEls);
      break;

    case Context.CONTENT:
      resp.body.value = yield this.listener.executeSimpleTest(script, args, scriptTimeout, opts);
      break;
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

  let get = this.listener.get({url: url, pageTimeout: this.timeouts.pageLoad});

  // If a remoteness update interrupts our page load, this will never return
  // We need to re-issue this request to correctly poll for readyState and
  // send errors.
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
GeckoDriver.prototype.getCurrentUrl = function (cmd) {
  const win = assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  switch (this.context) {
    case Context.CHROME:
      return win.location.href;

    case Context.CONTENT:
      if (this.curBrowser.contentBrowser) {
        return this.curBrowser.contentBrowser.currentURI.spec;
      } else {
        throw new NoSuchWindowError(
          "Not a browser window, or no tab currently selected");
      }
  }
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
  const win = assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  switch (this.context) {
    case Context.CHROME:
      resp.body.value = win.document.documentElement.getAttribute("title");
      break;

    case Context.CONTENT:
      resp.body.value = yield this.listener.getTitle();
      break;
  }
};

/** Gets the current type of the window. */
GeckoDriver.prototype.getWindowType = function (cmd, resp) {
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
  assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  if (!this.curBrowser.tab) {
    // Navigation does not work for non-browser windows
    return;
  }

  if (!this.curBrowser.contentBrowser.webNavigation.canGoBack) {
    return;
  }

  let currentURL = this.getCurrentUrl();
  let goBack = this.listener.goBack({pageTimeout: this.timeouts.pageLoad});

  // If a remoteness update interrupts our page load, this will never return
  // We need to re-issue this request to correctly poll for readyState and
  // send errors.
  this.curBrowser.pendingCommands.push(() => {
    let parameters = {
      // TODO(ato): Bug 1242595
      command_id: this.listener.activeMessageId,
      lastSeenURL: currentURL,
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
  assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  if (!this.curBrowser.tab) {
    // Navigation does not work for non-browser windows
    return;
  }

  if (!this.curBrowser.contentBrowser.webNavigation.canGoForward) {
    return;
  }

  let currentURL = this.getCurrentUrl();
  let goForward = this.listener.goForward({pageTimeout: this.timeouts.pageLoad});

  // If a remoteness update interrupts our page load, this will never return
  // We need to re-issue this request to correctly poll for readyState and
  // send errors.
  this.curBrowser.pendingCommands.push(() => {
    let parameters = {
      // TODO(ato): Bug 1242595
      command_id: this.listener.activeMessageId,
      lastSeenURL: currentURL,
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

  yield this.listener.refresh({pageTimeout: this.timeouts.pageLoad});
};

/**
 * Forces an update for the given browser's id.
 */
GeckoDriver.prototype.updateIdForBrowser = function (browser, newId) {
  this._browserIds.set(browser.permanentKey, newId);
};

/**
 * Retrieves a listener id for the given xul browser element. In case
 * the browser is not known, an attempt is made to retrieve the id from
 * a CPOW, and null is returned if this fails.
 */
GeckoDriver.prototype.getIdForBrowser = function (browser) {
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
 */
GeckoDriver.prototype.getWindowHandle = function (cmd, resp) {
  assert.window(this.getCurrentWindow(Context.CONTENT));

  // curFrameId always holds the current tab.
  if (this.curBrowser.curFrameId) {
    resp.body.value = this.curBrowser.curFrameId.toString();
    return;
  }

  for (let i in this.browsers) {
    if (this.curBrowser == this.browsers[i]) {
      resp.body.value = i.toString();
      return;
    }
  }
};

/**
 * Get a list of top-level browsing contexts. On desktop this typically
 * corresponds to the set of open tabs for browser windows, or the window itself
 * for non-browser chrome windows.
 *
 * Each window handle is assigned by the server and is guaranteed unique,
 * however the return array does not have a specified ordering.
 *
 * @return {Array.<string>}
 *     Unique window handles.
 */
GeckoDriver.prototype.getWindowHandles = function (cmd, resp) {
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
 */
GeckoDriver.prototype.getChromeWindowHandle = function (cmd, resp) {
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
GeckoDriver.prototype.getChromeWindowHandles = function (cmd, resp) {
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
GeckoDriver.prototype.getWindowRect = function (cmd, resp) {
  const win = assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  return {
    x: win.screenX,
    y: win.screenY,
    width: win.outerWidth,
    height: win.outerHeight,
  };
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
GeckoDriver.prototype.setWindowRect = function* (cmd, resp) {
  assert.firefox()
  const win = assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let {x, y, width, height} = cmd.parameters;

  if (win.windowState == win.STATE_FULLSCREEN) {
    win.document.exitFullscreen();
  }

  if (height != null && width != null) {
    assert.positiveInteger(height);
    assert.positiveInteger(width);

    if (win.outerWidth != width && win.outerHeight != height) {
      yield new Promise(resolve => {
        // When the DOM resize event claims that it fires _after_ the document
        // view has been resized, it is lying.
        //
        // Because resize events fire at a high rate, DOM modifications
        // such as updates to outerWidth/outerHeight are not guaranteed to
        // have processed.  To overcome this... abomination... of the web
        // platform, we throttle the event using setTimeout.  If everything
        // was well in this world we would use requestAnimationFrame, but
        // it does not seem to like our particular flavour of XUL.
        const fps15 = 66;
        const synchronousResize = () => win.setTimeout(resolve, fps15);
        win.addEventListener("resize", synchronousResize, {once: true});
        win.resizeTo(width, height);
      });
    }
  }

  if (x != null && y != null) {
    assert.integer(x);
    assert.integer(y);
    const orig = {screenX: win.screenX, screenY: win.screenY};

    win.moveTo(x, y);
    yield wait.until((resolve, reject) => {
      if ((x == win.screenX && y == win.screenY) ||
        (win.screenX != orig.screenX || win.screenY != orig.screenY)) {
        resolve();
      } else {
        reject();
      }
    });
  }

  resp.body = {
    "x": win.screenX,
    "y": win.screenY,
    "width": win.outerWidth,
    "height": win.outerHeight,
  };
};

/**
 * Switch current top-level browsing context by name or server-assigned ID.
 * Searches for windows by name, then ID.  Content windows take precedence.
 *
 * @param {string} name
 *     Target name or ID of the window to switch to.
 * @param {boolean=} focus
 *      A boolean value which determines whether to focus
 *      the window. Defaults to true.
 */
GeckoDriver.prototype.switchToWindow = function* (cmd, resp) {
  let focus = (cmd.parameters.focus !== undefined) ? cmd.parameters.focus : true;

  // Window IDs are internally handled as numbers, but here it could also be the
  // name of the window.
  let switchTo = parseInt(cmd.parameters.name);
  if (isNaN(switchTo)) {
    switchTo = cmd.parameters.name;
  }

  let byNameOrId = function (name, windowId) {
    return switchTo === name || switchTo === windowId;
  };

  let found;
  let winEn = Services.wm.getEnumerator(null);
  while (winEn.hasMoreElements()) {
    let win = winEn.getNext();
    let outerId = getOuterWindowId(win);
    let tabBrowser = browser.getTabBrowser(win);

    if (byNameOrId(win.name, outerId)) {
      // In case the wanted window is a chrome window, we are done.
      found = {win: win, outerId: outerId, hasTabBrowser: !!tabBrowser};
      break;

    } else if (tabBrowser) {
      // Otherwise check if the chrome window has a tab browser, and that it
      // contains a tab with the wanted window handle.
      for (let i = 0; i < tabBrowser.tabs.length; ++i) {
        let contentBrowser = browser.getBrowserForTab(tabBrowser.tabs[i]);
        let contentWindowId = this.getIdForBrowser(contentBrowser);

        if (byNameOrId(win.name, contentWindowId)) {
          found = {
            win: win,
            outerId: outerId,
            hasTabBrowser: true,
            tabIndex: i,
          };
          break;
        }
      }
    }
  }

  if (found) {
    if (!(found.outerId in this.browsers)) {
      // Initialise Marionette if the current chrome window has not been seen
      // before. Also register the initial tab, if one exists.
      let registerBrowsers, browserListening;

      if (found.hasTabBrowser) {
        registerBrowsers = this.registerPromise();
        browserListening = this.listeningPromise();
      }

      this.startBrowser(found.win, false /* isNewSession */);

      if (registerBrowsers && browserListening) {
        yield registerBrowsers;
        yield browserListening;
      }

    } else {
      // Otherwise switch to the known chrome window, and activate the tab
      // if it's a content browser.
      this.curBrowser = this.browsers[found.outerId];

      if ("tabIndex" in found) {
        this.curBrowser.switchToTab(found.tabIndex, found.win, focus);
      }
    }
  } else {
    throw new NoSuchWindowError(`Unable to locate window: ${switchTo}`);
  }
};

GeckoDriver.prototype.getActiveFrame = function (cmd, resp) {
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
      let baseURI = win.document.baseURI;
      if (baseURI.startsWith("about:certerror")) {
        throw new InsecureCertificateError();
      } else if (otherErrorsExpr.exec(win.document.baseURI)) {
        throw new UnknownError("Error loading page");
      }
    }

    checkTimer.initWithCallback(checkLoad.bind(this), 100, Ci.nsITimer.TYPE_ONE_SHOT);
  };

  if (this.context == Context.CHROME) {
    let foundFrame = null;

   // just focus
   if (typeof id == "undefined" && typeof element == "undefined") {
      this.curFrame = null;
      if (focus) {
        this.mainFrame.focus();
      }
      checkTimer.initWithCallback(checkLoad.bind(this), 100, Ci.nsITimer.TYPE_ONE_SHOT);
      return;
    }

    // by element
    if (this.curBrowser.seenEls.has(element)) {
      // HTMLIFrameElement
      let wantedFrame = this.curBrowser.seenEls.get(element, {frame: curWindow});
      // Deal with an embedded xul:browser case
      if (wantedFrame.tagName == "xul:browser" || wantedFrame.tagName == "browser") {
        curWindow = wantedFrame.contentWindow;
        this.curFrame = curWindow;
        if (focus) {
          this.curFrame.focus();
        }
        checkTimer.initWithCallback(checkLoad.bind(this), 100, Ci.nsITimer.TYPE_ONE_SHOT);
        return;
      }

      // Check if the frame is XBL anonymous
      let parent = curWindow.document.getBindingParent(wantedFrame);
      // Shadow nodes also show up in getAnonymousNodes, we should ignore them.
      if (parent && !(parent.shadowRoot && parent.shadowRoot.contains(wantedFrame))) {
        let anonNodes = [...curWindow.document.getAnonymousNodes(parent) || []];
        if (anonNodes.length > 0) {
          let el = wantedFrame;
          while (el) {
            if (anonNodes.indexOf(el) > -1) {
              curWindow = wantedFrame.contentWindow;
              this.curFrame = curWindow;
              if (focus) {
                this.curFrame.focus();
              }
              checkTimer.initWithCallback(checkLoad.bind(this), 100, Ci.nsITimer.TYPE_ONE_SHOT);
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
        if (new XPCNativeWrapper(frames[i]) == new XPCNativeWrapper(wantedFrame)) {
          curWindow = frames[i].contentWindow;
          this.curFrame = curWindow;
          if (focus) {
            this.curFrame.focus();
          }
          checkTimer.initWithCallback(checkLoad.bind(this), 100, Ci.nsITimer.TYPE_ONE_SHOT);
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
          //give precedence to name
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
          curWindow = curWindow.frames[foundFrame].frameElement.contentWindow;
        }
        break;
    }

    if (foundFrame !== null) {
      this.curFrame = curWindow;
      if (focus) {
        this.curFrame.focus();
      }
      checkTimer.initWithCallback(checkLoad.bind(this), 100, Ci.nsITimer.TYPE_ONE_SHOT);
    } else {
      throw new NoSuchFrameError(`Unable to locate frame: ${id}`);
    }

  } else if (this.context == Context.CONTENT) {
    if (!id && !element &&
        this.curBrowser.frameManager.currentRemoteFrame !== null) {
      // We're currently using a ChromeMessageSender for a remote frame, so this
      // request indicates we need to switch back to the top-level (parent) frame.
      // We'll first switch to the parent's (global) ChromeMessageBroadcaster, so
      // we send the message to the right listener.
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

GeckoDriver.prototype.getTimeouts = function (cmd, resp) {
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
GeckoDriver.prototype.setTimeouts = function (cmd, resp) {
  // backwards compatibility with old API
  // that accepted a dictionary {type: <string>, ms: <number>}
  let json = {};
  if (typeof cmd.parameters == "object" &&
      "type" in cmd.parameters &&
      "ms" in cmd.parameters) {
    logger.warn("Using deprecated data structure for setting timeouts");
    json = {[cmd.parameters.type]: parseInt(cmd.parameters.ms)};
  } else {
    json = cmd.parameters;
  }

  // merge with existing timeouts
  let merged = Object.assign(this.timeouts.toJSON(), json);
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
GeckoDriver.prototype.performActions = function (cmd, resp) {
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
GeckoDriver.prototype.releaseActions = function(cmd, resp) {
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
        opts.startNode = this.curBrowser.seenEls.get(opts.startNode, container);
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
        opts.startNode = this.curBrowser.seenEls.get(opts.startNode, container);
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
      // We need to protect against the click causing an OOP frame to close.
      // This fires the mozbrowserclose event when it closes so we need to
      // listen for it and then just send an error back. The person making the
      // call should be aware something isnt right and handle accordingly
      this.addFrameCloseListener("click");

      let click = this.listener.clickElement({id: id, pageTimeout: this.timeouts.pageLoad});

      // If a remoteness update interrupts our page load, this will never return
      // We need to re-issue this request to correctly poll for readyState and
      // send errors.
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
GeckoDriver.prototype.getElementAttribute = function* (cmd, resp) {
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
GeckoDriver.prototype.getElementProperty = function* (cmd, resp) {
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
GeckoDriver.prototype.getElementText = function* (cmd, resp) {
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
GeckoDriver.prototype.getElementTagName = function* (cmd, resp) {
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
GeckoDriver.prototype.isElementDisplayed = function* (cmd, resp) {
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
GeckoDriver.prototype.getElementValueOfCssProperty = function* (cmd, resp) {
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
      resp.body.value = yield this.listener.getElementValueOfCssProperty(id, prop);
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
GeckoDriver.prototype.isElementEnabled = function* (cmd, resp) {
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
GeckoDriver.prototype.isElementSelected = function* (cmd, resp) {
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
GeckoDriver.prototype.getElementRect = function* (cmd, resp) {
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
        height: rect.height
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
GeckoDriver.prototype.sendKeysToElement = function* (cmd, resp) {
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

/** Sets the test name.  The test name is used for logging purposes. */
GeckoDriver.prototype.setTestName = function*(cmd, resp) {
  assert.window(this.getCurrentWindow());

  let val = cmd.parameters.value;
  this.testName = val;
  yield this.listener.setTestName({value: val});
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
GeckoDriver.prototype.clearElement = function* (cmd, resp) {
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
GeckoDriver.prototype.switchToShadowRoot = function* (cmd, resp) {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());

  let id = cmd.parameters.id;
  yield this.listener.switchToShadowRoot(id);
};

/**
 * Add a cookie to the document.
 *
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.addCookie = function* (cmd, resp) {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let cb = msg => {
    this.mm.removeMessageListener("Marionette:addCookie", cb);
    let cookie = msg.json;
    Services.cookies.add(
        cookie.domain,
        cookie.path,
        cookie.name,
        cookie.value,
        cookie.secure,
        cookie.httpOnly,
        cookie.session,
        cookie.expiry,
        {}); // originAttributes
    return true;
  };

  this.mm.addMessageListener("Marionette:addCookie", cb);
  yield this.listener.addCookie(cmd.parameters.cookie);
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
GeckoDriver.prototype.getCookies = function* (cmd, resp) {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  resp.body = yield this.listener.getCookies();
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
GeckoDriver.prototype.deleteAllCookies = function* (cmd, resp) {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let cb = msg => {
    let cookie = msg.json;
    cookieManager.remove(
        cookie.host,
        cookie.name,
        cookie.path,
        false,
        cookie.originAttributes);
    return true;
  };

  this.mm.addMessageListener("Marionette:deleteCookie", cb);
  yield this.listener.deleteAllCookies();
  this.mm.removeMessageListener("Marionette:deleteCookie", cb);
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
GeckoDriver.prototype.deleteCookie = function* (cmd, resp) {
  assert.content(this.context);
  assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let cb = msg => {
    this.mm.removeMessageListener("Marionette:deleteCookie", cb);
    let cookie = msg.json;
    cookieManager.remove(
        cookie.host,
        cookie.name,
        cookie.path,
        false,
        cookie.originAttributes);
    return true;
  };

  this.mm.addMessageListener("Marionette:deleteCookie", cb);
  yield this.listener.deleteCookie(cmd.parameters.name);
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
GeckoDriver.prototype.close = function (cmd, resp) {
  assert.window(this.getCurrentWindow());
  assert.noUserPrompt(this.dialog);

  let nwins = 0;

  let winEn = Services.wm.getEnumerator(null);
  while (winEn.hasMoreElements()) {
    let win = winEn.getNext();

    // For browser windows count the tabs. Otherwise take the window itself.
    let tabbrowser = browser.getTabBrowser(win);
    if (tabbrowser) {
      nwins += tabbrowser.tabs.length;
    } else {
      nwins++;
    }
  }

  // If there is only 1 window left, do not close it. Instead return a faked
  // empty array of window handles. This will instruct geckodriver to terminate
  // the application.
  if (nwins == 1) {
    return [];
  }

  if (this.mm != globalMessageManager) {
    this.mm.removeDelayedFrameScript(FRAME_SCRIPT);
  }

  return this.curBrowser.closeTab().then(() => this.windowHandles.map(String));
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
GeckoDriver.prototype.closeChromeWindow = function (cmd, resp) {
  assert.firefox();
  assert.window(this.getCurrentWindow(Context.CHROME));

  let nwins = 0;

  let winEn = Services.wm.getEnumerator(null);
  while (winEn.hasMoreElements()) {
    nwins++;
    winEn.getNext();
  }

  // If there is only 1 window left, do not close it. Instead return a faked
  // empty array of window handles. This will instruct geckodriver to terminate
  // the application.
  if (nwins == 1) {
    return [];
  }

  // reset frame to the top-most frame
  this.curFrame = null;

  if (this.mm != globalMessageManager) {
    this.mm.removeDelayedFrameScript(FRAME_SCRIPT);
  }

  return this.curBrowser.closeWindow().then(() => this.chromeWindowHandles.map(String));
};

/** Delete Marionette session. */
GeckoDriver.prototype.deleteSession = function (cmd, resp) {
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

    let winEn = Services.wm.getEnumerator(null);
    while (winEn.hasMoreElements()) {
      let win = winEn.getNext();
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

/** Returns the current status of the Application Cache. */
GeckoDriver.prototype.getAppCacheStatus = function* (cmd, resp) {
  assert.window(this.getCurrentWindow());

  switch (this.context) {
    case Context.CHROME:
      throw new UnsupportedOperationError(
          "Command 'getAppCacheStatus' is not yet available in chrome context");

    case Context.CONTENT:
      resp.body.value = yield this.listener.getAppCacheStatus();
      break;
  }
};

/**
 * Import script to the JS evaluation runtime.
 *
 * Imported scripts are exposed in the contexts of all subsequent
 * calls to {@code executeScript}, {@code executeAsyncScript}, and
 * {@code executeJSScript} by prepending them to the evaluated script.
 *
 * Scripts can be cleared with the {@code clearImportedScripts} command.
 *
 * @param {string} script
 *     Script to include.  If the script is byte-by-byte equal to an
 *     existing imported script, it is not imported.
 */
GeckoDriver.prototype.importScript = function*(cmd, resp) {
  let script = cmd.parameters.script;
  this.importedScripts.for(this.context).add(script);
};

/**
 * Clear all scripts that are imported into the JS evaluation runtime.
 *
 * Scripts can be imported using the {@code importScript} command.
 */
GeckoDriver.prototype.clearImportedScripts = function*(cmd, resp) {
  this.importedScripts.for(this.context).clear();
};

/**
 * Takes a screenshot of a web element, current frame, or viewport.
 *
 * The screen capture is returned as a lossless PNG image encoded as
 * a base 64 string.
 *
 * If called in the content context, the <code>id</code> argument is not null
 * and refers to a present and visible web element's ID, the capture area
 * will be limited to the bounding box of that element. Otherwise, the
 * capture area will be the bounding box of the current frame.
 *
 * If called in the chrome context, the screenshot will always represent the
 * entire viewport.
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
 *     If {@code hash} is false, PNG image encoded as base64 encoded string. If
 *     'hash' is True, hex digest of the SHA-256 hash of the base64 encoded
 *     string.
 */
GeckoDriver.prototype.takeScreenshot = function (cmd, resp) {
  let win = assert.window(this.getCurrentWindow());

  let {id, highlights, full, hash, scroll} = cmd.parameters;
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
};

/**
 * Get the current browser orientation.
 *
 * Will return one of the valid primary orientation values
 * portrait-primary, landscape-primary, portrait-secondary, or
 * landscape-secondary.
 */
GeckoDriver.prototype.getScreenOrientation = function (cmd, resp) {
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
GeckoDriver.prototype.setScreenOrientation = function (cmd, resp) {
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
    win.addEventListener("resize", resolve, {once: true});

    if (win.windowState == win.STATE_FULLSCREEN) {
      win.document.exitFullscreen();
    } else {
      win.document.documentElement.requestFullscreen();
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
 * Dismisses a currently displayed tab modal, or returns no such alert if
 * no modal is displayed.
 */
GeckoDriver.prototype.dismissDialog = function (cmd, resp) {
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
GeckoDriver.prototype.acceptDialog = function (cmd, resp) {
  assert.window(this.getCurrentWindow());
  this._checkIfAlertIsPresent();

  let {button0} = this.dialog.ui;
  button0.click();
  this.dialog = null;
};

/**
 * Returns the message shown in a currently displayed modal, or returns a no such
 * alert error if no modal is currently displayed.
 */
GeckoDriver.prototype.getTextFromDialog = function (cmd, resp) {
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
GeckoDriver.prototype.sendKeysToDialog = function (cmd, resp) {
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

GeckoDriver.prototype._checkIfAlertIsPresent = function () {
  if (!this.dialog || !this.dialog.ui) {
    throw new NoAlertOpenError("No modal dialog is currently open");
  }
};

/**
 * Enables or disables accepting new socket connections.
 *
 * By calling this method with `false` the server will not accept any further
 * connections, but existing connections will not be forcible closed. Use `true`
 * to re-enable accepting connections.
 *
 * Please note that when closing the connection via the client you can end-up in
 * a non-recoverable state if it hasn't been enabled before.
 *
 * This method is used for custom in application shutdowns via marionette.quit()
 * or marionette.restart(), like File -> Quit.
 *
 * @param {boolean} state
 *     True if the server should accept new socket connections.
 */
GeckoDriver.prototype.acceptConnections = function (cmd, resp) {
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

GeckoDriver.prototype.installAddon = function (cmd, resp) {
  assert.firefox()

  let path = cmd.parameters.path;
  let temp = cmd.parameters.temporary || false;
  if (typeof path == "undefined" || typeof path != "string" ||
      typeof temp != "boolean") {
    throw InvalidArgumentError();
  }

  return addon.install(path, temp);
};

GeckoDriver.prototype.uninstallAddon = function (cmd, resp) {
  assert.firefox()

  let id = cmd.parameters.id;
  if (typeof id == "undefined" || typeof id != "string") {
    throw new InvalidArgumentError();
  }

  return addon.uninstall(id);
};

/** Receives all messages from content messageManager. */
GeckoDriver.prototype.receiveMessage = function (message) {
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

    case "Marionette:shareData":
      // log messages from tests
      if (message.json.log) {
        this.marionetteLog.addAll(message.json.log);
      }
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

    case "Marionette:getVisibleCookies":
      let [currentPath, host] = message.json;
      let isForCurrentPath = path => currentPath.indexOf(path) != -1;
      let results = [];

      let en = cookieManager.getCookiesFromHost(host, {});
      while (en.hasMoreElements()) {
        let cookie = en.getNext().QueryInterface(Ci.nsICookie2);
        // take the hostname and progressively shorten
        let hostname = host;
        do {
          if ((cookie.host == "." + hostname || cookie.host == hostname) &&
              isForCurrentPath(cookie.path)) {
            results.push({
              "name": cookie.name,
              "value": cookie.value,
              "path": cookie.path,
              "host": cookie.host,
              "secure": cookie.isSecure,
              "expiry": cookie.expires,
              "httpOnly": cookie.isHttpOnly,
              "originAttributes": cookie.originAttributes
            });
            break;
          }
          hostname = hostname.replace(/^.*?\./, "");
        } while (hostname.indexOf(".") != -1);
      }
      return results;

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
        // If remoteness gets updated we need to call newSession. In the case
        // of desktop this just sets up a small amount of state that doesn't
        // change over the course of a session.
        this.sendAsync("newSession", this.capabilities);
        this.curBrowser.flushPendingCommands();
      }
      break;
  }
};

GeckoDriver.prototype.responseCompleted = function () {
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
GeckoDriver.prototype.localizeEntity = function (cmd, resp) {
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
 *     localizeProperty(["chrome://global/locale/findbar.properties"], "FastFind")
 *
 * @param {Array.<string>} urls
 *     Array of .properties URLs.
 * @param {string} id
 *     The ID of the property to retrieve the localized string for.
 *
 * @return {string}
 *     The localized string for the requested property.
 */
GeckoDriver.prototype.localizeProperty = function (cmd, resp) {
  let {urls, id} = cmd.parameters;

  if (!Array.isArray(urls)) {
    throw new InvalidArgumentError("Value of `urls` should be of type 'Array'");
  }
  if (typeof id != "string") {
    throw new InvalidArgumentError("Value of `id` should be of type 'string'");
  }

  resp.body.value = l10n.localizeProperty(urls, id);
}

GeckoDriver.prototype.commands = {
  "getMarionetteID": GeckoDriver.prototype.getMarionetteID,
  "sayHello": GeckoDriver.prototype.sayHello,
  "newSession": GeckoDriver.prototype.newSession,
  "getSessionCapabilities": GeckoDriver.prototype.getSessionCapabilities,
  "log": GeckoDriver.prototype.log,
  "getLogs": GeckoDriver.prototype.getLogs,
  "setContext": GeckoDriver.prototype.setContext,
  "getContext": GeckoDriver.prototype.getContext,
  "executeScript": GeckoDriver.prototype.executeScript,
  "getTimeouts": GeckoDriver.prototype.getTimeouts,
  "timeouts": GeckoDriver.prototype.setTimeouts,  // deprecated until Firefox 55
  "setTimeouts": GeckoDriver.prototype.setTimeouts,
  "singleTap": GeckoDriver.prototype.singleTap,
  "performActions": GeckoDriver.prototype.performActions,
  "releaseActions": GeckoDriver.prototype.releaseActions,
  "actionChain": GeckoDriver.prototype.actionChain, // deprecated
  "multiAction": GeckoDriver.prototype.multiAction, // deprecated
  "executeAsyncScript": GeckoDriver.prototype.executeAsyncScript,
  "executeJSScript": GeckoDriver.prototype.executeJSScript,
  "findElement": GeckoDriver.prototype.findElement,
  "findElements": GeckoDriver.prototype.findElements,
  "clickElement": GeckoDriver.prototype.clickElement,
  "getElementAttribute": GeckoDriver.prototype.getElementAttribute,
  "getElementProperty": GeckoDriver.prototype.getElementProperty,
  "getElementText": GeckoDriver.prototype.getElementText,
  "getElementTagName": GeckoDriver.prototype.getElementTagName,
  "isElementDisplayed": GeckoDriver.prototype.isElementDisplayed,
  "getElementValueOfCssProperty": GeckoDriver.prototype.getElementValueOfCssProperty,
  "getElementRect": GeckoDriver.prototype.getElementRect,
  "isElementEnabled": GeckoDriver.prototype.isElementEnabled,
  "isElementSelected": GeckoDriver.prototype.isElementSelected,
  "sendKeysToElement": GeckoDriver.prototype.sendKeysToElement,
  "clearElement": GeckoDriver.prototype.clearElement,
  "getTitle": GeckoDriver.prototype.getTitle,
  "getWindowType": GeckoDriver.prototype.getWindowType,
  "getPageSource": GeckoDriver.prototype.getPageSource,
  "get": GeckoDriver.prototype.get,
  "getCurrentUrl": GeckoDriver.prototype.getCurrentUrl,
  "goBack": GeckoDriver.prototype.goBack,
  "goForward": GeckoDriver.prototype.goForward,
  "refresh":  GeckoDriver.prototype.refresh,
  "getWindowHandle": GeckoDriver.prototype.getWindowHandle,
  "getChromeWindowHandle": GeckoDriver.prototype.getChromeWindowHandle,
  "getCurrentChromeWindowHandle": GeckoDriver.prototype.getChromeWindowHandle,
  "getWindowHandles": GeckoDriver.prototype.getWindowHandles,
  "getChromeWindowHandles": GeckoDriver.prototype.getChromeWindowHandles,
  "getWindowPosition": GeckoDriver.prototype.getWindowRect, // Redirecting for compatibility
  "setWindowPosition": GeckoDriver.prototype.setWindowRect, // Redirecting for compatibility
  "setWindowRect": GeckoDriver.prototype.setWindowRect,
  "getWindowRect": GeckoDriver.prototype.getWindowRect,
  "getActiveFrame": GeckoDriver.prototype.getActiveFrame,
  "switchToFrame": GeckoDriver.prototype.switchToFrame,
  "switchToParentFrame": GeckoDriver.prototype.switchToParentFrame,
  "switchToWindow": GeckoDriver.prototype.switchToWindow,
  "switchToShadowRoot": GeckoDriver.prototype.switchToShadowRoot,
  "deleteSession": GeckoDriver.prototype.deleteSession,
  "importScript": GeckoDriver.prototype.importScript,
  "clearImportedScripts": GeckoDriver.prototype.clearImportedScripts,
  "getAppCacheStatus": GeckoDriver.prototype.getAppCacheStatus,
  "close": GeckoDriver.prototype.close,
  "closeChromeWindow": GeckoDriver.prototype.closeChromeWindow,
  "setTestName": GeckoDriver.prototype.setTestName,
  "takeScreenshot": GeckoDriver.prototype.takeScreenshot,
  "addCookie": GeckoDriver.prototype.addCookie,
  "getCookies": GeckoDriver.prototype.getCookies,
  "deleteAllCookies": GeckoDriver.prototype.deleteAllCookies,
  "deleteCookie": GeckoDriver.prototype.deleteCookie,
  "getActiveElement": GeckoDriver.prototype.getActiveElement,
  "getScreenOrientation": GeckoDriver.prototype.getScreenOrientation,
  "setScreenOrientation": GeckoDriver.prototype.setScreenOrientation,
  "getWindowSize": GeckoDriver.prototype.getWindowRect, // Redirecting for compatibility
  "setWindowSize": GeckoDriver.prototype.setWindowRect, // Redirecting for compatibility
  "maximizeWindow": GeckoDriver.prototype.maximizeWindow,
  "fullscreen": GeckoDriver.prototype.fullscreen,
  "dismissDialog": GeckoDriver.prototype.dismissDialog,
  "acceptDialog": GeckoDriver.prototype.acceptDialog,
  "getTextFromDialog": GeckoDriver.prototype.getTextFromDialog,
  "sendKeysToDialog": GeckoDriver.prototype.sendKeysToDialog,
  "acceptConnections": GeckoDriver.prototype.acceptConnections,
  "quitApplication": GeckoDriver.prototype.quit,  // deprecated, can be removed in Firefox 56
  "quit": GeckoDriver.prototype.quit,

  "localization:l10n:localizeEntity": GeckoDriver.prototype.localizeEntity,
  "localization:l10n:localizeProperty": GeckoDriver.prototype.localizeProperty,

  "addon:install": GeckoDriver.prototype.installAddon,
  "addon:uninstall": GeckoDriver.prototype.uninstallAddon,
};

function copy (obj) {
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
