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

Cu.import("chrome://marionette/content/action.js");
Cu.import("chrome://marionette/content/atom.js");
Cu.import("chrome://marionette/content/browser.js");
Cu.import("chrome://marionette/content/element.js");
Cu.import("chrome://marionette/content/error.js");
Cu.import("chrome://marionette/content/evaluate.js");
Cu.import("chrome://marionette/content/event.js");
Cu.import("chrome://marionette/content/interaction.js");
Cu.import("chrome://marionette/content/logging.js");
Cu.import("chrome://marionette/content/modal.js");
Cu.import("chrome://marionette/content/proxy.js");
Cu.import("chrome://marionette/content/simpletest.js");

this.EXPORTED_SYMBOLS = ["GeckoDriver", "Context"];

var FRAME_SCRIPT = "chrome://marionette/content/listener.js";
const BROWSER_STARTUP_FINISHED = "browser-delayed-startup-finished";
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

// This is used to prevent newSession from returning before the telephony
// API's are ready; see bug 792647.  This assumes that marionette-server.js
// will be loaded before the 'system-message-listener-ready' message
// is fired.  If this stops being true, this approach will have to change.
var systemMessageListenerReady = false;
Services.obs.addObserver(function() {
  systemMessageListenerReady = true;
}, "system-message-listener-ready", false);

// This is used on desktop to prevent newSession from returning before a page
// load initiated by the Firefox command line has completed.
var delayedBrowserStarted = false;
Services.obs.addObserver(function () {
  delayedBrowserStarted = true;
}, BROWSER_STARTUP_FINISHED, false);

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
 * Implements (parts of) the W3C WebDriver protocol.  GeckoDriver lives
 * in chrome space and mediates calls to the message listener of the current
 * browsing context's content frame message listener via ListenerProxy.
 *
 * Throughout this prototype, functions with the argument {@code cmd}'s
 * documentation refers to the contents of the {@code cmd.parameters}
 * object.
 *
 * @param {string} appName
 *     Description of the product, for example "B2G" or "Firefox".
 * @param {string} device
 *     Device this driver should assume.
 * @param {function()} stopSignal
 *     Signal to stop the Marionette server.
 */
this.GeckoDriver = function(appName, device, stopSignal) {
  this.appName = appName;
  this.stopSignal_ = stopSignal;

  this.sessionId = null;
  this.wins = new browser.Windows();
  this.browsers = {};
  // points to current browser
  this.curBrowser = null;
  this.context = Context.CONTENT;
  this.scriptTimeout = null;
  this.searchTimeout = null;
  this.pageTimeout = null;
  this.timer = null;
  this.inactivityTimer = null;
  this.marionetteLog = new logging.ContentLogger();
  // topmost chrome frame
  this.mainFrame = null;
  // chrome iframe that currently has focus
  this.curFrame = null;
  this.mainContentFrameId = null;
  this.importedScripts = new evaluate.ScriptStorageService([Context.CHROME, Context.CONTENT]);
  this.currentFrameElement = null;
  this.testName = null;
  this.mozBrowserClose = null;
  this.sandboxes = new Sandboxes(() => this.getCurrentWindow());
  // frame ID of the current remote frame, used for mozbrowserclose events
  this.oopFrameId = null;
  this.observing = null;
  this._browserIds = new WeakMap();
  this.actions = new action.Chain();

  this.sessionCapabilities = {
    // mandated capabilities
    "browserName": Services.appinfo.name,
    "browserVersion": Services.appinfo.version,
    "platformName": Services.sysinfo.getProperty("name"),
    "platformVersion": Services.sysinfo.getProperty("version"),
    "specificationLevel": 0,

    // supported features
    "raisesAccessibilityExceptions": false,
    "rotatable": this.appName == "B2G",
    "acceptSslCerts": false,
    "takesElementScreenshot": true,
    "takesScreenshot": true,
    "proxy": {},

    // Selenium 2 compat
    "platform": Services.sysinfo.getProperty("name").toUpperCase(),

    // proprietary extensions
    "XULappId" : Services.appinfo.ID,
    "appBuildId" : Services.appinfo.appBuildID,
    "device": device,
    "version": Services.appinfo.version,
  };

  this.mm = globalMessageManager;
  this.listener = proxy.toListener(() => this.mm, this.sendAsync.bind(this));

  // always keep weak reference to current dialogue
  this.dialog = null;
  let handleDialog = (subject, topic) => {
    let winr;
    if (topic == modal.COMMON_DIALOG_LOADED) {
      winr = Cu.getWeakReference(subject);
    }
    this.dialog = new modal.Dialog(() => this.curBrowser, winr);
  };
  modal.addHandler(handleDialog);
};

GeckoDriver.prototype.QueryInterface = XPCOMUtils.generateQI([
  Ci.nsIMessageListener,
  Ci.nsIObserver,
  Ci.nsISupportsWeakReference
]);

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
 * a message object consisting of JSON serialisable primitives,
 * and the current command's ID.
 *
 * @param {string} name
 *     Suffix of the targetted message listener ({@code Marionette:<suffix>}).
 * @param {Object=} msg
 *     JSON serialisable object to send to the listener.
 * @param {number=} cmdId
 *     Command ID to ensure synchronisity.
 */
GeckoDriver.prototype.sendAsync = function(name, msg, cmdId) {
  let curRemoteFrame = this.curBrowser.frameManager.currentRemoteFrame;
  name = "Marionette:" + name;

  // TODO(ato): When proxy.AsyncMessageChannel
  // is used for all chrome <-> content communication
  // this can be removed.
  if (cmdId) {
    msg.command_id = cmdId;
  }

  if (curRemoteFrame === null) {
    this.curBrowser.executeWhenReady(() => {
      if (this.curBrowser.curFrameId) {
        this.mm.broadcastAsyncMessage(name + this.curBrowser.curFrameId, msg);
      } else {
        throw new NoSuchWindowError(
            "No such content frame; perhaps the listener was not registered?");
      }
    });
  } else {
    let remoteFrameId = curRemoteFrame.targetFrameId;
    try {
      this.mm.sendAsyncMessage(name + remoteFrameId, msg);
    } catch (e) {
      switch(e.result) {
        case Cr.NS_ERROR_FAILURE:
        case Cr.NS_ERROR_NOT_INITIALIZED:
          throw new NoSuchWindowError();
        default:
          throw new WebDriverError(e.toString());
      }
    }
  }
};

/**
 * Gets the current active window.
 *
 * @return {nsIDOMWindow}
 */
