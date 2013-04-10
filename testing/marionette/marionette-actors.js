/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Gecko-specific actors.
 */

const FRAME_SCRIPT = "chrome://marionette/content/marionette-listener.js";

let {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
               .getService(Ci.mozIJSSubScriptLoader);
loader.loadSubScript("chrome://marionette/content/marionette-simpletest.js");
loader.loadSubScript("chrome://marionette/content/marionette-log-obj.js");
loader.loadSubScript("chrome://marionette/content/marionette-perf.js");
Cu.import("chrome://marionette/content/marionette-elements.js");
let utils = {};
loader.loadSubScript("chrome://marionette/content/EventUtils.js", utils);
loader.loadSubScript("chrome://marionette/content/ChromeUtils.js", utils);
loader.loadSubScript("chrome://marionette/content/atoms.js", utils);

let specialpowers = {};
loader.loadSubScript("chrome://specialpowers/content/SpecialPowersObserver.js",
                     specialpowers);
specialpowers.specialPowersObserver = new specialpowers.SpecialPowersObserver();
specialpowers.specialPowersObserver.init();

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");  

Services.prefs.setBoolPref("marionette.contentListener", false);
let appName = Services.appinfo.name;

// import logger
Cu.import("resource://gre/modules/services-common/log4moz.js");
let logger = Log4Moz.repository.getLogger("Marionette");
logger.info('marionette-actors.js loaded');

let bypassOffline = false;

try {
  XPCOMUtils.defineLazyGetter(this, "libcutils", function () {
    Cu.import("resource://gre/modules/systemlibs.js");
    return libcutils;
  });
  if (libcutils) {
    let platform = libcutils.property_get("ro.product.device");
    logger.info("Platform detected is " + platform);
    bypassOffline = (platform == "generic" || platform == "panda");
  }
}
catch(e) {}

if (bypassOffline) {
  logger.info("Bypassing offline status.");
  Services.prefs.setBoolPref("network.gonk.manage-offline-status", false);
  Services.io.manageOfflineStatus = false;
  Services.io.offline = false;
}

// This is used to prevent newSession from returning before the telephony
// API's are ready; see bug 792647.  This assumes that marionette-actors.js
// will be loaded before the 'system-message-listener-ready' message
// is fired.  If this stops being true, this approach will have to change.
let systemMessageListenerReady = false;
Services.obs.addObserver(function() {
  systemMessageListenerReady = true;
}, "system-message-listener-ready", false);

/**
 * Creates the root actor once a connection is established
 */

function createRootActor(aConnection)
{
  return new MarionetteRootActor(aConnection);
}

/**
 * Root actor for Marionette. Add any future actors to its actor pool.
 * Implements methods needed by resource:///modules/devtools/dbg-server.jsm
 */

function MarionetteRootActor(aConnection)
{
  this.conn = aConnection;
  this._marionetteActor = new MarionetteDriverActor(this.conn);
  this._marionetteActorPool = null; //hold future actors

  this._marionetteActorPool = new ActorPool(this.conn);
  this._marionetteActorPool.addActor(this._marionetteActor);
  this.conn.addActorPool(this._marionetteActorPool);
}

MarionetteRootActor.prototype = {
  /**
   * Called when a client first makes a connection
   *
   * @return object
   *         returns the name of the actor, the application type, and any traits
   */
  sayHello: function MRA_sayHello() {
    return { from: "root",
             applicationType: "gecko",
             traits: [] };
  },

  /**
   * Called when client disconnects. Cleans up marionette driver actions.
   */
  disconnect: function MRA_disconnect() {
    this._marionetteActor.deleteSession();
  },

  /**
   * Used to get the running marionette actor, so we can issue commands
   *
   * @return object
   *         Returns the ID the client can use to communicate with the
   *         MarionetteDriverActor
   */
  getMarionetteID: function MRA_getMarionette() {
    return { "from": "root",
             "id": this._marionetteActor.actorID } ;
  }
};

// register the calls
MarionetteRootActor.prototype.requestTypes = {
  "getMarionetteID": MarionetteRootActor.prototype.getMarionetteID,
  "sayHello": MarionetteRootActor.prototype.sayHello
};

/**
 * An object representing a frame that Marionette has loaded a
 * frame script in.
 */
function MarionetteRemoteFrame(windowId, frameId) {
  this.windowId = windowId;
  this.frameId = frameId;
  this.targetFrameId = null;
  this.messageManager = null;
  this.command_id = null;
}
// persistent list of remote frames that Marionette has loaded a frame script in
let remoteFrames = [];

/*
 * Custom exceptions
 */
function FrameSendNotInitializedError(frame) {
  this.code = 54;
  this.frame = frame;
  this.message = "Error sending message to frame (NS_ERROR_NOT_INITIALIZED)";
  this.toString = function() {
    return this.message + " " + this.frame + "; frame has closed.";
  }
}

function FrameSendFailureError(frame) {
  this.code = 55;
  this.frame = frame;
  this.message = "Error sending message to frame (NS_ERROR_FAILURE)";
  this.toString = function() {
    return this.message + " " + this.frame + "; frame not responding.";
  }
}

/**
 * This actor is responsible for all marionette API calls. It gets created
 * for each connection and manages all chrome and browser based calls. It
 * mediates content calls by issuing appropriate messages to the content process.
 */
function MarionetteDriverActor(aConnection)
{
  this.uuidGen = Cc["@mozilla.org/uuid-generator;1"]
                   .getService(Ci.nsIUUIDGenerator);

  this.conn = aConnection;
  this.globalMessageManager = Cc["@mozilla.org/globalmessagemanager;1"]
                             .getService(Ci.nsIMessageBroadcaster);
  this.messageManager = this.globalMessageManager;
  this.browsers = {}; //holds list of BrowserObjs
  this.curBrowser = null; // points to current browser
  this.context = "content";
  this.scriptTimeout = null;
  this.pageTimeout = null;
  this.timer = null;
  this.marionetteLog = new MarionetteLogObj();
  this.marionettePerf = new MarionettePerfData();
  this.command_id = null;
  this.mainFrame = null; //topmost chrome frame
  this.curFrame = null; //subframe that currently has focus
  this.importedScripts = FileUtils.getFile('TmpD', ['marionettescriptchrome']);
  this.currentRemoteFrame = null; // a member of remoteFrames
  this.testName = null;
  this.mozBrowserClose = null;

  //register all message listeners
  this.addMessageManagerListeners(this.messageManager);
}

MarionetteDriverActor.prototype = {

  //name of the actor
  actorPrefix: "marionette",

  /**
   * Helper methods:
   */

  /**
   * Switches to the global ChromeMessageBroadcaster, potentially replacing a frame-specific
   * ChromeMessageSender.  Has no effect if the global ChromeMessageBroadcaster is already
   * in use.  If this replaces a frame-specific ChromeMessageSender, it removes the message
   * listeners from that sender, and then puts the corresponding frame script "to sleep",
   * which removes most of the message listeners from it as well.
   */
  switchToGlobalMessageManager: function MDA_switchToGlobalMM() {
    if (this.currentRemoteFrame !== null) {
      this.removeMessageManagerListeners(this.messageManager);
      this.sendAsync("sleepSession", null, null, true);
    }
    this.messageManager = this.globalMessageManager;
    this.currentRemoteFrame = null;
  },

  /**
   * Helper method to send async messages to the content listener
   *
   * @param string name
   *        Suffix of the targetted message listener (Marionette:<suffix>)
   * @param object values
   *        Object to send to the listener
   */
  sendAsync: function MDA_sendAsync(name, values, commandId, ignoreFailure) {
    let success = true;
    if (values instanceof Object && commandId) {
      values.command_id = commandId;
    }
    if (this.currentRemoteFrame !== null) {
      try {
        this.messageManager.sendAsyncMessage(
          "Marionette:" + name + this.currentRemoteFrame.targetFrameId, values);
      }
      catch(e) {
        if (!ignoreFailure) {
          success = false;
          let error = e;
          switch(e.result) {
            case Components.results.NS_ERROR_FAILURE:
              error = new FrameSendFailureError(this.currentRemoteFrame);
              break;
            case Components.results.NS_ERROR_NOT_INITIALIZED:
              error = new FrameSendNotInitializedError(this.currentRemoteFrame);
              break;
            default:
              break;
          }
          let code = error.hasOwnProperty('code') ? e.code : 500;
          this.sendError(error.toString(), code, error.stack, commandId);
        }
      }
    }
    else {
      this.messageManager.broadcastAsyncMessage(
        "Marionette:" + name + this.curBrowser.curFrameId, values);
    }
    return success;
  },

  /**
   * Adds listeners for messages from content frame scripts.
   *
   * @param object messageManager
   *        The messageManager object (ChromeMessageBroadcaster or ChromeMessageSender)
   *        to which the listeners should be added.
   */
  addMessageManagerListeners: function MDA_addMessageManagerListeners(messageManager) {
    messageManager.addMessageListener("Marionette:ok", this);
    messageManager.addMessageListener("Marionette:done", this);
    messageManager.addMessageListener("Marionette:error", this);
    messageManager.addMessageListener("Marionette:log", this);
    messageManager.addMessageListener("Marionette:shareData", this);
    messageManager.addMessageListener("Marionette:register", this);
    messageManager.addMessageListener("Marionette:runEmulatorCmd", this);
    messageManager.addMessageListener("Marionette:switchToFrame", this);
  },

  /**
   * Removes listeners for messages from content frame scripts.
   *
   * @param object messageManager
   *        The messageManager object (ChromeMessageBroadcaster or ChromeMessageSender)
   *        from which the listeners should be removed.
   */
  removeMessageManagerListeners: function MDA_removeMessageManagerListeners(messageManager) {
    messageManager.removeMessageListener("Marionette:ok", this);
    messageManager.removeMessageListener("Marionette:done", this);
    messageManager.removeMessageListener("Marionette:error", this);
    messageManager.removeMessageListener("Marionette:log", this);
    messageManager.removeMessageListener("Marionette:shareData", this);
    messageManager.removeMessageListener("Marionette:register", this);
    messageManager.removeMessageListener("Marionette:runEmulatorCmd", this);
    messageManager.removeMessageListener("Marionette:switchToFrame", this);
  },

  logRequest: function MDA_logRequest(type, data) {
    logger.debug("Got request: " + type + ", data: " + JSON.stringify(data) + ", id: " + this.command_id);
  },

  /**
   * Generic method to pass a response to the client
   *
   * @param object msg
   *        Response to send back to client
   * @param string command_id
   *        Unique identifier assigned to the client's request.
   *        Used to distinguish the asynchronous responses.
   */
  sendToClient: function MDA_sendToClient(msg, command_id) {
    logger.info("sendToClient: " + JSON.stringify(msg) + ", " + command_id +
                ", " + this.command_id);
    if (!command_id) {
      logger.warn("got a response with no command_id");
      return;
    }
    else if (command_id != -1) {
      // A command_id of -1 is used for emulator callbacks, and those
      // don't use this.command_id.
      if (!this.command_id) {
        // A null value for this.command_id means we've already processed
        // a message for the previous value, and so the current message is a
        // duplicate.
        logger.warn("ignoring duplicate response for command_id " + command_id);
        return;
      }
      else if (this.command_id != command_id) {
        logger.warn("ignoring out-of-sync response");
        return;
      }
    }
    this.conn.send(msg);
    if (command_id != -1) {
      // Don't unset this.command_id if this message is to process an
      // emulator callback, since another response for this command_id is
      // expected, after the containing call to execute_async_script finishes.
      this.command_id = null;
    }
  },

  /**
   * Send a value to client
   *
   * @param object value
   *        Value to send back to client
   * @param string command_id
   *        Unique identifier assigned to the client's request.
   *        Used to distinguish the asynchronous responses.
   */
  sendResponse: function MDA_sendResponse(value, command_id) {
    if (typeof(value) == 'undefined')
        value = null;
    this.sendToClient({from:this.actorID, value: value}, command_id);
  },

  /**
   * Send ack to client
   * 
   * @param string command_id
   *        Unique identifier assigned to the client's request.
   *        Used to distinguish the asynchronous responses.
   */
  sendOk: function MDA_sendOk(command_id) {
    this.sendToClient({from:this.actorID, ok: true}, command_id);
  },

  /**
   * Send error message to client
   *
   * @param string message
   *        Error message
   * @param number status
   *        Status number
   * @param string trace
   *        Stack trace
   * @param string command_id
   *        Unique identifier assigned to the client's request.
   *        Used to distinguish the asynchronous responses.
   */
  sendError: function MDA_sendError(message, status, trace, command_id) {
    let error_msg = {message: message, status: status, stacktrace: trace};
    this.sendToClient({from:this.actorID, error: error_msg}, command_id);
  },

  /**
   * Gets the current active window
   * 
   * @return nsIDOMWindow
   */
  getCurrentWindow: function MDA_getCurrentWindow() {
    let type = null;
    if (this.curFrame == null) {
      if (this.curBrowser == null) {
        if (this.context == "content") {
          type = 'navigator:browser';
        }
        return Services.wm.getMostRecentWindow(type);
      }
      else {
        return this.curBrowser.window;
      }
    }
    else {
      return this.curFrame;
    }
  },

  /**
   * Gets the the window enumerator
   *
   * @return nsISimpleEnumerator
   */
  getWinEnumerator: function MDA_getWinEnumerator() {
    let type = null;
    if (appName != "B2G" && this.context == "content") {
      type = 'navigator:browser';
    }
    return Services.wm.getEnumerator(type);
  },

  /**
   * Create a new BrowserObj for window and add to known browsers
   * 
   * @param nsIDOMWindow win
   *        Window for which we will create a BrowserObj
   *
   * @return string
   *        Returns the unique server-assigned ID of the window
   */
  addBrowser: function MDA_addBrowser(win) {
    let browser = new BrowserObj(win);
    let winId = win.QueryInterface(Ci.nsIInterfaceRequestor).
                    getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
    winId = winId + ((appName == "B2G") ? '-b2g' : '');
    this.browsers[winId] = browser;
    this.curBrowser = this.browsers[winId];
    if (this.curBrowser.elementManager.seenItems[winId] == undefined) {
      //add this to seenItems so we can guarantee the user will get winId as this window's id
      this.curBrowser.elementManager.seenItems[winId] = Cu.getWeakReference(win);
    }
  },

  /**
   * Start a new session in a new browser. 
   *
   * If newSession is true, we will switch focus to the start frame 
   * when it registers. Also, if it is in desktop, then a new tab 
   * with the start page uri (about:blank) will be opened.
   *
   * @param nsIDOMWindow win
   *        Window whose browser we need to access
   * @param boolean newSession
   *        True if this is the first time we're talking to this browser
   */
  startBrowser: function MDA_startBrowser(win, newSession) {
    this.mainFrame = win;
    this.curFrame = null;
    this.addBrowser(win);
    this.curBrowser.newSession = newSession;
    this.curBrowser.startSession(newSession, win, this.whenBrowserStarted.bind(this));
  },

  /**
   * Callback invoked after a new session has been started in a browser.
   * Loads the Marionette frame script into the browser if needed.
   *
   * @param nsIDOMWindow win
   *        Window whose browser we need to access
   * @param boolean newSession
   *        True if this is the first time we're talking to this browser
   */
  whenBrowserStarted: function MDA_whenBrowserStarted(win, newSession) {
    try {
      if (!Services.prefs.getBoolPref("marionette.contentListener") || !newSession) {
        this.curBrowser.loadFrameScript(FRAME_SCRIPT, win);
      }
    }
    catch (e) {
      //there may not always be a content process
      logger.info("could not load listener into content for page: " + win.location.href);
    }
    utils.window = win;
  },

  /**
   * Recursively get all labeled text
   *
   * @param nsIDOMElement el
   *        The parent element
   * @param array lines
   *        Array that holds the text lines
   */
  getVisibleText: function MDA_getVisibleText(el, lines) {
    let nodeName = el.nodeName;
    try {
      if (utils.isElementDisplayed(el)) {
        if (el.value) {
          lines.push(el.value);
        }
        for (var child in el.childNodes) {
          this.getVisibleText(el.childNodes[child], lines);
        };
      }
    }
    catch (e) {
      if (nodeName == "#text") {
        lines.push(el.textContent);
      }
    }
  },

  getCommandId: function MDA_getCommandId() {
    return this.uuidGen.generateUUID().toString();
  },

  /**
   * Marionette API:
   *
   * All methods implementing a command from the client should create a
   * command_id, and then use this command_id in all messages exchanged with
   * the frame scripts and with responses sent to the client.  This prevents
   * commands and responses from getting out-of-sync, which can happen in
   * the case of execute_async calls that timeout and then later send a
   * response, and other situations.  See bug 779011. See setScriptTimeout()
   * for a basic example.
   */

  /**
   * Create a new session. This creates a BrowserObj.
   *
   * In a desktop environment, this opens a new 'about:blank' tab for 
   * the client to test in.
   *
   */
  newSession: function MDA_newSession() {
    this.command_id = this.getCommandId();
    this.newSessionCommandId = this.command_id;

    this.scriptTimeout = 10000;

    function waitForWindow() {
      let checkTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      let win = this.getCurrentWindow();
      if (!win ||
          (appName == "Firefox" && !win.gBrowser) ||
          (appName == "Fennec" && !win.BrowserApp)) { 
        checkTimer.initWithCallback(waitForWindow.bind(this), 100, Ci.nsITimer.TYPE_ONE_SHOT);
      }
      else {
        this.startBrowser(win, true);
      }
    }

    this.switchToGlobalMessageManager();

    if (!Services.prefs.getBoolPref("marionette.contentListener")) {
      waitForWindow.call(this);
    }
    else if ((appName != "Firefox") && (this.curBrowser == null)) {
      //if there is a content listener, then we just wake it up
      this.addBrowser(this.getCurrentWindow());
      this.curBrowser.startSession(false, this.getCurrentWindow(), this.whenBrowserStarted);
      this.messageManager.broadcastAsyncMessage("Marionette:restart", {});
    }
    else {
      this.sendError("Session already running", 500, null, this.command_id);
    }
  },

  getSessionCapabilities: function MDA_getSessionCapabilities(){
    this.command_id = this.getCommandId();

    let rotatable = appName == "B2G" ? true : false;

    let value = {
          'appBuildId' : Services.appinfo.appBuildID,
          'XULappId' : Services.appinfo.ID,
          'cssSelectorsEnabled': true,
          'browserName': appName,
          'handlesAlerts': false,
          'javascriptEnabled': true,
          'nativeEvents': false,
          'platform': Services.appinfo.OS,
          'rotatable': rotatable,
          'takesScreenshot': false,
          'version': Services.appinfo.version
    };

    this.sendResponse(value, this.command_id);
  },

  getStatus: function MDA_getStatus(){
    this.command_id = this.getCommandId();

    let arch;
    try {
      arch = (Services.appinfo.XPCOMABI || 'unknown').split('-')[0]
    }
    catch (ignored) {
      arch = 'unknown'
    };

    let value = {
          'os': {
            'arch': arch,
            'name': Services.appinfo.OS,
            'version': 'unknown'
          },
          'build': {
            'revision': 'unknown',
            'time': Services.appinfo.platformBuildID,
            'version': Services.appinfo.version
          }
    };

    this.sendResponse(value, this.command_id);
  },

  /**
   * Log message. Accepts user defined log-level.
   *
   * @param object aRequest
   *        'value' member holds log message
   *        'level' member hold log level
   */
  log: function MDA_log(aRequest) {
    this.command_id = this.getCommandId();
    this.marionetteLog.log(aRequest.value, aRequest.level);
    this.sendOk(this.command_id);
  },

  /**
   * Return all logged messages.
   */
  getLogs: function MDA_getLogs() {
    this.command_id = this.getCommandId();
    this.sendResponse(this.marionetteLog.getLogs(), this.command_id);
  },
  
  /**
   * Log some performance data
   */
  addPerfData: function MDA_addPerfData(aRequest) {
    this.command_id = this.getCommandId();
    this.marionettePerf.addPerfData(aRequest.suite, aRequest.name, aRequest.value);
    this.sendOk(this.command_id);
  },

  /**
   * Retrieve the performance data
   */
  getPerfData: function MDA_getPerfData() {
    this.command_id = this.getCommandId();
    this.sendResponse(this.marionettePerf.getPerfData(), this.command_id);
  },

  /**
   * Sets the context of the subsequent commands to be either 'chrome' or 'content'
   *
   * @param object aRequest
   *        'value' member holds the name of the context to be switched to
   */
  setContext: function MDA_setContext(aRequest) {
    this.command_id = this.getCommandId();
    this.logRequest("setContext", aRequest);
    let context = aRequest.value;
    if (context != "content" && context != "chrome") {
      this.sendError("invalid context", 500, null, this.command_id);
    }
    else {
      this.context = context;
      this.sendOk(this.command_id);
    }
  },

  /**
   * Returns a chrome sandbox that can be used by the execute_foo functions.
   *
   * @param nsIDOMWindow aWindow
   *        Window in which we will execute code
   * @param Marionette marionette
   *        Marionette test instance
   * @param object args
   *        Client given args
   * @return Sandbox
   *        Returns the sandbox
   */
  createExecuteSandbox: function MDA_createExecuteSandbox(aWindow, marionette, args, specialPowers, command_id) {
    try {
      args = this.curBrowser.elementManager.convertWrappedArguments(args, aWindow);
    }
    catch(e) {
      this.sendError(e.message, e.code, e.stack, command_id);
      return;
    }

    let _chromeSandbox = new Cu.Sandbox(aWindow,
       { sandboxPrototype: aWindow, wantXrays: false, sandboxName: ''});
    _chromeSandbox.__namedArgs = this.curBrowser.elementManager.applyNamedArgs(args);
    _chromeSandbox.__marionetteParams = args;
    _chromeSandbox.testUtils = utils;

    marionette.exports.forEach(function(fn) {
      try {
        _chromeSandbox[fn] = marionette[fn].bind(marionette);
      }
      catch(e) {
        _chromeSandbox[fn] = marionette[fn];
      }
    });

    _chromeSandbox.isSystemMessageListenerReady =
        function() { return systemMessageListenerReady; }

    if (specialPowers == true) {
      loader.loadSubScript("chrome://specialpowers/content/specialpowersAPI.js",
                           _chromeSandbox);
      loader.loadSubScript("chrome://specialpowers/content/SpecialPowersObserverAPI.js",
                           _chromeSandbox);
      loader.loadSubScript("chrome://specialpowers/content/ChromePowers.js",
                           _chromeSandbox);
    }

    return _chromeSandbox;
  },

  /**
   * Executes a script in the given sandbox.
   *
   * @param Sandbox sandbox
   *        Sandbox in which the script will run
   * @param string script
   *        The script to run
   * @param boolean directInject
   *        If true, then the script will be run as is,
   *        and not as a function body (as you would
   *        do using the WebDriver spec)
   * @param boolean async
   *        True if the script is asynchronous
   */
  executeScriptInSandbox: function MDA_executeScriptInSandbox(sandbox, script,
     directInject, async, command_id, timeout) {

    if (directInject && async &&
        (timeout == null || timeout == 0)) {
      this.sendError("Please set a timeout", 21, null, command_id);
      return;
    }

    if (this.importedScripts.exists()) {
      let stream = Cc["@mozilla.org/network/file-input-stream;1"].  
                    createInstance(Ci.nsIFileInputStream);
      stream.init(this.importedScripts, -1, 0, 0);
      let data = NetUtil.readInputStreamToString(stream, stream.available());
      script = data + script;
    }

    let res = Cu.evalInSandbox(script, sandbox, "1.8");

    if (directInject && !async &&
        (res == undefined || res.passed == undefined)) {
      this.sendError("finish() not called", 500, null, command_id);
      return;
    }

    if (!async) {
      this.sendResponse(this.curBrowser.elementManager.wrapValue(res),
                        command_id);
    }
  },

  /**
   * Execute the given script either as a function body (executeScript)
   * or directly (for 'mochitest' like JS Marionette tests)
   *
   * @param object aRequest
   *        'value' member is the script to run
   *        'args' member holds the arguments to the script
   * @param boolean directInject
   *        if true, it will be run directly and not as a 
   *        function body
   */
  execute: function MDA_execute(aRequest, directInject) {
    let timeout = aRequest.scriptTimeout ? aRequest.scriptTimeout : this.scriptTimeout;
    let command_id = this.command_id = this.getCommandId();
    this.logRequest("execute", aRequest);
    if (aRequest.newSandbox == undefined) {
      //if client does not send a value in newSandbox, 
      //then they expect the same behaviour as webdriver
      aRequest.newSandbox = true;
    }
    if (this.context == "content") {
      this.sendAsync("executeScript",
                     {
                       value: aRequest.value,
                       args: aRequest.args,
                       newSandbox: aRequest.newSandbox,
                       timeout: timeout,
                       specialPowers: aRequest.specialPowers
                     },
                     command_id);
      return;
    }

    let curWindow = this.getCurrentWindow();
    let marionette = new Marionette(this, curWindow, "chrome",
                                    this.marionetteLog, this.marionettePerf,
                                    timeout, this.testName);
    let _chromeSandbox = this.createExecuteSandbox(curWindow,
                                                   marionette,
                                                   aRequest.args,
                                                   aRequest.specialPowers,
                                                   command_id);
    if (!_chromeSandbox)
      return;

    try {
      _chromeSandbox.finish = function chromeSandbox_finish() {
        return marionette.generate_results();
      };

      let script;
      if (directInject) {
        script = aRequest.value;
      }
      else {
        script = "let func = function() {" +
                       aRequest.value + 
                     "};" +
                     "func.apply(null, __marionetteParams);";
      }
      this.executeScriptInSandbox(_chromeSandbox, script, directInject,
                                  false, command_id, timeout);
    }
    catch (e) {
      this.sendError(e.name + ': ' + e.message, 17, e.stack, command_id);
    }
  },

  /**
   * Set the timeout for asynchronous script execution
   *
   * @param object aRequest
   *        'value' member is time in milliseconds to set timeout
   */
  setScriptTimeout: function MDA_setScriptTimeout(aRequest) {
    this.command_id = this.getCommandId();
    let timeout = parseInt(aRequest.value);
    if(isNaN(timeout)){
      this.sendError("Not a Number", 500, null, this.command_id);
    }
    else {
      this.scriptTimeout = timeout;
      this.sendOk(this.command_id);
    }
  },

  /**
   * execute pure JS script. Used to execute 'mochitest'-style Marionette tests.
   *
   * @param object aRequest
   *        'value' member holds the script to execute
   *        'args' member holds the arguments to the script
   *        'timeout' member will be used as the script timeout if it is given
   */
  executeJSScript: function MDA_executeJSScript(aRequest) {
    let timeout = aRequest.scriptTimeout ? aRequest.scriptTimeout : this.scriptTimeout;
    let command_id = this.command_id = this.getCommandId();
    //all pure JS scripts will need to call Marionette.finish() to complete the test.
    if (aRequest.newSandbox == undefined) {
      //if client does not send a value in newSandbox, 
      //then they expect the same behaviour as webdriver
      aRequest.newSandbox = true;
    }
    if (this.context == "chrome") {
      if (aRequest.async) {
        this.executeWithCallback(aRequest, aRequest.async);
      }
      else {
        this.execute(aRequest, true);
      }
    }
    else {
      this.sendAsync("executeJSScript",
                     {
                       value: aRequest.value,
                       args: aRequest.args,
                       newSandbox: aRequest.newSandbox,
                       async: aRequest.async,
                       timeout: timeout,
                       specialPowers: aRequest.specialPowers
                     },
                     command_id);
   }
  },

  /**
   * This function is used by executeAsync and executeJSScript to execute a script
   * in a sandbox. 
   * 
   * For executeJSScript, it will return a message only when the finish() method is called.
   * For executeAsync, it will return a response when marionetteScriptFinished/arguments[arguments.length-1] 
   * method is called, or if it times out.
   *
   * @param object aRequest
   *        'value' member holds the script to execute
   *        'args' member holds the arguments for the script
   * @param boolean directInject
   *        if true, it will be run directly and not as a 
   *        function body
   */
  executeWithCallback: function MDA_executeWithCallback(aRequest, directInject) {
    let timeout = aRequest.scriptTimeout ? aRequest.scriptTimeout : this.scriptTimeout;
    let command_id = this.command_id = this.getCommandId();
    this.logRequest("executeWithCallback", aRequest);
    if (aRequest.newSandbox == undefined) {
      //if client does not send a value in newSandbox, 
      //then they expect the same behaviour as webdriver
      aRequest.newSandbox = true;
    }

    if (this.context == "content") {
      this.sendAsync("executeAsyncScript",
                     {
                       value: aRequest.value,
                       args: aRequest.args,
                       id: this.command_id,
                       newSandbox: aRequest.newSandbox,
                       timeout: timeout,
                       specialPowers: aRequest.specialPowers
                     },
                     command_id);
      return;
    }

    let curWindow = this.getCurrentWindow();
    let original_onerror = curWindow.onerror;
    let that = this;
    that.timeout = timeout;
    let marionette = new Marionette(this, curWindow, "chrome",
                                    this.marionetteLog, this.marionettePerf,
                                    timeout, this.testName);
    marionette.command_id = this.command_id;

    function chromeAsyncReturnFunc(value, status) {
      if (that._emu_cbs && Object.keys(that._emu_cbs).length) {
        value = "Emulator callback still pending when finish() called";
        status = 500;
        that._emu_cbs = null;
      }

      if (value == undefined)
        value = null;
      if (that.command_id == marionette.command_id) {
        if (that.timer != null) {
          that.timer.cancel();
          that.timer = null;
        }

        curWindow.onerror = original_onerror;

        if (status == 0 || status == undefined) {
          that.sendToClient({from: that.actorID, value: that.curBrowser.elementManager.wrapValue(value), status: status},
                            marionette.command_id);
        }
        else {
          let error_msg = {message: value, status: status, stacktrace: null};
          that.sendToClient({from: that.actorID, error: error_msg},
                            marionette.command_id);
        }
      }
    }

    curWindow.onerror = function (errorMsg, url, lineNumber) {
      chromeAsyncReturnFunc(errorMsg + " at: " + url + " line: " + lineNumber, 17);
      return true;
    };

    function chromeAsyncFinish() {
      chromeAsyncReturnFunc(marionette.generate_results(), 0);
    }

    let _chromeSandbox = this.createExecuteSandbox(curWindow,
                                                   marionette,
                                                   aRequest.args,
                                                   aRequest.specialPowers,
                                                   command_id);
    if (!_chromeSandbox)
      return;

    try {

      this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      if (this.timer != null) {
        this.timer.initWithCallback(function() {
          chromeAsyncReturnFunc("timed out", 28);
        }, that.timeout, Ci.nsITimer.TYPE_ONESHOT);
      }

      _chromeSandbox.returnFunc = chromeAsyncReturnFunc;
      _chromeSandbox.finish = chromeAsyncFinish;

      let script;
      if (directInject) {
        script = aRequest.value;
      }
      else {
        script =  '__marionetteParams.push(returnFunc);'
                + 'let marionetteScriptFinished = returnFunc;'
                + 'let __marionetteFunc = function() {' + aRequest.value + '};'
                + '__marionetteFunc.apply(null, __marionetteParams);';
      }

      this.executeScriptInSandbox(_chromeSandbox, script, directInject,
                                  true, command_id, timeout);
    } catch (e) {
      chromeAsyncReturnFunc(e.name + ": " + e.message, 17);
    }
  },

  /**
   * Navigates to given url
   *
   * @param object aRequest
   *        'value' member holds the url to navigate to
   */
  goUrl: function MDA_goUrl(aRequest) {
    let command_id = this.command_id = this.getCommandId();
    if (this.context != "chrome") {
      aRequest.command_id = command_id;
      aRequest.pageTimeout = this.pageTimeout;
      this.sendAsync("goUrl", aRequest, command_id);
      return;
    }

    this.getCurrentWindow().location.href = aRequest.value;
    let checkTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    let start = new Date().getTime();
    let end = null;
    function checkLoad() { 
      end = new Date().getTime();
      let elapse = end - start;
      if (this.pageTimeout == null || elapse <= this.pageTimeout){
        if (curWindow.document.readyState == "complete") { 
          sendOk(command_id);
          return;
        }
        else{ 
          checkTimer.initWithCallback(checkLoad, 100, Ci.nsITimer.TYPE_ONE_SHOT);
        }
      }
      else{
        sendError("Error loading page", 13, null, command_id);
        return;
      }
    }//end
    checkTimer.initWithCallback(checkLoad, 100, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  /**
   * Gets current url
   */
  getUrl: function MDA_getUrl() {
    this.command_id = this.getCommandId();
    if (this.context == "chrome") {
      this.sendResponse(this.getCurrentWindow().location.href, this.command_id);
    }
    else {
      this.sendAsync("getUrl", {}, this.command_id);
    }
  },

  /**
   * Gets the current title of the window
   */
  getTitle: function MDA_getTitle() {
    this.command_id = this.getCommandId();
    this.sendAsync("getTitle", {}, this.command_id);
  },

  /**
   * Gets the page source of the content document
   */
  getPageSource: function MDA_getPageSource(){
    this.command_id = this.getCommandId();
    if (this.context == "chrome"){
      var curWindow = this.getCurrentWindow();
      var XMLSerializer = curWindow.XMLSerializer; 
      var pageSource = new XMLSerializer().serializeToString(curWindow.document);
      this.sendResponse(pageSource, this.command_id);
    }
    else {
      this.sendAsync("getPageSource", {}, this.command_id);
    }
  },

  /**
   * Go back in history
   */
  goBack: function MDA_goBack() {
    this.command_id = this.getCommandId();
    this.sendAsync("goBack", {}, this.command_id);
  },

  /**
   * Go forward in history
   */
  goForward: function MDA_goForward() {
    this.command_id = this.getCommandId();
    this.sendAsync("goForward", {}, this.command_id);
  },

  /**
   * Refresh the page
   */
  refresh: function MDA_refresh() {
    this.command_id = this.getCommandId();
    this.sendAsync("refresh", {}, this.command_id);
  },

  /**
   * Get the current window's server-assigned ID
   */
  getWindow: function MDA_getWindow() {
    this.command_id = this.getCommandId();
    for (let i in this.browsers) {
      if (this.curBrowser == this.browsers[i]) {
        this.sendResponse(i, this.command_id);
      }
    }
  },

  /**
   * Get the server-assigned IDs of all available windows
   */
  getWindows: function MDA_getWindows() {
    this.command_id = this.getCommandId();
    let res = [];
    let winEn = this.getWinEnumerator(); 
    while(winEn.hasMoreElements()) {
      let foundWin = winEn.getNext();
      let winId = foundWin.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
      winId = winId + ((appName == "B2G") ? '-b2g' : '');
      res.push(winId)
    }
    this.sendResponse(res, this.command_id);
  },

  /**
   * Switch to a window based on name or server-assigned id.
   * Searches based on name, then id.
   *
   * @param object aRequest
   *        'value' member holds the name or id of the window to switch to
   */
  switchToWindow: function MDA_switchToWindow(aRequest) {
    let command_id = this.command_id = this.getCommandId();
    let winEn = this.getWinEnumerator(); 
    while(winEn.hasMoreElements()) {
      let foundWin = winEn.getNext();
      let winId = foundWin.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindowUtils)
                          .outerWindowID;
      winId = winId + ((appName == "B2G") ? '-b2g' : '');
      if (aRequest.value == foundWin.name || aRequest.value == winId) {
        if (this.browsers[winId] == undefined) {
          //enable Marionette in that browser window
          this.startBrowser(foundWin, false);
        }
        else {
          utils.window = foundWin;
          this.curBrowser = this.browsers[winId];
        }
        this.sendOk(command_id);
        return;
      }
    }
    this.sendError("Unable to locate window " + aRequest.value, 23, null,
                   command_id);
  },
 
  /**
   * Switch to a given frame within the current window
   *
   * @param object aRequest
   *        'element' is the element to switch to
   *        'value' if element is not set, then this
   *                holds either the id, name or index 
   *                of the frame to switch to
   */
  switchToFrame: function MDA_switchToFrame(aRequest) {
    let command_id = this.command_id = this.getCommandId();
    this.logRequest("switchToFrame", aRequest);
    let checkTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    let checkLoad = function() { 
      let errorRegex = /about:.+(error)|(blocked)\?/;
      if (curWindow.document.readyState == "complete") { 
        this.sendOk(command_id);
        return;
      } 
      else if (curWindow.document.readyState == "interactive" && errorRegex.exec(curWindow.document.baseURI)) {
        this.sendError("Error loading page", 13, null, command_id);
        return;
      }
      
      checkTimer.initWithCallback(checkLoad.bind(this), 100, Ci.nsITimer.TYPE_ONE_SHOT);
    }
    let curWindow = this.getCurrentWindow();
    if (this.context == "chrome") {
      let foundFrame = null;
      if ((aRequest.value == null) && (aRequest.element == null)) {
        this.curFrame = null;
        if (aRequest.focus) {
          this.mainFrame.focus();
        }
        checkTimer.initWithCallback(checkLoad.bind(this), 100, Ci.nsITimer.TYPE_ONE_SHOT);
        return;
      }
      if (aRequest.element != undefined) {
        if (this.curBrowser.elementManager.seenItems[aRequest.element] != undefined) {
          let wantedFrame = this.curBrowser.elementManager.getKnownElement(aRequest.element, curWindow); //HTMLIFrameElement
          let numFrames = curWindow.frames.length;
          for (let i = 0; i < numFrames; i++) {
            if (curWindow.frames[i].frameElement == wantedFrame) {
              curWindow = curWindow.frames[i]; 
              this.curFrame = curWindow;
              if (aRequest.focus) {
                this.curFrame.focus();
              }
              checkTimer.initWithCallback(checkLoad.bind(this), 100, Ci.nsITimer.TYPE_ONE_SHOT);
              return;
          }
        }
      }
    }
    switch(typeof(aRequest.value)) {
      case "string" :
        let foundById = null;
        let numFrames = curWindow.frames.length;
        for (let i = 0; i < numFrames; i++) {
          //give precedence to name
          let frame = curWindow.frames[i];
          let frameElement = frame.frameElement;
          if (frame.name == aRequest.value) {
            foundFrame = i;
            break;
          } else if ((foundById == null) && (frameElement.id == aRequest.value)) {
            foundById = i;
          }
        }
        if ((foundFrame == null) && (foundById != null)) {
          foundFrame = foundById;
        }
        break;
      case "number":
        if (curWindow.frames[aRequest.value] != undefined) {
          foundFrame = aRequest.value;
        }
        break;
      }
      if (foundFrame != null) {
        curWindow = curWindow.frames[foundFrame];
        this.curFrame = curWindow;
        if (aRequest.focus) {
          this.curFrame.focus();
        }
        checkTimer.initWithCallback(checkLoad.bind(this), 100, Ci.nsITimer.TYPE_ONE_SHOT);
      } else {
        this.sendError("Unable to locate frame: " + aRequest.value, 8, null,
                       command_id);
      }
    }
    else {
      if ((!aRequest.value) && (!aRequest.element) &&
          (this.currentRemoteFrame !== null)) {
        // We're currently using a ChromeMessageSender for a remote frame, so this
        // request indicates we need to switch back to the top-level (parent) frame.
        // We'll first switch to the parent's (global) ChromeMessageBroadcaster, so
        // we send the message to the right listener.
        this.switchToGlobalMessageManager();
      }
      aRequest.command_id = command_id;
      this.sendAsync("switchToFrame", aRequest, command_id);
    }
  },

  /**
   * Set timeout for searching for elements
   *
   * @param object aRequest
   *        'value' holds the search timeout in milliseconds
   */
  setSearchTimeout: function MDA_setSearchTimeout(aRequest) {
    this.command_id = this.getCommandId();
    if (this.context == "chrome") {
      try {
        this.curBrowser.elementManager.setSearchTimeout(aRequest.value);
        this.sendOk(this.command_id);
      }
      catch (e) {
        this.sendError(e.message, e.code, e.stack, this.command_id);
      }
    }
    else {
      this.sendAsync("setSearchTimeout",
                     { value: aRequest.value },
                     this.command_id);
    }
  },

/**
 * Set a value to decide if sending mouse event
 *
 * @param object aRequest
 *        'value' holds the boolean value
 */
 sendMouseEvent: function MDA_sendMouseEvent(aRequest) {
   this.command_id = this.getCommandId();
   if (this.context == "chrome") {
     this.sendError("Not in Chrome", 500, null, this.command_id);
    }
    else {
      this.sendAsync("sendMouseEvent", {value: aRequest.value,
                                        command_id: this.command_id});
    }
 },

  /**
   * Set timeout for page loading, searching and scripts
   *
   * @param object aRequest
   *        'type' hold the type of timeout
   *        'ms' holds the timeout in milliseconds
   */
  timeouts: function MDA_timeouts(aRequest){
    /*setTimeout*/
    this.command_id = this.getCommandId();
    let timeout_type = aRequest.timeoutType;
    let timeout = parseInt(aRequest.ms);
    if (isNaN(timeout)) {
      this.sendError("Not a Number", 500, null, this.command_id);
    }
    else {
      if (timeout_type == "implicit") {
        aRequest.value = aRequest.ms;
        this.setSearchTimeout(aRequest);
      }
      else if (timeout_type == "script") {
        aRequest.value = aRequest.ms;
        this.setScriptTimeout(aRequest);
      }
      else {
        this.pageTimeout = timeout;
        this.sendOk(this.command_id);
      }
    }
  },

  /**
   * Single Tap
   *
   * @param object aRequest
            'element' represents the ID of the element to single tap on
   */
  singleTap: function MDA_singleTap(aRequest) {
    this.command_id = this.getCommandId();
    let serId = aRequest.element;
    let x = aRequest.x;
    let y = aRequest.y;
    if (this.context == "chrome") {
      this.sendError("Not in Chrome", 500, null, this.command_id);
    }
    else {
      this.sendAsync("singleTap",
                     {
                       value: serId,
                       corx: x,
                       cory: y
                     },
                     this.command_id);
    }
  },

  /**
   * Double Tap
   *
   * @param object aRequest
   *        'element' represents the ID of the element to double tap on
   */
  doubleTap: function MDA_doubleTap(aRequest) {
    this.command_id = this.getCommandId();
    let serId = aRequest.element;
    let x = aRequest.x;
    let y = aRequest.y;
    if (this.context == "chrome") {
      this.sendError("Not in Chrome", 500, null, this.command_id);
    }
    else {
      this.sendAsync("doubleTap",
                     {
                       value: serId,
                       corx: x,
                       cory: y
                     },
                     this.command_id);
    }
  },

  /**
   * Start touch
   *
   * @param object aRequest
   *        'element' represents the ID of the element to touch
   */
  press: function MDA_press(aRequest) {
    this.command_id = this.getCommandId();
    let element = aRequest.element;
    let x = aRequest.x;
    let y = aRequest.y;
    if (this.context == "chrome") {
      this.sendError("Not in Chrome", 500, null, this.command_id);
    }
    else {
      this.sendAsync("press",
                     {
                       value: element,
                       corx: x,
                       cory: y
                     },
                     this.command_id);
    }
  },

  /**
   * Cancel touch
   *
   * @param object aRequest
   *        'element' represents the ID of the element to touch
   */
  cancelTouch: function MDA_cancelTouch(aRequest) {
    this.command_id = this.getCommandId();
    let element = aRequest.element;
    let touchId = aRequest.touchId;
    if (this.context == "chrome") {
      this.sendError("Not in Chrome", 500, null, this.command_id);
    }
    else {
      this.sendAsync("cancelTouch",
                     {
                       value: element,
                       touchId: touchId
                     },
                     this.command_id);
    }
  },

  /**
   * End touch
   *
   * @param object aRequest
   *        'element' represents the ID of the element to end the touch
   */
  release: function MDA_release(aRequest) {
    this.command_id = this.getCommandId();
    let element = aRequest.element;
    let touchId = aRequest.touchId;
    let x = aRequest.x;
    let y = aRequest.y;
    if (this.context == "chrome") {
      this.sendError("Not in Chrome", 500, null, this.command_id);
    }
    else {
      this.sendAsync("release",
                     {
                       value: element,
                       touchId: touchId,
                       corx: x,
                       cory: y
                     },
                     this.command_id);
    }
  },

  /**
   * actionChain
   *
   * @param object aRequest
   *        'value' represents a nested array: inner array represents each event; outer array represents collection of events
   */
  actionChain: function MDA_actionChain(aRequest) {
    this.command_id = this.getCommandId();
    if (this.context == "chrome") {
      this.sendError("Not in Chrome", 500, null, this.command_id);
    }
    else {
      this.sendAsync("actionChain",
                     {
                       chain: aRequest.chain,
                       nextId: aRequest.nextId
                     },
                     this.command_id);
    }
  },

  /**
   * multiAction
   *
   * @param object aRequest
   *        'value' represents a nested array: inner array represents each event;
   *        middle array represents collection of events for each finger
   *        outer array represents all the fingers
   */

  multiAction: function MDA_multiAction(aRequest) {
    this.command_id = this.getCommandId();
    if (this.context == "chrome") {
       this.sendError("Not in Chrome", 500, null, this.command_id);
    }
    else {
      this.sendAsync("multiAction",
                     {
                       value: aRequest.value,
                       maxlen: aRequest.max_length
                     },
                     this.command_id);
   }
 },

  /**
   * Find an element using the indicated search strategy.
   *
   * @param object aRequest
   *        'using' member indicates which search method to use
   *        'value' member is the value the client is looking for
   */
  findElement: function MDA_findElement(aRequest) {
    let command_id = this.command_id = this.getCommandId();
    if (this.context == "chrome") {
      let id;
      try {
        let on_success = this.sendResponse.bind(this);
        let on_error = this.sendError.bind(this);
        id = this.curBrowser.elementManager.find(
                              this.getCurrentWindow(),
                              aRequest,
                              on_success,
                              on_error,
                              false,
                              command_id);
      }
      catch (e) {
        this.sendError(e.message, e.code, e.stack, command_id);
        return;
      }
    }
    else {
      this.sendAsync("findElementContent",
                     {
                       value: aRequest.value,
                       using: aRequest.using,
                       element: aRequest.element
                     },
                     command_id);
    }
  },

  /**
   * Find elements using the indicated search strategy.
   *
   * @param object aRequest
   *        'using' member indicates which search method to use
   *        'value' member is the value the client is looking for
   */
  findElements: function MDA_findElements(aRequest) {
    let command_id = this.command_id = this.getCommandId();
    if (this.context == "chrome") {
      let id;
      try {
        let on_success = this.sendResponse.bind(this);
        let on_error = this.sendError.bind(this);
        id = this.curBrowser.elementManager.find(this.getCurrentWindow(),
                                                 aRequest,
                                                 on_success,
                                                 on_error,
                                                 true,
                                                 command_id);
      }
      catch (e) {
        this.sendError(e.message, e.code, e.stack, command_id);
        return;
      }
    }
    else {
      this.sendAsync("findElementsContent",
                     {
                       value: aRequest.value,
                       using: aRequest.using,
                       element: aRequest.element
                     },
                     command_id);
    }
  },

  /**
   * Return the active element on the page
   */
  getActiveElement: function MDA_getActiveElement(){
    let command_id = this.command_id = this.getCommandId();
    this.sendAsync("getActiveElement", {}, command_id);
  },

  /**
   * Send click event to element
   *
   * @param object aRequest
   *        'element' member holds the reference id to
   *        the element that will be clicked
   */
  clickElement: function MDA_clickElement(aRequest) {
    let command_id = this.command_id = this.getCommandId();
    if (this.context == "chrome") {
      try {
        //NOTE: click atom fails, fall back to click() action
        let el = this.curBrowser.elementManager.getKnownElement(
            aRequest.element, this.getCurrentWindow());
        el.click();
        this.sendOk(command_id);
      }
      catch (e) {
        this.sendError(e.message, e.code, e.stack, command_id);
      }
    }
    else {
      // We need to protect against the click causing an OOP frame to close. 
      // This fires the mozbrowserclose event when it closes so we need to 
      // listen for it and then just send an error back. The person making the
      // call should be aware something isnt right and handle accordingly
      let curWindow = this.getCurrentWindow();
      let self = this;
      this.mozBrowserClose = function() { 
        curWindow.removeEventListener('mozbrowserclose', self.mozBrowserClose, true);
        self.switchToGlobalMessageManager();
        self.sendError("The frame closed during the click, recovering to allow further communications", 500, null, command_id);
      };
      curWindow.addEventListener('mozbrowserclose', this.mozBrowserClose, true);
      this.sendAsync("clickElement",
                     { element: aRequest.element },
                     command_id);
    }
  },

  /**
   * Get a given attribute of an element
   *
   * @param object aRequest
   *        'element' member holds the reference id to
   *        the element that will be inspected
   *        'name' member holds the name of the attribute to retrieve
   */
  getElementAttribute: function MDA_getElementAttribute(aRequest) {
    let command_id = this.command_id = this.getCommandId();
    if (this.context == "chrome") {
      try {
        let el = this.curBrowser.elementManager.getKnownElement(
            aRequest.element, this.getCurrentWindow());
        this.sendResponse(utils.getElementAttribute(el, aRequest.name),
                          command_id);
      }
      catch (e) {
        this.sendError(e.message, e.code, e.stack, command_id);
      }
    }
    else {
      this.sendAsync("getElementAttribute",
                     {
                       element: aRequest.element,
                       name: aRequest.name
                     },
                     command_id);
    }
  },

  /**
   * Get the text of an element, if any. Includes the text of all child elements.
   *
   * @param object aRequest
   *        'element' member holds the reference id to
   *        the element that will be inspected 
   */
  getElementText: function MDA_getElementText(aRequest) {
    let command_id = this.command_id = this.getCommandId();
    if (this.context == "chrome") {
      //Note: for chrome, we look at text nodes, and any node with a "label" field
      try {
        let el = this.curBrowser.elementManager.getKnownElement(
            aRequest.element, this.getCurrentWindow());
        let lines = [];
        this.getVisibleText(el, lines);
        lines = lines.join("\n");
        this.sendResponse(lines, command_id);
      }
      catch (e) {
        this.sendError(e.message, e.code, e.stack, command_id);
      }
    }
    else {
      this.sendAsync("getElementText",
                     { element: aRequest.element },
                     command_id);
    }
  },

  /**
   * Get the tag name of the element.
   *
   * @param object aRequest
   *        'element' member holds the reference id to
   *        the element that will be inspected 
   */
  getElementTagName: function MDA_getElementTagName(aRequest) {
    let command_id = this.command_id = this.getCommandId();
    if (this.context == "chrome") {
      try {
        let el = this.curBrowser.elementManager.getKnownElement(
            aRequest.element, this.getCurrentWindow());
        this.sendResponse(el.tagName.toLowerCase(), command_id);
      }
      catch (e) {
        this.sendError(e.message, e.code, e.stack, command_id);
      }
    }
    else {
      this.sendAsync("getElementTagName",
                     { element: aRequest.element },
                     command_id);
    }
  },

  /**
   * Check if element is displayed
   *
   * @param object aRequest
   *        'element' member holds the reference id to
   *        the element that will be checked 
   */
  isElementDisplayed: function MDA_isElementDisplayed(aRequest) {
    let command_id = this.command_id = this.getCommandId();
    if (this.context == "chrome") {
      try {
        let el = this.curBrowser.elementManager.getKnownElement(
            aRequest.element, this.getCurrentWindow());
        this.sendResponse(utils.isElementDisplayed(el), command_id);
      }
      catch (e) {
        this.sendError(e.message, e.code, e.stack, command_id);
      }
    }
    else {
      this.sendAsync("isElementDisplayed",
                     { element:aRequest.element },
                     command_id);
    }
  },

  /**
   * Check if element is enabled
   *
   * @param object aRequest
   *        'element' member holds the reference id to
   *        the element that will be checked
   */
  isElementEnabled: function MDA_isElementEnabled(aRequest) {
    let command_id = this.command_id = this.getCommandId();
    if (this.context == "chrome") {
      try {
        //Selenium atom doesn't quite work here
        let el = this.curBrowser.elementManager.getKnownElement(
            aRequest.element, this.getCurrentWindow());
        if (el.disabled != undefined) {
          this.sendResponse(!!!el.disabled, command_id);
        }
        else {
        this.sendResponse(true, command_id);
        }
      }
      catch (e) {
        this.sendError(e.message, e.code, e.stack, command_id);
      }
    }
    else {
      this.sendAsync("isElementEnabled",
                     { element:aRequest.element },
                     command_id);
    }
  },

  /**
   * Check if element is selected
   *
   * @param object aRequest
   *        'element' member holds the reference id to
   *        the element that will be checked
   */
  isElementSelected: function MDA_isElementSelected(aRequest) {
    let command_id = this.command_id = this.getCommandId();
    if (this.context == "chrome") {
      try {
        //Selenium atom doesn't quite work here
        let el = this.curBrowser.elementManager.getKnownElement(
            aRequest.element, this.getCurrentWindow());
        if (el.checked != undefined) {
          this.sendResponse(!!el.checked, command_id);
        }
        else if (el.selected != undefined) {
          this.sendResponse(!!el.selected, command_id);
        }
        else {
          this.sendResponse(true, command_id);
        }
      }
      catch (e) {
        this.sendError(e.message, e.code, e.stack, command_id);
      }
    }
    else {
      this.sendAsync("isElementSelected",
                     { element:aRequest.element },
                     command_id);
    }
  },

  getElementSize: function MDA_getElementSize(aRequest) {
    let command_id = this.command_id = this.getCommandId();
    if (this.context == "chrome") {
      try {
        let el = this.curBrowser.elementManager.getKnownElement(
            aRequest.element, this.getCurrentWindow());
        let clientRect = el.getBoundingClientRect();  
        this.sendResponse({width: clientRect.width, height: clientRect.height},
                          command_id);
      }
      catch (e) {
        this.sendError(e.message, e.code, e.stack, command_id);
      }
    }
    else {
      this.sendAsync("getElementSize",
                     { element:aRequest.element },
                     command_id);
    }
  },

  /**
   * Send key presses to element after focusing on it
   *
   * @param object aRequest
   *        'element' member holds the reference id to
   *        the element that will be checked
   *        'value' member holds the value to send to the element
   */
  sendKeysToElement: function MDA_sendKeysToElement(aRequest) {
    let command_id = this.command_id = this.getCommandId();
    if (this.context == "chrome") {
      try {
        let el = this.curBrowser.elementManager.getKnownElement(
            aRequest.element, this.getCurrentWindow());
        el.focus();
        utils.sendString(aRequest.value.join(""), utils.window);
        this.sendOk(command_id);
      }
      catch (e) {
        this.sendError(e.message, e.code, e.stack, command_id);
      }
    }
    else {
      this.sendAsync("sendKeysToElement",
                     {
                       element:aRequest.element,
                       value: aRequest.value
                     },
                     command_id);
    }
  },

  /**
   * Sets the test name
   *
   * The test name is used in logging messages.
   */
  setTestName: function MDA_setTestName(aRequest) {
    this.command_id = this.getCommandId();
    this.logRequest("setTestName", aRequest);
    this.testName = aRequest.value;
    this.sendAsync("setTestName",
                   { value: aRequest.value },
                   this.command_id);
  },

  /**
   * Clear the text of an element
   *
   * @param object aRequest
   *        'element' member holds the reference id to
   *        the element that will be cleared 
   */
  clearElement: function MDA_clearElement(aRequest) {
    let command_id = this.command_id = this.getCommandId();
    if (this.context == "chrome") {
      //the selenium atom doesn't work here
      try {
        let el = this.curBrowser.elementManager.getKnownElement(
            aRequest.element, this.getCurrentWindow());
        if (el.nodeName == "textbox") {
          el.value = "";
        }
        else if (el.nodeName == "checkbox") {
          el.checked = false;
        }
        this.sendOk(command_id);
      }
      catch (e) {
        this.sendError(e.message, e.code, e.stack, command_id);
      }
    }
    else {
      this.sendAsync("clearElement",
                     { element:aRequest.element },
                     command_id);
    }
  },

  getElementPosition: function MDA_getElementPosition(aRequest) {
    this.command_id = this.getCommandId();
    this.sendAsync("getElementPosition",
                   { element:aRequest.element },
                   this.command_id);
  },

  /**
   * Add a cookie to the document.
   */
  addCookie: function MDA_addCookie(aRequest) {
    this.command_id = this.getCommandId();
    this.sendAsync("addCookie",
                   { cookie:aRequest.cookie },
                   this.command_id);
  },

  /**
   * Get all visible cookies for a document
   */
  getAllCookies: function MDA_getAllCookies() {
    this.command_id = this.getCommandId();
    this.sendAsync("getAllCookies", {}, this.command_id);
  },

  /**
   * Delete all cookies that are visible to a document
   */
  deleteAllCookies: function MDA_deleteAllCookies() {
    this.command_id = this.getCommandId();
    this.sendAsync("deleteAllCookies", {}, this.command_id);
  },

  /**
   * Delete a cookie by name
   */
  deleteCookie: function MDA_deleteCookie(aRequest) {
    this.command_id = this.getCommandId();
    this.sendAsync("deleteCookie",
                   { name:aRequest.name },
                   this.command_id);
  },

  /**
   * Closes the Browser Window.
   *
   * If it is B2G it returns straight away and does not do anything
   *
   * If is desktop it calculates how many windows are open and if there is only 
   * 1 then it deletes the session otherwise it closes the window
   */
  closeWindow: function MDA_closeWindow() {
    let command_id = this.command_id = this.getCommandId();
    if (appName == "B2G") {
      // We can't close windows so just return
      this.sendOk(command_id);
    }
    else {
      // Get the total number of windows
      let numOpenWindows = 0;
      let winEnum = this.getWinEnumerator();
      while (winEnum.hasMoreElements()) {
        numOpenWindows += 1;
        winEnum.getNext(); 
      }

      // if there is only 1 window left, delete the session
      if (numOpenWindows === 1){
        this.deleteSession();
        return;
      }

      try{
        this.messageManager.removeDelayedFrameScript(FRAME_SCRIPT); 
        this.getCurrentWindow().close();
        this.sendOk(command_id);
      }
      catch (e) {
        this.sendError("Could not close window: " + e.message, 13, e.stack,
                       command_id);
      }
    }
  }, 

  /**
   * Deletes the session.
   * 
   * If it is a desktop environment, it will close the session's tab and close all listeners
   *
   * If it is a B2G environment, it will make the main content listener sleep, and close
   * all other listeners. The main content listener persists after disconnect (it's the homescreen),
   * and can safely be reused.
   */
  deleteSession: function MDA_deleteSession() {
    let command_id = this.command_id = this.getCommandId();
    if (this.curBrowser != null) {
      if (appName == "B2G") {
        this.globalMessageManager.broadcastAsyncMessage(
            "Marionette:sleepSession" + this.curBrowser.mainContentId, {});
        this.curBrowser.knownFrames.splice(
            this.curBrowser.knownFrames.indexOf(this.curBrowser.mainContentId), 1);
      }
      else {
        //don't set this pref for B2G since the framescript can be safely reused
        Services.prefs.setBoolPref("marionette.contentListener", false);
      }
      this.curBrowser.closeTab();
      //delete session in each frame in each browser
      for (let win in this.browsers) {
        for (let i in this.browsers[win].knownFrames) {
          this.globalMessageManager.broadcastAsyncMessage("Marionette:deleteSession" + this.browsers[win].knownFrames[i], {});
        }
      }
      let winEnum = this.getWinEnumerator();
      while (winEnum.hasMoreElements()) {
        winEnum.getNext().messageManager.removeDelayedFrameScript(FRAME_SCRIPT); 
      }
    }
    this.sendOk(command_id);
    this.removeMessageManagerListeners(this.globalMessageManager);
    this.switchToGlobalMessageManager();
    // reset frame to the top-most frame
    this.curFrame = null;
    if (this.mainFrame) {
      this.mainFrame.focus();
    }
    this.curBrowser = null;
    try {
      this.importedScripts.remove(false);
    }
    catch (e) {
    }
  },

  /**
   * Returns the current status of the Application Cache
   */
  getAppCacheStatus: function MDA_getAppCacheStatus(aRequest) {
    this.command_id = this.getCommandId();
    this.sendAsync("getAppCacheStatus", {}, this.command_id);
  },

  _emu_cb_id: 0,
  _emu_cbs: null,
  runEmulatorCmd: function runEmulatorCmd(cmd, callback) {
    if (callback) {
      if (!this._emu_cbs) {
        this._emu_cbs = {};
      }
      this._emu_cbs[this._emu_cb_id] = callback;
    }
    this.sendToClient({emulator_cmd: cmd, id: this._emu_cb_id}, -1);
    this._emu_cb_id += 1;
  },

  emulatorCmdResult: function emulatorCmdResult(message) {
    if (this.context != "chrome") {
      this.sendAsync("emulatorCmdResult", message, -1);
      return;
    }

    if (!this._emu_cbs) {
      return;
    }

    let cb = this._emu_cbs[message.id];
    delete this._emu_cbs[message.id];
    if (!cb) {
      return;
    }
    try {
      cb(message.result);
    }
    catch(e) {
      this.sendError(e.message, e.code, e.stack, -1);
      return;
    }
  },
  
  importScript: function MDA_importScript(aRequest) {
    let command_id = this.command_id = this.getCommandId();
    if (this.context == "chrome") {
      let file;
      if (this.importedScripts.exists()) {
        file = FileUtils.openFileOutputStream(this.importedScripts,
            FileUtils.MODE_APPEND | FileUtils.MODE_WRONLY);
      }
      else {
        //Note: The permission bits here don't actually get set (bug 804563)
        this.importedScripts.createUnique(
            Components.interfaces.nsIFile.NORMAL_FILE_TYPE, parseInt("0666", 8));
        file = FileUtils.openFileOutputStream(this.importedScripts,
            FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE);
        this.importedScripts.permissions = parseInt("0666", 8); //actually set permissions
      }
      file.write(aRequest.script, aRequest.script.length);
      file.close();
      this.sendOk(command_id);
    }
    else {
      this.sendAsync("importScript",
                     { script: aRequest.script },
                     command_id);
    }
  },

  /**
   * Takes a screenshot of a DOM node. If there is no node given a screenshot
   * of the window will be taken.
   */
  screenShot: function MDA_saveScreenshot(aRequest) {
    this.command_id = this.getCommandId();
    this.sendAsync("screenShot",
                   {
                     element: aRequest.element,
                     highlights: aRequest.highlights
                   },
                   this.command_id);
  },

  /**
   * Helper function to convert an outerWindowID into a UID that Marionette
   * tracks.
   */
  generateFrameId: function MDA_generateFrameId(id) {
    let uid = id + (appName == "B2G" ? "-b2g" : "");
    return uid;
  },

  /**
   * Receives all messages from content messageManager
   */
  receiveMessage: function MDA_receiveMessage(message) {
    // We need to just check if we need to remove the mozbrowserclose listener
    if (this.mozBrowserClose !== null){
      let curWindow = this.getCurrentWindow();
      curWindow.removeEventListener('mozbrowserclose', this.mozBrowserClose, true);
      this.mozBrowserClose = null;
    }

    switch (message.name) {
      case "Marionette:done":
        this.sendResponse(message.json.value, message.json.command_id);
        break;
      case "Marionette:ok":
        this.sendOk(message.json.command_id);
        break;
      case "Marionette:error":
        this.sendError(message.json.message, message.json.status, message.json.stacktrace, message.json.command_id);
        break;
      case "Marionette:log":
        //log server-side messages
        logger.info(message.json.message);
        break;
      case "Marionette:shareData":
        //log messages from tests
        if (message.json.log) {
          this.marionetteLog.addLogs(message.json.log);
        }
        if (message.json.perf) {
          this.marionettePerf.appendPerfData(message.json.perf);
        }
        break;
      case "Marionette:runEmulatorCmd":
        this.sendToClient(message.json, -1);
        break;
      case "Marionette:switchToFrame":
        // Switch to a remote frame.
        let thisWin = this.getCurrentWindow();
        let frameWindow = thisWin.QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIDOMWindowUtils)
                                 .getOuterWindowWithId(message.json.win);
        let thisFrame = frameWindow.document.getElementsByTagName("iframe")[message.json.frame];
        let mm = thisFrame.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader.messageManager;

        // See if this frame already has our frame script loaded in it; if so,
        // just wake it up.
        for (let i = 0; i < remoteFrames.length; i++) {
          let frame = remoteFrames[i];
          if ((frame.messageManager == mm)) {
            this.currentRemoteFrame = frame;
            this.currentRemoteFrame.command_id = message.json.command_id;
            this.messageManager = frame.messageManager;
            this.addMessageManagerListeners(this.messageManager);
            this.messageManager.sendAsyncMessage("Marionette:restart", {});
            return;
          }
        }

        // Load the frame script in this frame, and set the frame's ChromeMessageSender
        // as the active message manager.
        this.addMessageManagerListeners(mm);
        mm.loadFrameScript(FRAME_SCRIPT, true);
        this.messageManager = mm;
        let aFrame = new MarionetteRemoteFrame(message.json.win, message.json.frame);
        aFrame.messageManager = this.messageManager;
        aFrame.command_id = message.json.command_id;
        remoteFrames.push(aFrame);
        this.currentRemoteFrame = aFrame;
        break;
      case "Marionette:register":
        // This code processes the content listener's registration information
        // and either accepts the listener, or ignores it
        let nullPrevious = (this.curBrowser.curFrameId == null);
        let curWin = this.getCurrentWindow();
        let listenerWindow = curWin.QueryInterface(Ci.nsIInterfaceRequestor)
                                   .getInterface(Ci.nsIDOMWindowUtils)
                                   .getOuterWindowWithId(message.json.value);

        if (!listenerWindow || (listenerWindow.location.href != message.json.href) &&
            (this.currentRemoteFrame !== null)) {
          // The outerWindowID from an OOP frame will not be meaningful to
          // the parent process here, since each process maintains its own
          // independent window list.  So, it will either be null (!listenerWindow)
          // or it will point to some random window, which will hopefully 
          // cause an href mistmach.  Currently this only happens
          // in B2G for OOP frames registered in Marionette:switchToFrame, so
          // we'll acknowledge the switchToFrame message here.
          // XXX: Should have a better way of determining that this message
          // is from a remote frame.
          this.currentRemoteFrame.targetFrameId = this.generateFrameId(message.json.value);
          this.sendAsync("setState",
                         {
                           scriptTimeout: this.scriptTimeout,
                           searchTimeout: this.curBrowser.elementManager.searchTimeout
                         },
                         this.currentRemoteFrame.command_id);
        }

        let browserType;
        try {
          browserType = message.target.getAttribute("type");
        } catch (ex) {
          // browserType remains undefined.
        }
        let reg = {};
        if (!browserType || browserType != "content") {
          reg.id = this.curBrowser.register(this.generateFrameId(message.json.value),
                                         message.json.href); 
        }
        this.curBrowser.elementManager.seenItems[reg.id] = Cu.getWeakReference(listenerWindow); //add to seenItems
        reg.importedScripts = this.importedScripts.path;
        if (nullPrevious && (this.curBrowser.curFrameId != null)) {
          if (!this.sendAsync("newSession",
                              { B2G: (appName == "B2G") },
                              this.newSessionCommandId)) {
            return;
          }
          if (this.curBrowser.newSession) {
            this.sendResponse(reg.id, this.newSessionCommandId);
            this.newSessionCommandId = null;
          }
        }
        return reg;
    }
  }
};

MarionetteDriverActor.prototype.requestTypes = {
  "newSession": MarionetteDriverActor.prototype.newSession,
  "getSessionCapabilities": MarionetteDriverActor.prototype.getSessionCapabilities,
  "getStatus": MarionetteDriverActor.prototype.getStatus,
  "log": MarionetteDriverActor.prototype.log,
  "getLogs": MarionetteDriverActor.prototype.getLogs,
  "addPerfData": MarionetteDriverActor.prototype.addPerfData,
  "getPerfData": MarionetteDriverActor.prototype.getPerfData,
  "setContext": MarionetteDriverActor.prototype.setContext,
  "executeScript": MarionetteDriverActor.prototype.execute,
  "setScriptTimeout": MarionetteDriverActor.prototype.setScriptTimeout,
  "timeouts": MarionetteDriverActor.prototype.timeouts,
  "singleTap": MarionetteDriverActor.prototype.singleTap,
  "doubleTap": MarionetteDriverActor.prototype.doubleTap,
  "press": MarionetteDriverActor.prototype.press,
  "release": MarionetteDriverActor.prototype.release,
  "cancelTouch": MarionetteDriverActor.prototype.cancelTouch,
  "actionChain": MarionetteDriverActor.prototype.actionChain,
  "multiAction": MarionetteDriverActor.prototype.multiAction,
  "executeAsyncScript": MarionetteDriverActor.prototype.executeWithCallback,
  "executeJSScript": MarionetteDriverActor.prototype.executeJSScript,
  "setSearchTimeout": MarionetteDriverActor.prototype.setSearchTimeout,
  "sendMouseEvent": MarionetteDriverActor.prototype.sendMouseEvent,
  "findElement": MarionetteDriverActor.prototype.findElement,
  "findElements": MarionetteDriverActor.prototype.findElements,
  "clickElement": MarionetteDriverActor.prototype.clickElement,
  "getElementAttribute": MarionetteDriverActor.prototype.getElementAttribute,
  "getElementText": MarionetteDriverActor.prototype.getElementText,
  "getElementTagName": MarionetteDriverActor.prototype.getElementTagName,
  "isElementDisplayed": MarionetteDriverActor.prototype.isElementDisplayed,
  "getElementSize": MarionetteDriverActor.prototype.getElementSize,
  "isElementEnabled": MarionetteDriverActor.prototype.isElementEnabled,
  "isElementSelected": MarionetteDriverActor.prototype.isElementSelected,
  "sendKeysToElement": MarionetteDriverActor.prototype.sendKeysToElement,
  "getElementPosition": MarionetteDriverActor.prototype.getElementPosition,
  "clearElement": MarionetteDriverActor.prototype.clearElement,
  "getTitle": MarionetteDriverActor.prototype.getTitle,
  "getPageSource": MarionetteDriverActor.prototype.getPageSource,
  "goUrl": MarionetteDriverActor.prototype.goUrl,
  "getUrl": MarionetteDriverActor.prototype.getUrl,
  "goBack": MarionetteDriverActor.prototype.goBack,
  "goForward": MarionetteDriverActor.prototype.goForward,
  "refresh":  MarionetteDriverActor.prototype.refresh,
  "getWindow":  MarionetteDriverActor.prototype.getWindow,
  "getWindows":  MarionetteDriverActor.prototype.getWindows,
  "switchToFrame": MarionetteDriverActor.prototype.switchToFrame,
  "switchToWindow": MarionetteDriverActor.prototype.switchToWindow,
  "deleteSession": MarionetteDriverActor.prototype.deleteSession,
  "emulatorCmdResult": MarionetteDriverActor.prototype.emulatorCmdResult,
  "importScript": MarionetteDriverActor.prototype.importScript,
  "getAppCacheStatus": MarionetteDriverActor.prototype.getAppCacheStatus,
  "closeWindow": MarionetteDriverActor.prototype.closeWindow,
  "setTestName": MarionetteDriverActor.prototype.setTestName,
  "screenShot": MarionetteDriverActor.prototype.screenShot,
  "addCookie": MarionetteDriverActor.prototype.addCookie,
  "getAllCookies": MarionetteDriverActor.prototype.getAllCookies,
  "deleteAllCookies": MarionetteDriverActor.prototype.deleteAllCookies,
  "deleteCookie": MarionetteDriverActor.prototype.deleteCookie,
  "getActiveElement": MarionetteDriverActor.prototype.getActiveElement
};

/**
 * Creates a BrowserObj. BrowserObjs handle interactions with the
 * browser, according to the current environment (desktop, b2g, etc.)
 *
 * @param nsIDOMWindow win
 *        The window whose browser needs to be accessed
 */

function BrowserObj(win) {
  this.DESKTOP = "desktop";
  this.B2G = "B2G";
  this.browser;
  this.tab = null;
  this.window = win;
  this.knownFrames = [];
  this.curFrameId = null;
  this.startPage = "about:blank";
  this.mainContentId = null; // used in B2G to identify the homescreen content page
  this.newSession = true; //used to set curFrameId upon new session
  this.elementManager = new ElementManager([SELECTOR, NAME, LINK_TEXT, PARTIAL_LINK_TEXT]);
  this.setBrowser(win);
}

BrowserObj.prototype = {
  /**
   * Set the browser if the application is not B2G
   *
   * @param nsIDOMWindow win
   *        current window reference
   */
  setBrowser: function BO_setBrowser(win) {
    switch (appName) {
      case "Firefox":
        this.browser = win.gBrowser;
        break;
      case "Fennec":
        this.browser = win.BrowserApp;
        break;
    }
  },
  /**
   * Called when we start a session with this browser.
   *
   * In a desktop environment, if newTab is true, it will start 
   * a new 'about:blank' tab and change focus to this tab.
   *
   * This will also set the active messagemanager for this object
   *
   * @param boolean newTab
   *        If true, create new tab
   */
  startSession: function BO_startSession(newTab, win, callback) {
    if (appName != "Firefox") {
      callback(win, newTab);
    }
    else if (newTab) {
      this.addTab(this.startPage);
      //if we have a new tab, make it the selected tab
      this.browser.selectedTab = this.tab;
      let newTabBrowser = this.browser.getBrowserForTab(this.tab);
      // wait for tab to be loaded
      newTabBrowser.addEventListener("load", function onLoad() {
        newTabBrowser.removeEventListener("load", onLoad, true);
        callback(win, newTab);
      }, true);
    }
    else {
      //set this.tab to the currently focused tab
      if (this.browser != undefined && this.browser.selectedTab != undefined) {
        this.tab = this.browser.selectedTab;
      }
      callback(win, newTab);
    }
  },

  /**
   * Closes current tab
   */
  closeTab: function BO_closeTab() {
    if (this.tab != null && (appName != "B2G")) {
      this.browser.removeTab(this.tab);
      this.tab = null;
    }
  },

  /**
   * Opens a tab with given uri
   *
   * @param string uri
   *      URI to open
   */
  addTab: function BO_addTab(uri) {
    this.tab = this.browser.addTab(uri, true);
  },

  /**
   * Loads content listeners if we don't already have them
   *
   * @param string script
   *        path of script to load
   * @param nsIDOMWindow frame
   *        frame to load the script in
   */
  loadFrameScript: function BO_loadFrameScript(script, frame) {
    frame.window.messageManager.loadFrameScript(script, true);
    Services.prefs.setBoolPref("marionette.contentListener", true);
  },

  /**
   * Registers a new frame, and sets its current frame id to this frame
   * if it is not already assigned, and if a) we already have a session 
   * or b) we're starting a new session and it is the right start frame.
   *
   * @param string uid
   *        frame uid
   * @param string href
   *        frame's href 
   */
  register: function BO_register(uid, href) {
    if (this.curFrameId == null) {
      if ((!this.newSession) || (this.newSession && 
          ((appName != "Firefox") || href.indexOf(this.startPage) > -1))) {
        this.curFrameId = uid;
        this.mainContentId = uid;
      }
    }
    this.knownFrames.push(uid); //used to delete sessions
    return uid;
  },
}