GeckoDriver.prototype.getCurrentWindow = function() {
  let typ = null;
  if (this.curFrame === null) {
    if (this.curBrowser === null) {
      if (this.context == Context.CONTENT) {
        typ = "navigator:browser";
      }
      return Services.wm.getMostRecentWindow(typ);
    } else {
      return this.curBrowser.window;
    }
  } else {
    return this.curFrame;
  }
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
  let winId = win.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
  winId = winId + ((this.appName == "B2G") ? "-b2g" : "");
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
GeckoDriver.prototype.startBrowser = function(win, isNewSession=false) {
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
GeckoDriver.prototype.whenBrowserStarted = function(win, isNewSession) {
  try {
    let mm = win.window.messageManager;
    if (!isNewSession) {
      // Loading the frame script corresponds to a situation we need to
      // return to the server. If the messageManager is a message broadcaster
      // with no children, we don't have a hope of coming back from this call,
      // so send the ack here. Otherwise, make a note of how many child scripts
      // will be loaded so we known when it's safe to return.
      if (mm.childCount !== 0) {
        this.curBrowser.frameRegsPending = mm.childCount;
      }
    }

    if (!Preferences.get(CONTENT_LISTENER_PREF) || !isNewSession) {
      mm.loadFrameScript(FRAME_SCRIPT, true, true);
      Preferences.set(CONTENT_LISTENER_PREF, true);
    }
  } catch (e) {
    // there may not always be a content process
    logger.error(
        `Could not load listener into content for page ${win.location.href}: ${e}`);
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
    this.curBrowser.frameManager.currentRemoteFrame.targetFrameId =
        this.generateFrameId(id);
  }

  let reg = {};
  // this will be sent to tell the content process if it is the main content
  let mainContent = this.curBrowser.mainContentId === null;
  if (be.getAttribute("type") != "content") {
    // curBrowser holds all the registered frames in knownFrames
    let uid = this.generateFrameId(id);
    reg.id = uid;
    reg.remotenessChange = this.curBrowser.register(uid, be);
  }

  // set to true if we updated mainContentId
  mainContent = mainContent && this.curBrowser.mainContentId !== null;
  if (mainContent) {
    this.mainContentFrameId = this.curBrowser.curFrameId;
  }

  this.wins.set(reg.id, listenerWindow);
  if (nullPrevious && (this.curBrowser.curFrameId !== null)) {
    this.sendAsync("newSession", this.sessionCapabilities, this.newSessionCommandId);
    if (this.curBrowser.isNewSession) {
      this.newSessionCommandId = null;
    }
  }

  return [reg, mainContent, this.sessionCapabilities];
};

GeckoDriver.prototype.registerPromise = function() {
  const li = "Marionette:register";

  return new Promise((resolve) => {
    let cb = (msg) => {
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
  return new Promise((resolve) => {
    let cb = () => {
      this.mm.removeMessageListener(li, cb);
      resolve();
    };
    this.mm.addMessageListener(li, cb);
  });
};

/** Create a new session. */
GeckoDriver.prototype.newSession = function*(cmd, resp) {
  this.sessionId = cmd.parameters.sessionId ||
      cmd.parameters.session_id ||
      element.generateUUID();

  this.newSessionCommandId = cmd.id;
  this.setSessionCapabilities(cmd.parameters.capabilities);
  this.scriptTimeout = 10000;

  let registerBrowsers = this.registerPromise();
  let browserListening = this.listeningPromise();

  let waitForWindow = function() {
    let win = this.getCurrentWindow();
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
      if (clickToStart && (this.appName != "B2G")) {
        let pService = Cc["@mozilla.org/embedcomp/prompt-service;1"]
            .getService(Ci.nsIPromptService);
        pService.alert(win, "", "Click to start execution of marionette tests");
      }
      this.startBrowser(win, true);
    }
  };

  let runSessionStart = function() {
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
  };

  if (!delayedBrowserStarted && this.appName != "B2G") {
    let self = this;
    Services.obs.addObserver(function onStart() {
      Services.obs.removeObserver(onStart, BROWSER_STARTUP_FINISHED);
      runSessionStart.call(self);
    }, BROWSER_STARTUP_FINISHED, false);
  } else {
    runSessionStart.call(this);
  }

  yield registerBrowsers;
  yield browserListening;

  resp.body.sessionId = this.sessionId;
  resp.body.capabilities = this.sessionCapabilities;
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
  resp.body.capabilities = this.sessionCapabilities;
};

/**
 * Update the sessionCapabilities object with the keys that have been
 * passed in when a new session is created.
 *
 * This is not a public API, only available when a new session is
 * created.
 *
 * @param {Object} newCaps
 *     Key/value dictionary to overwrite session's current capabilities.
 */
GeckoDriver.prototype.setSessionCapabilities = function(newCaps) {
  const copy = (from, to={}) => {
    let errors = [];

    // Remove any duplicates between required and desired in favour of the
    // required capabilities
    if (from !== null && from.desiredCapabilities) {
      for (let cap in from.requiredCapabilities) {
        if (from.desiredCapabilities[cap]) {
          delete from.desiredCapabilities[cap];
        }
      }

      // Let's remove the sessionCapabilities from desired capabilities
      for (let cap in this.sessionCapabilities) {
        if (from.desiredCapabilities && from.desiredCapabilities[cap]) {
          delete from.desiredCapabilities[cap];
        }
      }
    }

    for (let key in from) {
      switch (key) {
        case "desiredCapabilities":
          to = copy(from[key], to);
          break;

        case "requiredCapabilities":
          if (from[key].proxy) {
              this.setUpProxy(from[key].proxy);
              to.proxy = from[key].proxy;
              delete from[key].proxy;
          }
          for (let caps in from[key]) {
            if (from[key][caps] !== this.sessionCapabilities[caps]) {
              errors.push(from[key][caps] + " does not equal " +
                  this.sessionCapabilities[caps]);
            }
          }
          break;

        default:
          to[key] = from[key];
      }
    }

    if (Object.keys(errors).length == 0) {
      return to;
    }

    throw new SessionNotCreatedError(
        `Not all requiredCapabilities could be met: ${JSON.stringify(errors)}`);
  };

  // clone, overwrite, and set
  let caps = copy(this.sessionCapabilities);
  caps = copy(newCaps, caps);
  logger.config("Changing capabilities: " + JSON.stringify(caps));
  this.sessionCapabilities = caps;
};

GeckoDriver.prototype.setUpProxy = function(proxy) {
  logger.config("User-provided proxy settings: " + JSON.stringify(proxy));

  if (typeof proxy == "object" && proxy.hasOwnProperty("proxyType")) {
    switch (proxy.proxyType.toUpperCase()) {
      case "MANUAL":
        Preferences.set("network.proxy.type", 1);
        if (proxy.httpProxy && proxy.httpProxyPort){
          Preferences.set("network.proxy.http", proxy.httpProxy);
          Preferences.set("network.proxy.http_port", proxy.httpProxyPort);
        }
        if (proxy.sslProxy && proxy.sslProxyPort){
          Preferences.set("network.proxy.ssl", proxy.sslProxy);
          Preferences.set("network.proxy.ssl_port", proxy.sslProxyPort);
        }
        if (proxy.ftpProxy && proxy.ftpProxyPort) {
          Preferences.set("network.proxy.ftp", proxy.ftpProxy);
          Preferences.set("network.proxy.ftp_port", proxy.ftpProxyPort);
        }
        if (proxy.socksProxy) {
          Preferences.set("network.proxy.socks", proxy.socksProxy);
          Preferences.set("network.proxy.socks_port", proxy.socksProxyPort);
          if (proxy.socksVersion) {
            Preferences.set("network.proxy.socks_version", proxy.socksVersion);
          }
        }
        break;

      case "PAC":
        Preferences.set("network.proxy.type", 2);
        Preferences.set("network.proxy.autoconfig_url", proxy.pacUrl);
        break;

      case "AUTODETECT":
        Preferences.set("network.proxy.type", 4);
        break;

      case "SYSTEM":
        Preferences.set("network.proxy.type", 5);
        break;

      case "NOPROXY":
      default:
        Preferences.set("network.proxy.type", 0);
        break;
    }
  } else {
    throw new InvalidArgumentError("Value of 'proxy' should be an object");
  }
};

/**
 * Log message.  Accepts user defined log-level.
 *
 * @param {string} value
 *     Log message.
 * @param {string} level
 *     Arbitrary log level.
 */
GeckoDriver.prototype.log = function(cmd, resp) {
  // if level is null, we want to use ContentLogger#send's default
  this.marionetteLog.log(
      cmd.parameters.value,
      cmd.parameters.level || undefined);
};

/** Return all logged messages. */
GeckoDriver.prototype.getLogs = function(cmd, resp) {
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
  let {script, args, scriptTimeout} = cmd.parameters;
  scriptTimeout = scriptTimeout || this.scriptTimeout;

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
GeckoDriver.prototype.executeAsyncScript = function(cmd, resp) {
  let {script, args, scriptTimeout} = cmd.parameters;
  scriptTimeout = scriptTimeout || this.scriptTimeout;

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
      let wargs = element.fromJson(args, this.curBrowser.seenEls, sb.window);
      let evaluatePromise = evaluate.sandbox(sb, script, wargs, opts);
      return evaluatePromise.then(res => element.toJson(res, this.curBrowser.seenEls));
  }
};

/**
 * Execute pure JavaScript.  Used to execute simpletest harness tests,
 * which are like mochitests only injected using Marionette.
 *
 * Scripts are expected to call the {@code finish} global when done.
 */
GeckoDriver.prototype.executeJSScript = function(cmd, resp) {
  let {script, args, scriptTimeout} = cmd.parameters;
  scriptTimeout = scriptTimeout || this.scriptTimeout;

  let opts = {
    filename: cmd.parameters.filename,
    line: cmd.parameters.line,
    async: cmd.parameters.async,
  };

  switch (this.context) {
    case Context.CHROME:
      let win = this.getCurrentWindow();
      let wargs = element.fromJson(args, this.curBrowser.seenEls, win);
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
      resp.body.value = element.toJson(res, this.curBrowser.seenEls);
      break;

    case Context.CONTENT:
      resp.body.value = yield this.listener.executeSimpleTest(script, args, scriptTimeout, opts);
      break;
  }
};

/**
 * Set the timeout for asynchronous script execution.
 *
 * @param {number} ms
 *     Time in milliseconds.
 */
GeckoDriver.prototype.setScriptTimeout = function(cmd, resp) {
  let ms = parseInt(cmd.parameters.ms);
  if (isNaN(ms)) {
    throw new WebDriverError("Not a Number");
  }
  this.scriptTimeout = ms;
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
 */
GeckoDriver.prototype.get = function*(cmd, resp) {
  let url = cmd.parameters.url;

  switch (this.context) {
    case Context.CONTENT:
      let get = this.listener.get({url: url, pageTimeout: this.pageTimeout});
      // TODO(ato): Bug 1242595
      let id = this.listener.activeMessageId;

      // If a remoteness update interrupts our page load, this will never return
      // We need to re-issue this request to correctly poll for readyState and
      // send errors.
      this.curBrowser.pendingCommands.push(() => {
        cmd.parameters.command_id = id;
        this.mm.broadcastAsyncMessage(
            "Marionette:pollForReadyState" + this.curBrowser.curFrameId,
            cmd.parameters);
      });

      yield get;
      break;

    case Context.CHROME:
      // At least on desktop, navigating in chrome scope does not
      // correspond to something a user can do, and leaves marionette
      // and the browser in an unusable state. Return a generic error insted.
      // TODO: Error codes need to be refined as a part of bug 1100545 and
      // bug 945729.
      if (this.appName == "Firefox") {
        throw new UnknownError("Cannot navigate in chrome context");
      }

      this.getCurrentWindow().location.href = url;
      yield this.pageLoadPromise();
      break;
  }
};

GeckoDriver.prototype.pageLoadPromise = function() {
  let win = this.getCurrentWindow();
  let timeout = this.pageTimeout;
  let checkTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  let start = new Date().getTime();
  let end = null;

  return new Promise((resolve) => {
    let checkLoad = function() {
      end = new Date().getTime();
      let elapse = end - start;
      if (timeout === null || elapse <= timeout) {
        if (win.document.readyState == "complete") {
          resolve();
        } else {
          checkTimer.initWithCallback(checkLoad, 100, Ci.nsITimer.TYPE_ONE_SHOT);
        }
      } else {
        throw new UnknownError("Error loading page");
      }
    };
    checkTimer.initWithCallback(checkLoad, 100, Ci.nsITimer.TYPE_ONE_SHOT);
  });
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
 */
GeckoDriver.prototype.getCurrentUrl = function(cmd) {
  switch (this.context) {
    case Context.CHROME:
      return this.getCurrentWindow().location.href;

    case Context.CONTENT:
      let isB2G = this.appName == "B2G";
      return this.listener.getCurrentUrl(isB2G);
  }
};

/** Gets the current title of the window. */
GeckoDriver.prototype.getTitle = function*(cmd, resp) {
  switch (this.context) {
    case Context.CHROME:
      let win = this.getCurrentWindow();
      resp.body.value = win.document.documentElement.getAttribute("title");
      break;

    case Context.CONTENT:
      resp.body.value = yield this.listener.getTitle();
      break;
  }
};

/** Gets the current type of the window. */
GeckoDriver.prototype.getWindowType = function(cmd, resp) {
  let win = this.getCurrentWindow();
  resp.body.value = win.document.documentElement.getAttribute("windowtype");
};

/** Gets the page source of the content document. */
GeckoDriver.prototype.getPageSource = function*(cmd, resp) {
  switch (this.context) {
    case Context.CHROME:
      let win = this.getCurrentWindow();
      let s = new win.XMLSerializer();
      resp.body.value = s.serializeToString(win.document);
      break;

    case Context.CONTENT:
      resp.body.value = yield this.listener.getPageSource();
      break;
  }
};

/** Go back in history. */
GeckoDriver.prototype.goBack = function*(cmd, resp) {
  yield this.listener.goBack();
};

/** Go forward in history. */
GeckoDriver.prototype.goForward = function*(cmd, resp) {
  yield this.listener.goForward();
};

/** Refresh the page. */
GeckoDriver.prototype.refresh = function*(cmd, resp) {
  yield this.listener.refresh();
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
 */
GeckoDriver.prototype.getWindowHandle = function(cmd, resp) {
  // curFrameId always holds the current tab.
  if (this.curBrowser.curFrameId && this.appName != "B2G") {
    resp.body.value = this.curBrowser.curFrameId;
    return;
  }

  for (let i in this.browsers) {
    if (this.curBrowser == this.browsers[i]) {
      resp.body.value = i;
      return;
    }
  }
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
GeckoDriver.prototype.getIdForBrowser = function getIdForBrowser(browser) {
  if (browser === null) {
    return null;
  }
  let permKey = browser.permanentKey;
  if (this._browserIds.has(permKey)) {
    return this._browserIds.get(permKey);
  }

  let winId = browser.outerWindowID;
  if (winId) {
    winId += "";
    this._browserIds.set(permKey, winId);
    return winId;
  }
  return null;
},

/**
 * Get a list of top-level browsing contexts.  On desktop this typically
 * corresponds to the set of open tabs.
 *
 * Each window handle is assigned by the server and is guaranteed unique,
 * however the return array does not have a specified ordering.
 *
 * @return {Array.<string>}
 *     Unique window handles.
 */
GeckoDriver.prototype.getWindowHandles = function(cmd, resp) {
  let hs = [];
  let winEn = Services.wm.getEnumerator(null);
  while (winEn.hasMoreElements()) {
    let win = winEn.getNext();
    if (win.gBrowser && this.appName != "B2G") {
      let tabbrowser = win.gBrowser;
      for (let i = 0; i < tabbrowser.browsers.length; ++i) {
        let winId = this.getIdForBrowser(tabbrowser.getBrowserAtIndex(i));
        if (winId !== null) {
          hs.push(winId);
        }
      }
    } else {
      // XUL Windows, at least, do not have gBrowser
      let winId = win.QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIDOMWindowUtils)
          .outerWindowID;
      winId += (this.appName == "B2G") ? "-b2g" : "";
      hs.push(winId);
    }
  }
  resp.body = hs;
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
 */
GeckoDriver.prototype.getChromeWindowHandle = function(cmd, resp) {
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
  let hs = [];
  let winEn = Services.wm.getEnumerator(null);
  while (winEn.hasMoreElements()) {
    let foundWin = winEn.getNext();
    let winId = foundWin.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindowUtils)
        .outerWindowID;
    winId = winId + ((this.appName == "B2G") ? "-b2g" : "");
    hs.push(winId);
  }
  resp.body = hs;
};

/**
 * Get the current window position.
 *
 * @return {Object.<string, number>}
 *     Object with x and y coordinates.
 */
GeckoDriver.prototype.getWindowPosition = function(cmd, resp) {
  let win = this.getCurrentWindow();
  resp.body.x = win.screenX;
  resp.body.y = win.screenY;
};

/**
 * Set the window position of the browser on the OS Window Manager
 *
 * @param {number} x
 *     X coordinate of the top/left of the window that it will be
 *     moved to.
 * @param {number} y
 *     Y coordinate of the top/left of the window that it will be
 *     moved to.
 */
GeckoDriver.prototype.setWindowPosition = function(cmd, resp) {
  if (this.appName != "Firefox") {
    throw new WebDriverError("Unable to set the window position on mobile");
  }

  let x = parseInt(cmd.parameters.x);
  let y  = parseInt(cmd.parameters.y);
  if (isNaN(x) || isNaN(y)) {
    throw new UnknownError("x and y arguments should be integers");
  }

  let win = this.getCurrentWindow();
  win.moveTo(x, y);
};

/**
 * Switch current top-level browsing context by name or server-assigned ID.
 * Searches for windows by name, then ID.  Content windows take precedence.
 *
 * @param {string} name
 *     Target name or ID of the window to switch to.
 */
GeckoDriver.prototype.switchToWindow = function*(cmd, resp) {
  let switchTo = cmd.parameters.name;
  let isB2G = this.appName == "B2G";
  let found;

  let getOuterWindowId = function(win) {
    let rv = win.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindowUtils)
        .outerWindowID;
    rv += isB2G ? "-b2g" : "";
    return rv;
  };

  let byNameOrId = function(name, outerId, contentWindowId) {
    return switchTo == name ||
        switchTo == contentWindowId ||
        switchTo == outerId;
  };

  let winEn = Services.wm.getEnumerator(null);
  while (winEn.hasMoreElements()) {
    let win = winEn.getNext();
    let outerId = getOuterWindowId(win);

    if (win.gBrowser && !isB2G) {
      let tabbrowser = win.gBrowser;
      for (let i = 0; i < tabbrowser.browsers.length; ++i) {
        let browser = tabbrowser.getBrowserAtIndex(i);
        let contentWindowId = this.getIdForBrowser(browser);
        if (byNameOrId(win.name, contentWindowId, outerId)) {
          found = {
            win: win,
            outerId: outerId,
            tabIndex: i,
          };
          break;
        }
      }
    } else {
      if (byNameOrId(win.name, outerId)) {
        found = {win: win, outerId: outerId};
        break;
      }
    }
  }

  if (found) {
    // Initialise Marionette if browser has not been seen before,
    // otherwise switch to known browser and activate the tab if it's a
    // content browser.
    if (!(found.outerId in this.browsers)) {
      let registerBrowsers, browserListening;
      if ("tabIndex" in found) {
        registerBrowsers = this.registerPromise();
        browserListening = this.listeningPromise();
      }

      this.startBrowser(found.win, false /* isNewSession */);

      if (registerBrowsers && browserListening) {
        yield registerBrowsers;
        yield browserListening;
      }
    } else {
      this.curBrowser = this.browsers[found.outerId];

      if ("tabIndex" in found) {
        this.curBrowser.switchToTab(found.tabIndex, found.win);
      }
    }
  } else {
    throw new NoSuchWindowError(`Unable to locate window: ${switchTo}`);
  }
};

GeckoDriver.prototype.getActiveFrame = function(cmd, resp) {
  switch (this.context) {
    case Context.CHROME:
      // no frame means top-level
      resp.body.value = null;
      if (this.curFrame) {
        resp.body.value = this.curBrowser.seenEls
            .add(this.curFrame.frameElement);
      }
      break;

    case Context.CONTENT:
      resp.body.value = this.currentFrameElement;
      break;
  }
};

GeckoDriver.prototype.switchToParentFrame = function*(cmd, resp) {
  let res = yield this.listener.switchToParentFrame();
};

/**
 * Switch to a given frame within the current window.
 *
 * @param {Object} element
 *     A web element reference to the element to switch to.
 * @param {(string|number)} id
 *     If element is not defined, then this holds either the id, name,
 *     or index of the frame to switch to.
 */
GeckoDriver.prototype.switchToFrame = function*(cmd, resp) {
  let {id, element, focus} = cmd.parameters;

  let checkTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  let curWindow = this.getCurrentWindow();

  let checkLoad = function() {
    let errorRegex = /about:.+(error)|(blocked)\?/;
    let curWindow = this.getCurrentWindow();
    if (curWindow.document.readyState == "complete") {
      return;
    } else if (curWindow.document.readyState == "interactive" &&
        errorRegex.exec(curWindow.document.baseURI)) {
      throw new UnknownError("Error loading page");
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

/**
 * Set timeout for searching for elements.
 *
 * @param {number} ms
 *     Search timeout in milliseconds.
 */
GeckoDriver.prototype.setSearchTimeout = function(cmd, resp) {
  let ms = parseInt(cmd.parameters.ms);
  if (isNaN(ms)) {
    throw new WebDriverError("Not a Number");
  }
  this.searchTimeout = ms;
};

/**
 * Set timeout for page loading, searching, and scripts.
 *
 * @param {string} type
 *     Type of timeout.
 * @param {number} ms
 *     Timeout in milliseconds.
 */
GeckoDriver.prototype.timeouts = function(cmd, resp) {
  let typ = cmd.parameters.type;
  let ms = parseInt(cmd.parameters.ms);
  if (isNaN(ms)) {
    throw new WebDriverError("Not a Number");
  }

  switch (typ) {
    case "implicit":
      this.setSearchTimeout(cmd, resp);
      break;

    case "script":
      this.setScriptTimeout(cmd, resp);
      break;

    default:
      this.pageTimeout = ms;
      break;
  }
};

/** Single tap. */
GeckoDriver.prototype.singleTap = function*(cmd, resp) {
  let {id, x, y} = cmd.parameters;

  switch (this.context) {
    case Context.CHROME:
      throw new WebDriverError("Command 'singleTap' is not available in chrome context");

    case Context.CONTENT:
      this.addFrameCloseListener("tap");
      yield this.listener.singleTap(id, x, y);
      break;
  }
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
 */
GeckoDriver.prototype.actionChain = function*(cmd, resp) {
  let {chain, nextId} = cmd.parameters;

  switch (this.context) {
    case Context.CHROME:
      if (this.appName != "Firefox") {
        // be conservative until this has a use case and is established
        // to work as expected on b2g/fennec
        throw new WebDriverError(
            "Command 'actionChain' is not available in chrome context");
      }

      let win = this.getCurrentWindow();
      resp.body.value = yield this.actions.dispatchActions(
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
 */
GeckoDriver.prototype.multiAction = function*(cmd, resp) {
  switch (this.context) {
    case Context.CHROME:
      throw new WebDriverError("Command 'multiAction' is not available in chrome context");

    case Context.CONTENT:
      this.addFrameCloseListener("multi action chain");
      yield this.listener.multiAction(cmd.parameters.value, cmd.parameters.max_length);
      break;
  }
};

/**
 * Find an element using the indicated search strategy.
 *
 * @param {string} using
 *     Indicates which search method to use.
 * @param {string} value
 *     Value the client is looking for.
 */
GeckoDriver.prototype.findElement = function*(cmd, resp) {
  let strategy = cmd.parameters.using;
  let expr = cmd.parameters.value;
  let opts = {
    startNode: cmd.parameters.element,
    timeout: this.searchTimeout,
    all: false,
  };

  switch (this.context) {
    case Context.CHROME:
      if (!SUPPORTED_STRATEGIES.has(strategy)) {
        throw new InvalidSelectorError(`Strategy not supported: ${strategy}`);
      }

      let container = {frame: this.getCurrentWindow()};
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
  let strategy = cmd.parameters.using;
  let expr = cmd.parameters.value;
  let opts = {
    startNode: cmd.parameters.element,
    timeout: this.searchTimeout,
    all: true,
  };

  switch (this.context) {
    case Context.CHROME:
      if (!SUPPORTED_STRATEGIES.has(strategy)) {
        throw new InvalidSelectorError(`Strategy not supported: ${strategy}`);
      }

      let container = {frame: this.getCurrentWindow()};
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

/** Return the active element on the page. */
GeckoDriver.prototype.getActiveElement = function*(cmd, resp) {
  resp.body.value = yield this.listener.getActiveElement();
};

/**
 * Send click event to element.
 *
 * @param {string} id
 *     Reference ID to the element that will be clicked.
 */
GeckoDriver.prototype.clickElement = function*(cmd, resp) {
  let id = cmd.parameters.id;

  switch (this.context) {
    case Context.CHROME:
      let win = this.getCurrentWindow();
      let el = this.curBrowser.seenEls.get(id, {frame: win});
      yield interaction.clickElement(
          el, this.sessionCapabilities.raisesAccessibilityExceptions);
      break;

    case Context.CONTENT:
      // We need to protect against the click causing an OOP frame to close.
      // This fires the mozbrowserclose event when it closes so we need to
      // listen for it and then just send an error back. The person making the
      // call should be aware something isnt right and handle accordingly
      this.addFrameCloseListener("click");
      yield this.listener.clickElement(id);
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
 */
GeckoDriver.prototype.getElementAttribute = function*(cmd, resp) {
  let {id, name} = cmd.parameters;

  switch (this.context) {
    case Context.CHROME:
      let win = this.getCurrentWindow();
      let el = this.curBrowser.seenEls.get(id, {frame: win});
      resp.body.value = atom.getElementAttribute(el, name, this.getCurrentWindow());
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
 */
GeckoDriver.prototype.getElementProperty = function*(cmd, resp) {
  let {id, name} = cmd.parameters;

  switch (this.context) {
    case Context.CHROME:
      throw new UnsupportedOperationError();

    case Context.CONTENT:
      return this.listener.getElementProperty(id, name);
  }
};

/**
 * Get the text of an element, if any.  Includes the text of all child
 * elements.
 *
 * @param {string} id
 *     Reference ID to the element that will be inspected.
 */
GeckoDriver.prototype.getElementText = function*(cmd, resp) {
  let id = cmd.parameters.id;

  switch (this.context) {
    case Context.CHROME:
      // for chrome, we look at text nodes, and any node with a "label" field
      let win = this.getCurrentWindow();
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
 */
GeckoDriver.prototype.getElementTagName = function*(cmd, resp) {
  let id = cmd.parameters.id;

  switch (this.context) {
    case Context.CHROME:
      let win = this.getCurrentWindow();
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
 */
GeckoDriver.prototype.isElementDisplayed = function*(cmd, resp) {
  let id = cmd.parameters.id;

  switch (this.context) {
    case Context.CHROME:
      let win = this.getCurrentWindow();
      let el = this.curBrowser.seenEls.get(id, {frame: win});
      resp.body.value = yield interaction.isElementDisplayed(
          el, this.sessionCapabilities.raisesAccessibilityExceptions);
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
 */
GeckoDriver.prototype.getElementValueOfCssProperty = function*(cmd, resp) {
  let {id, propertyName: prop} = cmd.parameters;

  switch (this.context) {
    case Context.CHROME:
      let win = this.getCurrentWindow();
      let el = this.curBrowser.seenEls.get(id, {frame: win});
      let sty = win.document.defaultView.getComputedStyle(el, null);
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
 */
GeckoDriver.prototype.isElementEnabled = function*(cmd, resp) {
  let id = cmd.parameters.id;

  switch (this.context) {
    case Context.CHROME:
      // Selenium atom doesn't quite work here
      let win = this.getCurrentWindow();
      let el = this.curBrowser.seenEls.get(id, {frame: win});
      resp.body.value = yield interaction.isElementEnabled(
          el, this.sessionCapabilities.raisesAccessibilityExceptions);
      break;

    case Context.CONTENT:
      resp.body.value = yield this.listener.isElementEnabled(id);
      break;
  }
},

/**
 * Check if element is selected.
 *
 * @param {string} id
 *     Reference ID to the element that will be checked.
 */
GeckoDriver.prototype.isElementSelected = function*(cmd, resp) {
  let id = cmd.parameters.id;

  switch (this.context) {
    case Context.CHROME:
      // Selenium atom doesn't quite work here
      let win = this.getCurrentWindow();
      let el = this.curBrowser.seenEls.get(id, {frame: win});
      resp.body.value = yield interaction.isElementSelected(
          el, this.sessionCapabilities.raisesAccessibilityExceptions);
      break;

    case Context.CONTENT:
      resp.body.value = yield this.listener.isElementSelected(id);
      break;
  }
};

GeckoDriver.prototype.getElementRect = function*(cmd, resp) {
  let id = cmd.parameters.id;

  switch (this.context) {
    case Context.CHROME:
      let win = this.getCurrentWindow();
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
 */
GeckoDriver.prototype.sendKeysToElement = function*(cmd, resp) {
  let {id, value} = cmd.parameters;

  if (!value) {
    throw new InvalidArgumentError(`Expected character sequence: ${value}`);
  }

  switch (this.context) {
    case Context.CHROME:
      let win = this.getCurrentWindow();
      let el = this.curBrowser.seenEls.get(id, {frame: win});
      yield interaction.sendKeysToElement(
          el, value, true, this.sessionCapabilities.raisesAccessibilityExceptions);
      break;

    case Context.CONTENT:
      let err;
      let listener = function(msg) {
        this.mm.removeMessageListener("Marionette:setElementValue", listener);

        let val = msg.data.value;
        let el = msg.objects.element;
        let win = this.getCurrentWindow();

        if (el.type == "file") {
          Cu.importGlobalProperties(["File"]);
          let fs = Array.prototype.slice.call(el.files);
          let file;
          try {
            file = new File(val);
          } catch (e) {
            err = new InvalidArgumentError(`File not found: ${val}`);
          }
          fs.push(file);
          el.mozSetFileArray(fs);
        } else {
          el.value = val;
        }
      }.bind(this);

      this.mm.addMessageListener("Marionette:setElementValue", listener);
      yield this.listener.sendKeysToElement({id: id, value: value});
      this.mm.removeMessageListener("Marionette:setElementValue", listener);
      if (err) {
        throw err;
      }
      break;
  }
};

/** Sets the test name.  The test name is used for logging purposes. */
GeckoDriver.prototype.setTestName = function*(cmd, resp) {
  let val = cmd.parameters.value;
  this.testName = val;
  yield this.listener.setTestName({value: val});
};

/**
 * Clear the text of an element.
 *
 * @param {string} id
 *     Reference ID to the element that will be cleared.
 */
GeckoDriver.prototype.clearElement = function*(cmd, resp) {
  let id = cmd.parameters.id;

  switch (this.context) {
    case Context.CHROME:
      // the selenium atom doesn't work here
      let win = this.getCurrentWindow();
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
  let id;
  if (cmd.parameters) { id = cmd.parameters.id; }
  yield this.listener.switchToShadowRoot(id);
};

/** Add a cookie to the document. */
GeckoDriver.prototype.addCookie = function*(cmd, resp) {
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
        cookie.expiry);
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
 */
GeckoDriver.prototype.getCookies = function*(cmd, resp) {
  resp.body = yield this.listener.getCookies();
};

/** Delete all cookies that are visible to a document. */
GeckoDriver.prototype.deleteAllCookies = function*(cmd, resp) {
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

/** Delete a cookie by name. */
GeckoDriver.prototype.deleteCookie = function*(cmd, resp) {
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
 * Close the current window, ending the session if it's the last
 * window currently open.
 *
 * On B2G this method is a noop and will return immediately.
 */
GeckoDriver.prototype.close = function(cmd, resp) {
  // can't close windows on B2G
  if (this.appName == "B2G") {
    return;
  }

  let nwins = 0;
  let winEn = Services.wm.getEnumerator(null);
  while (winEn.hasMoreElements()) {
    let win = winEn.getNext();

    // count both windows and tabs
    if (win.gBrowser) {
      nwins += win.gBrowser.browsers.length;
    } else {
      nwins++;
    }
  }

  // if there is only 1 window left, delete the session
  if (nwins == 1) {
    this.sessionTearDown();
    return;
  }

  try {
    if (this.mm != globalMessageManager) {
      this.mm.removeDelayedFrameScript(FRAME_SCRIPT);
    }

    if (this.curBrowser.tab) {
      this.curBrowser.closeTab();
    } else {
      this.getCurrentWindow().close();
    }
  } catch (e) {
    throw new UnknownError(`Could not close window: ${e.message}`);
  }
};

/**
 * Close the currently selected chrome window, ending the session if it's the last
 * window currently open.
 *
 * On B2G this method is a noop and will return immediately.
 */
GeckoDriver.prototype.closeChromeWindow = function(cmd, resp) {
  // can't close windows on B2G
  if (this.appName == "B2G") {
    return;
  }

  // Get the total number of windows
  let nwins = 0;
  let winEn = Services.wm.getEnumerator(null);
  while (winEn.hasMoreElements()) {
    nwins++;
    winEn.getNext();
  }

  // if there is only 1 window left, delete the session
  if (nwins == 1) {
    this.sessionTearDown();
    return;
  }

  try {
    this.mm.removeDelayedFrameScript(FRAME_SCRIPT);
    this.getCurrentWindow().close();
  } catch (e) {
    throw new UnknownError(`Could not close window: ${e.message}`);
  }
};

/**
 * Deletes the session.
 *
 * If it is a desktop environment, it will close all listeners.
 *
 * If it is a B2G environment, it will make the main content listener
 * sleep, and close all other listeners.  The main content listener
 * persists after disconnect (it's the homescreen), and can safely
 * be reused.
 */
GeckoDriver.prototype.sessionTearDown = function(cmd, resp) {
  if (this.curBrowser !== null) {
    if (this.appName == "B2G") {
      globalMessageManager.broadcastAsyncMessage(
          "Marionette:sleepSession" + this.curBrowser.mainContentId, {});
      this.curBrowser.knownFrames.splice(
          this.curBrowser.knownFrames.indexOf(this.curBrowser.mainContentId), 1);
    } else {
      // don't set this pref for B2G since the framescript can be safely reused
      Preferences.set(CONTENT_LISTENER_PREF, false);
    }

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
      winEn.getNext().messageManager.removeDelayedFrameScript(FRAME_SCRIPT);
    }

    this.curBrowser.frameManager.removeMessageManagerListeners(
        globalMessageManager);
  }

  this.switchToGlobalMessageManager();

  // reset frame to the top-most frame
  this.curFrame = null;
  if (this.mainFrame) {
    this.mainFrame.focus();
  }

  this.sessionId = null;

  if (this.observing !== null) {
    for (let topic in this.observing) {
      Services.obs.removeObserver(this.observing[topic], topic);
    }
    this.observing = null;
  }
  this.sandboxes.clear();
};

/**
 * Processes the "deleteSession" request from the client by tearing down
 * the session and responding "ok".
 */
GeckoDriver.prototype.deleteSession = function(cmd, resp) {
  this.sessionTearDown();
};

/** Returns the current status of the Application Cache. */
GeckoDriver.prototype.getAppCacheStatus = function*(cmd, resp) {
  resp.body.value = yield this.listener.getAppCacheStatus();
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
 * @param {string} id
 *     Reference to a web element.
 * @param {string} highlights
 *     List of web elements to highlight.
 * @param {boolean} hash
 *     True if the user requests a hash of the image data.
 *
 * @return {string}
 *     If {@code hash} is false, PNG image encoded as base64 encoded string. If
 *     'hash' is True, hex digest of the SHA-256 hash of the base64 encoded
 *     string.
 */
GeckoDriver.prototype.takeScreenshot = function(cmd, resp) {
  let {id, highlights, full, hash} = cmd.parameters;
  highlights = highlights || [];

  switch (this.context) {
    case Context.CHROME:
      let win = this.getCurrentWindow();
      let canvas = win.document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
      let doc;
      if (this.appName == "B2G") {
        doc = win.document.body;
      } else {
        doc = win.document.documentElement;
      }
      let docRect = doc.getBoundingClientRect();
      let width = docRect.width;
      let height = docRect.height;

      // Convert width and height from CSS pixels (potentially fractional)
      // to device pixels (integer).
      let scale = win.devicePixelRatio;
      canvas.setAttribute("width", Math.round(width * scale));
      canvas.setAttribute("height", Math.round(height * scale));

      let context = canvas.getContext("2d");
      let flags;
      if (this.appName == "B2G") {
        flags =
          context.DRAWWINDOW_DRAW_CARET |
          context.DRAWWINDOW_DRAW_VIEW |
          context.DRAWWINDOW_USE_WIDGET_LAYERS;
      } else {
        // Bug 1075168: CanvasRenderingContext2D image is distorted
        // when using certain flags in chrome context.
        flags =
          context.DRAWWINDOW_DRAW_VIEW |
          context.DRAWWINDOW_USE_WIDGET_LAYERS;
      }
      context.scale(scale, scale);
      context.drawWindow(win, 0, 0, width, height, "rgb(255,255,255)", flags);
      let dataUrl = canvas.toDataURL("image/png", "");
      let data = dataUrl.substring(dataUrl.indexOf(",") + 1);
      resp.body.value = data;
      break;

    case Context.CONTENT:
      if (hash) {
        return this.listener.getScreenshotHash(id, full, highlights);
      } else {
        return this.listener.takeScreenshot(id, full, highlights);
      }
  }
};

/**
 * Get the current browser orientation.
 *
 * Will return one of the valid primary orientation values
 * portrait-primary, landscape-primary, portrait-secondary, or
 * landscape-secondary.
 */
GeckoDriver.prototype.getScreenOrientation = function(cmd, resp) {
  if (this.appName == "Firefox") {
    throw new UnsupportedOperationError();
  }
  resp.body.value = this.getCurrentWindow().screen.mozOrientation;
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
  if (this.appName == "Firefox") {
    throw new UnsupportedOperationError();
  }

  const ors = [
    "portrait", "landscape",
    "portrait-primary", "landscape-primary",
    "portrait-secondary", "landscape-secondary"
  ];

  let or = String(cmd.parameters.orientation);
  let mozOr = or.toLowerCase();
  if (ors.indexOf(mozOr) < 0) {
    throw new InvalidArgumentError(`Unknown screen orientation: ${or}`);
  }

  let win = this.getCurrentWindow();
  if (!win.screen.mozLockOrientation(mozOr)) {
    throw new WebDriverError(`Unable to set screen orientation: ${or}`);
  }
};

/**
 * Get the size of the browser window currently in focus.
 *
 * Will return the current browser window size in pixels. Refers to
 * window outerWidth and outerHeight values, which include scroll bars,
 * title bars, etc.
 */
GeckoDriver.prototype.getWindowSize = function(cmd, resp) {
  let win = this.getCurrentWindow();
  resp.body.width = win.outerWidth;
  resp.body.height = win.outerHeight;
};

/**
 * Set the size of the browser window currently in focus.
 *
 * Not supported on B2G. The supplied width and height values refer to
 * the window outerWidth and outerHeight values, which include scroll
 * bars, title bars, etc.
 *
 * An error will be returned if the requested window size would result
 * in the window being in the maximized state.
 */
GeckoDriver.prototype.setWindowSize = function(cmd, resp) {
  if (this.appName != "Firefox") {
    throw new UnsupportedOperationError();
  }

  let width = parseInt(cmd.parameters.width);
  let height = parseInt(cmd.parameters.height);

  let win = this.getCurrentWindow();
  if (width >= win.screen.availWidth || height >= win.screen.availHeight) {
    throw new UnsupportedOperationError("Requested size exceeds screen size")
  }

  win.resizeTo(width, height);
};

/**
 * Maximizes the user agent window as if the user pressed the maximise
 * button.
 *
 * Not Supported on B2G or Fennec.
 */
GeckoDriver.prototype.maximizeWindow = function(cmd, resp) {
  if (this.appName != "Firefox") {
    throw new UnsupportedOperationError();
  }

  let win = this.getCurrentWindow();
  win.maximize()
};

/**
 * Dismisses a currently displayed tab modal, or returns no such alert if
 * no modal is displayed.
 */
GeckoDriver.prototype.dismissDialog = function(cmd, resp) {
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
  this._checkIfAlertIsPresent();

  let {button0} = this.dialog.ui;
  button0.click();
  this.dialog = null;
};

/**
 * Returns the message shown in a currently displayed modal, or returns a no such
 * alert error if no modal is currently displayed.
 */
GeckoDriver.prototype.getTextFromDialog = function(cmd, resp) {
  this._checkIfAlertIsPresent();

  let {infoBody} = this.dialog.ui;
  resp.body.value = infoBody.textContent;
};

/**
 * Sends keys to the input field of a currently displayed modal, or
 * returns a no such alert error if no modal is currently displayed. If
 * a tab modal is currently displayed but has no means for text input,
 * an element not visible error is returned.
 */
GeckoDriver.prototype.sendKeysToDialog = function(cmd, resp) {
  this._checkIfAlertIsPresent();

  // see toolkit/components/prompts/content/commonDialog.js
  let {loginContainer, loginTextbox} = this.dialog.ui;
  if (loginContainer.hidden) {
    throw new ElementNotVisibleError("This prompt does not accept text input");
  }

  let win = this.dialog.window ? this.dialog.window : this.getCurrentWindow();
  event.sendKeysToElement(
      cmd.parameters.value,
      loginTextbox,
      {ignoreVisibility: true},
      win);
};

GeckoDriver.prototype._checkIfAlertIsPresent = function() {
  if (!this.dialog || !this.dialog.ui) {
    throw new NoAlertOpenError(
        "No tab modal was open when attempting to get the dialog text");
  }
};

/**
 * Quits Firefox with the provided flags and tears down the current
 * session.
 */
GeckoDriver.prototype.quitApplication = function(cmd, resp) {
  if (this.appName != "Firefox") {
    throw new WebDriverError("In app initiated quit only supported in Firefox");
  }

  let flags = Ci.nsIAppStartup.eAttemptQuit;
  for (let k of cmd.parameters.flags) {
    flags |= Ci.nsIAppStartup[k];
  }

  this.stopSignal_();
  resp.send();

  this.sessionTearDown();
  Services.startup.quit(flags);
};

/**
 * Helper function to convert an outerWindowID into a UID that Marionette
 * tracks.
 */
GeckoDriver.prototype.generateFrameId = function(id) {
  let uid = id + (this.appName == "B2G" ? "-b2g" : "");
  return uid;
};

/** Receives all messages from content messageManager. */
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

      let en = cookieManager.getCookiesFromHost(host);
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

    case "Marionette:getFiles":
      // Generates file objects to send back to the content script
      // for handling file uploads.
      let val = message.json.value;
      let command_id = message.json.command_id;
      Cu.importGlobalProperties(["File"]);
      try {
        let file = new File(val);
        this.sendAsync("receiveFiles",
                       {file: file, command_id: command_id});
      } catch (e) {
        let err = `File not found: ${val}`;
        this.sendAsync("receiveFiles",
                       {error: err, command_id: command_id});
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
        // If remoteness gets updated we need to call newSession. In the case
        // of desktop this just sets up a small amount of state that doesn't
        // change over the course of a session.
        this.sendAsync("newSession", this.sessionCapabilities);
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
  "setScriptTimeout": GeckoDriver.prototype.setScriptTimeout,
  "timeouts": GeckoDriver.prototype.timeouts,
  "singleTap": GeckoDriver.prototype.singleTap,
  "actionChain": GeckoDriver.prototype.actionChain,
  "multiAction": GeckoDriver.prototype.multiAction,
  "executeAsyncScript": GeckoDriver.prototype.executeAsyncScript,
  "executeJSScript": GeckoDriver.prototype.executeJSScript,
  "setSearchTimeout": GeckoDriver.prototype.setSearchTimeout,
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
  "goUrl": GeckoDriver.prototype.get,  // deprecated
  "getCurrentUrl": GeckoDriver.prototype.getCurrentUrl,
  "getUrl": GeckoDriver.prototype.getCurrentUrl,  // deprecated
  "goBack": GeckoDriver.prototype.goBack,
  "goForward": GeckoDriver.prototype.goForward,
  "refresh":  GeckoDriver.prototype.refresh,
  "getWindowHandle": GeckoDriver.prototype.getWindowHandle,
  "getCurrentWindowHandle":  GeckoDriver.prototype.getWindowHandle,  // Selenium 2 compat
  "getChromeWindowHandle": GeckoDriver.prototype.getChromeWindowHandle,
  "getCurrentChromeWindowHandle": GeckoDriver.prototype.getChromeWindowHandle,
  "getWindow":  GeckoDriver.prototype.getWindowHandle,  // deprecated
  "getWindowHandles": GeckoDriver.prototype.getWindowHandles,
  "getChromeWindowHandles": GeckoDriver.prototype.getChromeWindowHandles,
  "getCurrentWindowHandles": GeckoDriver.prototype.getWindowHandles,  // Selenium 2 compat
  "getWindows":  GeckoDriver.prototype.getWindowHandles,  // deprecated
  "getWindowPosition": GeckoDriver.prototype.getWindowPosition,
  "setWindowPosition": GeckoDriver.prototype.setWindowPosition,
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
  "closeWindow": GeckoDriver.prototype.close,  // deprecated
  "closeChromeWindow": GeckoDriver.prototype.closeChromeWindow,
  "setTestName": GeckoDriver.prototype.setTestName,
  "takeScreenshot": GeckoDriver.prototype.takeScreenshot,
  "screenShot": GeckoDriver.prototype.takeScreenshot,  // deprecated
  "screenshot": GeckoDriver.prototype.takeScreenshot,  // Selenium 2 compat
  "addCookie": GeckoDriver.prototype.addCookie,
  "getCookies": GeckoDriver.prototype.getCookies,
  "getAllCookies": GeckoDriver.prototype.getCookies,  // deprecated
  "deleteAllCookies": GeckoDriver.prototype.deleteAllCookies,
  "deleteCookie": GeckoDriver.prototype.deleteCookie,
  "getActiveElement": GeckoDriver.prototype.getActiveElement,
  "getScreenOrientation": GeckoDriver.prototype.getScreenOrientation,
  "setScreenOrientation": GeckoDriver.prototype.setScreenOrientation,
  "getWindowSize": GeckoDriver.prototype.getWindowSize,
  "setWindowSize": GeckoDriver.prototype.setWindowSize,
  "maximizeWindow": GeckoDriver.prototype.maximizeWindow,
  "dismissDialog": GeckoDriver.prototype.dismissDialog,
  "acceptDialog": GeckoDriver.prototype.acceptDialog,
  "getTextFromDialog": GeckoDriver.prototype.getTextFromDialog,
  "sendKeysToDialog": GeckoDriver.prototype.sendKeysToDialog,
  "quitApplication": GeckoDriver.prototype.quitApplication,
};
