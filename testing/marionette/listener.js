/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

var uuidGen = Cc["@mozilla.org/uuid-generator;1"]
                .getService(Ci.nsIUUIDGenerator);

var loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
               .getService(Ci.mozIJSSubScriptLoader);

loader.loadSubScript("chrome://marionette/content/simpletest.js");
loader.loadSubScript("chrome://marionette/content/common.js");
loader.loadSubScript("chrome://marionette/content/actions.js");
Cu.import("chrome://marionette/content/capture.js");
Cu.import("chrome://marionette/content/elements.js");
Cu.import("chrome://marionette/content/error.js");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
var utils = {};
utils.window = content;
// Load Event/ChromeUtils for use with JS scripts:
loader.loadSubScript("chrome://marionette/content/EventUtils.js", utils);
loader.loadSubScript("chrome://marionette/content/ChromeUtils.js", utils);
loader.loadSubScript("chrome://marionette/content/atoms.js", utils);
loader.loadSubScript("chrome://marionette/content/sendkeys.js", utils);

var marionetteLogObj = new MarionetteLogObj();

var isB2G = false;

var marionetteTestName;
var winUtil = content.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIDOMWindowUtils);
var listenerId = null; // unique ID of this listener
var curContainer = { frame: content, shadowRoot: null };
var isRemoteBrowser = () => curContainer.frame.contentWindow !== null;
var previousContainer = null;
var elementManager = new ElementManager([]);
var accessibility = new Accessibility();
var actions = new ActionChain(utils, checkForInterrupted);
var importedScripts = null;

// Contains the last file input element that was the target of
// sendKeysToElement.
var fileInputElement;

// A dict of sandboxes used this session
var sandboxes = {};
// The name of the current sandbox
var sandboxName = 'default';

// the unload handler
var onunload;

// Flag to indicate whether an async script is currently running or not.
var asyncTestRunning = false;
var asyncTestCommandId;
var asyncTestTimeoutId;

var inactivityTimeoutId = null;
var heartbeatCallback = function () {}; // Called by the simpletest methods.

var originalOnError;
//timer for doc changes
var checkTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
//timer for readystate
var readyStateTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
// timer for navigation commands.
var navTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
var onDOMContentLoaded;
// Send move events about this often
var EVENT_INTERVAL = 30; // milliseconds
// last touch for each fingerId
var multiLast = {};

Cu.import("resource://gre/modules/Log.jsm");
var logger = Log.repository.getLogger("Marionette");
logger.info("loaded listener.js");
var modalHandler = function() {
  // This gets called on the system app only since it receives the mozbrowserprompt event
  sendSyncMessage("Marionette:switchedToFrame", { frameValue: null, storePrevious: true });
  let isLocal = sendSyncMessage("MarionetteFrame:handleModal", {})[0].value;
  if (isLocal) {
    previousContainer = curContainer;
  }
  curContainer = { frame: content, shadowRoot: null };
};

/**
 * Called when listener is first started up.
 * The listener sends its unique window ID and its current URI to the actor.
 * If the actor returns an ID, we start the listeners. Otherwise, nothing happens.
 */
function registerSelf() {
  let msg = {value: winUtil.outerWindowID};
  // register will have the ID and a boolean describing if this is the main process or not
  let register = sendSyncMessage("Marionette:register", msg);

  if (register[0]) {
    let {id, remotenessChange} = register[0][0];
    let {B2G, raisesAccessibilityExceptions} = register[0][2];
    isB2G = B2G;
    accessibility.strict = raisesAccessibilityExceptions;
    listenerId = id;
    if (typeof id != "undefined") {
      // check if we're the main process
      if (register[0][1] == true) {
        addMessageListener("MarionetteMainListener:emitTouchEvent", emitTouchEventForIFrame);
      }
      importedScripts = FileUtils.getDir('TmpD', [], false);
      importedScripts.append('marionetteContentScripts');
      startListeners();
      let rv = {};
      if (remotenessChange) {
        rv.listenerId = id;
      }
      sendAsyncMessage("Marionette:listenersAttached", rv);
    }
  }
}

function emitTouchEventForIFrame(message) {
  message = message.json;
  let identifier = actions.nextTouchId;

  let domWindowUtils = curContainer.frame.
    QueryInterface(Components.interfaces.nsIInterfaceRequestor).
    getInterface(Components.interfaces.nsIDOMWindowUtils);
  var ratio = domWindowUtils.screenPixelsPerCSSPixel;

  var typeForUtils;
  switch (message.type) {
    case 'touchstart':
      typeForUtils = domWindowUtils.TOUCH_CONTACT;
      break;
    case 'touchend':
      typeForUtils = domWindowUtils.TOUCH_REMOVE;
      break;
    case 'touchcancel':
      typeForUtils = domWindowUtils.TOUCH_CANCEL;
      break;
    case 'touchmove':
      typeForUtils = domWindowUtils.TOUCH_CONTACT;
      break;
  }
  domWindowUtils.sendNativeTouchPoint(identifier, typeForUtils,
    Math.round(message.screenX * ratio), Math.round(message.screenY * ratio),
    message.force, 90);
}

// Eventually we will not have a closure for every single command, but
// use a generic dispatch for all listener commands.
//
// Perhaps one could even conceive having a separate instance of
// CommandProcessor for the listener, because the code is mostly the same.
function dispatch(fn) {
  return function(msg) {
    let id = msg.json.command_id;

    let req = Task.spawn(function*() {
      if (typeof msg.json == "undefined" || msg.json instanceof Array) {
        return yield fn.apply(null, msg.json);
      } else {
        return yield fn(msg.json);
      }
    });

    let okOrValueResponse = rv => {
      if (typeof rv == "undefined") {
        sendOk(id);
      } else {
        sendResponse({value: rv}, id);
      }
    };

    req.then(okOrValueResponse, err => sendError(err, id))
        .catch(error.report);
  };
}

/**
 * Add a message listener that's tied to our listenerId.
 */
function addMessageListenerId(messageName, handler) {
  addMessageListener(messageName + listenerId, handler);
}

/**
 * Remove a message listener that's tied to our listenerId.
 */
function removeMessageListenerId(messageName, handler) {
  removeMessageListener(messageName + listenerId, handler);
}

var getTitleFn = dispatch(getTitle);
var getPageSourceFn = dispatch(getPageSource);
var getActiveElementFn = dispatch(getActiveElement);
var clickElementFn = dispatch(clickElement);
var goBackFn = dispatch(goBack);
var getElementAttributeFn = dispatch(getElementAttribute);
var getElementTextFn = dispatch(getElementText);
var getElementTagNameFn = dispatch(getElementTagName);
var getElementRectFn = dispatch(getElementRect);
var isElementEnabledFn = dispatch(isElementEnabled);
var getCurrentUrlFn = dispatch(getCurrentUrl);
var findElementContentFn = dispatch(findElementContent);
var findElementsContentFn = dispatch(findElementsContent);
var isElementSelectedFn = dispatch(isElementSelected);
var clearElementFn = dispatch(clearElement);
var isElementDisplayedFn = dispatch(isElementDisplayed);
var getElementValueOfCssPropertyFn = dispatch(getElementValueOfCssProperty);
var switchToShadowRootFn = dispatch(switchToShadowRoot);
var getCookiesFn = dispatch(getCookies);
var singleTapFn = dispatch(singleTap);
var takeScreenshotFn = dispatch(takeScreenshot);

/**
 * Start all message listeners
 */
function startListeners() {
  addMessageListenerId("Marionette:receiveFiles", receiveFiles);
  addMessageListenerId("Marionette:newSession", newSession);
  addMessageListenerId("Marionette:executeScript", executeScript);
  addMessageListenerId("Marionette:executeAsyncScript", executeAsyncScript);
  addMessageListenerId("Marionette:executeJSScript", executeJSScript);
  addMessageListenerId("Marionette:singleTap", singleTapFn);
  addMessageListenerId("Marionette:actionChain", actionChain);
  addMessageListenerId("Marionette:multiAction", multiAction);
  addMessageListenerId("Marionette:get", get);
  addMessageListenerId("Marionette:pollForReadyState", pollForReadyState);
  addMessageListenerId("Marionette:cancelRequest", cancelRequest);
  addMessageListenerId("Marionette:getCurrentUrl", getCurrentUrlFn);
  addMessageListenerId("Marionette:getTitle", getTitleFn);
  addMessageListenerId("Marionette:getPageSource", getPageSourceFn);
  addMessageListenerId("Marionette:goBack", goBackFn);
  addMessageListenerId("Marionette:goForward", goForward);
  addMessageListenerId("Marionette:refresh", refresh);
  addMessageListenerId("Marionette:findElementContent", findElementContentFn);
  addMessageListenerId("Marionette:findElementsContent", findElementsContentFn);
  addMessageListenerId("Marionette:getActiveElement", getActiveElementFn);
  addMessageListenerId("Marionette:clickElement", clickElementFn);
  addMessageListenerId("Marionette:getElementAttribute", getElementAttributeFn);
  addMessageListenerId("Marionette:getElementText", getElementTextFn);
  addMessageListenerId("Marionette:getElementTagName", getElementTagNameFn);
  addMessageListenerId("Marionette:isElementDisplayed", isElementDisplayedFn);
  addMessageListenerId("Marionette:getElementValueOfCssProperty", getElementValueOfCssPropertyFn);
  addMessageListenerId("Marionette:getElementRect", getElementRectFn);
  addMessageListenerId("Marionette:isElementEnabled", isElementEnabledFn);
  addMessageListenerId("Marionette:isElementSelected", isElementSelectedFn);
  addMessageListenerId("Marionette:sendKeysToElement", sendKeysToElement);
  addMessageListenerId("Marionette:clearElement", clearElementFn);
  addMessageListenerId("Marionette:switchToFrame", switchToFrame);
  addMessageListenerId("Marionette:switchToParentFrame", switchToParentFrame);
  addMessageListenerId("Marionette:switchToShadowRoot", switchToShadowRootFn);
  addMessageListenerId("Marionette:deleteSession", deleteSession);
  addMessageListenerId("Marionette:sleepSession", sleepSession);
  addMessageListenerId("Marionette:emulatorCmdResult", emulatorCmdResult);
  addMessageListenerId("Marionette:importScript", importScript);
  addMessageListenerId("Marionette:getAppCacheStatus", getAppCacheStatus);
  addMessageListenerId("Marionette:setTestName", setTestName);
  addMessageListenerId("Marionette:takeScreenshot", takeScreenshotFn);
  addMessageListenerId("Marionette:addCookie", addCookie);
  addMessageListenerId("Marionette:getCookies", getCookiesFn);
  addMessageListenerId("Marionette:deleteAllCookies", deleteAllCookies);
  addMessageListenerId("Marionette:deleteCookie", deleteCookie);
}

/**
 * Used during newSession and restart, called to set up the modal dialog listener in b2g
 */
function waitForReady() {
  if (content.document.readyState == 'complete') {
    readyStateTimer.cancel();
    content.addEventListener("mozbrowsershowmodalprompt", modalHandler, false);
    content.addEventListener("unload", waitForReady, false);
  }
  else {
    readyStateTimer.initWithCallback(waitForReady, 100, Ci.nsITimer.TYPE_ONE_SHOT);
  }
}

/**
 * Called when we start a new session. It registers the
 * current environment, and resets all values
 */
function newSession(msg) {
  isB2G = msg.json.B2G;
  accessibility.strict = msg.json.raisesAccessibilityExceptions;
  resetValues();
  if (isB2G) {
    readyStateTimer.initWithCallback(waitForReady, 100, Ci.nsITimer.TYPE_ONE_SHOT);
    // We have to set correct mouse event source to MOZ_SOURCE_TOUCH
    // to offer a way for event listeners to differentiate
    // events being the result of a physical mouse action.
    // This is especially important for the touch event shim,
    // in order to prevent creating touch event for these fake mouse events.
    actions.inputSource = Ci.nsIDOMMouseEvent.MOZ_SOURCE_TOUCH;
  }
}

/**
 * Puts the current session to sleep, so all listeners are removed except
 * for the 'restart' listener. This is used to keep the content listener
 * alive for reuse in B2G instead of reloading it each time.
 */
function sleepSession(msg) {
  deleteSession();
  addMessageListener("Marionette:restart", restart);
}

/**
 * Restarts all our listeners after this listener was put to sleep
 */
function restart(msg) {
  removeMessageListener("Marionette:restart", restart);
  if (isB2G) {
    readyStateTimer.initWithCallback(waitForReady, 100, Ci.nsITimer.TYPE_ONE_SHOT);
  }
  registerSelf();
}

/**
 * Removes all listeners
 */
function deleteSession(msg) {
  removeMessageListenerId("Marionette:receiveFiles", receiveFiles);
  removeMessageListenerId("Marionette:newSession", newSession);
  removeMessageListenerId("Marionette:executeScript", executeScript);
  removeMessageListenerId("Marionette:executeAsyncScript", executeAsyncScript);
  removeMessageListenerId("Marionette:executeJSScript", executeJSScript);
  removeMessageListenerId("Marionette:singleTap", singleTapFn);
  removeMessageListenerId("Marionette:actionChain", actionChain);
  removeMessageListenerId("Marionette:multiAction", multiAction);
  removeMessageListenerId("Marionette:get", get);
  removeMessageListenerId("Marionette:pollForReadyState", pollForReadyState);
  removeMessageListenerId("Marionette:cancelRequest", cancelRequest);
  removeMessageListenerId("Marionette:getTitle", getTitleFn);
  removeMessageListenerId("Marionette:getPageSource", getPageSourceFn);
  removeMessageListenerId("Marionette:getCurrentUrl", getCurrentUrlFn);
  removeMessageListenerId("Marionette:goBack", goBackFn);
  removeMessageListenerId("Marionette:goForward", goForward);
  removeMessageListenerId("Marionette:refresh", refresh);
  removeMessageListenerId("Marionette:findElementContent", findElementContentFn);
  removeMessageListenerId("Marionette:findElementsContent", findElementsContentFn);
  removeMessageListenerId("Marionette:getActiveElement", getActiveElementFn);
  removeMessageListenerId("Marionette:clickElement", clickElementFn);
  removeMessageListenerId("Marionette:getElementAttribute", getElementAttributeFn);
  removeMessageListenerId("Marionette:getElementText", getElementTextFn);
  removeMessageListenerId("Marionette:getElementTagName", getElementTagNameFn);
  removeMessageListenerId("Marionette:isElementDisplayed", isElementDisplayedFn);
  removeMessageListenerId("Marionette:getElementValueOfCssProperty", getElementValueOfCssPropertyFn);
  removeMessageListenerId("Marionette:getElementRect", getElementRectFn);
  removeMessageListenerId("Marionette:isElementEnabled", isElementEnabledFn);
  removeMessageListenerId("Marionette:isElementSelected", isElementSelectedFn);
  removeMessageListenerId("Marionette:sendKeysToElement", sendKeysToElement);
  removeMessageListenerId("Marionette:clearElement", clearElementFn);
  removeMessageListenerId("Marionette:switchToFrame", switchToFrame);
  removeMessageListenerId("Marionette:switchToParentFrame", switchToParentFrame);
  removeMessageListenerId("Marionette:switchToShadowRoot", switchToShadowRootFn);
  removeMessageListenerId("Marionette:deleteSession", deleteSession);
  removeMessageListenerId("Marionette:sleepSession", sleepSession);
  removeMessageListenerId("Marionette:emulatorCmdResult", emulatorCmdResult);
  removeMessageListenerId("Marionette:importScript", importScript);
  removeMessageListenerId("Marionette:getAppCacheStatus", getAppCacheStatus);
  removeMessageListenerId("Marionette:setTestName", setTestName);
  removeMessageListenerId("Marionette:takeScreenshot", takeScreenshotFn);
  removeMessageListenerId("Marionette:addCookie", addCookie);
  removeMessageListenerId("Marionette:getCookies", getCookiesFn);
  removeMessageListenerId("Marionette:deleteAllCookies", deleteAllCookies);
  removeMessageListenerId("Marionette:deleteCookie", deleteCookie);
  if (isB2G) {
    content.removeEventListener("mozbrowsershowmodalprompt", modalHandler, false);
  }
  elementManager.reset();
  // reset container frame to the top-most frame
  curContainer = { frame: content, shadowRoot: null };
  curContainer.frame.focus();
  actions.touchIds = {};
}

/*
 * Helper methods
 */

/**
 * Generic method to send a message to the server
 */
function sendToServer(name, data, objs, id) {
  if (!data) {
    data = {}
  }
  if (id) {
    data.command_id = id;
  }
  sendAsyncMessage(name, data, objs);
}

/**
 * Send response back to server
 */
function sendResponse(value, command_id) {
  sendToServer("Marionette:done", value, null, command_id);
}

/**
 * Send ack back to server
 */
function sendOk(command_id) {
  sendToServer("Marionette:ok", null, null, command_id);
}

/**
 * Send log message to server
 */
function sendLog(msg) {
  sendToServer("Marionette:log", {message: msg});
}

/**
 * Send error message to server
 */
function sendError(err, cmdId) {
  sendToServer("Marionette:error", null, {error: err}, cmdId);
}

/**
 * Clear test values after completion of test
 */
function resetValues() {
  sandboxes = {};
  curContainer = { frame: content, shadowRoot: null };
  actions.mouseEventsOnly = false;
}

/**
 * Dump a logline to stdout. Prepends logline with a timestamp.
 */
function dumpLog(logline) {
  dump(Date.now() + " Marionette: " + logline);
}

/**
 * Check if our context was interrupted
 */
function wasInterrupted() {
  if (previousContainer) {
    let element = content.document.elementFromPoint((content.innerWidth/2), (content.innerHeight/2));
    if (element.id.indexOf("modal-dialog") == -1) {
      return true;
    }
    else {
      return false;
    }
  }
  return sendSyncMessage("MarionetteFrame:getInterruptedState", {})[0].value;
}

function checkForInterrupted() {
    if (wasInterrupted()) {
      if (previousContainer) {
        // if previousContainer is set, then we're in a single process environment
        curContainer = actions.container = previousContainer;
        previousContainer = null;
      }
      else {
        //else we're in OOP environment, so we'll switch to the original OOP frame
        sendSyncMessage("Marionette:switchToModalOrigin");
      }
      sendSyncMessage("Marionette:switchedToFrame", { restorePrevious: true });
    }
}

/*
 * Marionette Methods
 */

/**
 * Returns a content sandbox that can be used by the execute_foo functions.
 */
function createExecuteContentSandbox(win, timeout) {
  let mn = new Marionette(
      win,
      "content",
      marionetteLogObj,
      timeout,
      heartbeatCallback,
      marionetteTestName);

  let principal = win;
  if (sandboxName == "system") {
    principal = Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal);
  }
  let sandbox = new Cu.Sandbox(principal, {sandboxPrototype: win});
  sandbox.global = sandbox;
  sandbox.window = win;
  sandbox.document = sandbox.window.document;
  sandbox.navigator = sandbox.window.navigator;
  sandbox.testUtils = utils;
  sandbox.asyncTestCommandId = asyncTestCommandId;
  sandbox.marionette = mn;

  mn.exports.forEach(fn => {
    if (typeof mn[fn] == "function") {
      sandbox[fn] = mn[fn].bind(mn);
    } else {
      sandbox[fn] = mn[fn];
    }
  });
  sandbox.runEmulatorCmd = (cmd, cb) => this.runEmulatorCmd(cmd, cb);
  sandbox.runEmulatorShell = (args, cb) => this.runEmulatorShell(args, cb);

  sandbox.asyncComplete = (obj, id) => {
    if (id == asyncTestCommandId) {
      curContainer.frame.removeEventListener("unload", onunload, false);
      curContainer.frame.clearTimeout(asyncTestTimeoutId);

      if (inactivityTimeoutId != null) {
        curContainer.frame.clearTimeout(inactivityTimeoutId);
      }

      sendSyncMessage("Marionette:shareData",
          {log: elementManager.wrapValue(marionetteLogObj.getLogs())});
      marionetteLogObj.clearLogs();

      if (error.isError(obj)) {
        sendError(obj, id);
      } else {
        if (Object.keys(_emu_cbs).length) {
          _emu_cbs = {};
          sendError(new WebDriverError("Emulator callback still pending when finish() called"), id);
        } else {
          sendResponse({value: elementManager.wrapValue(obj)}, id);
        }
      }

      asyncTestRunning = false;
      asyncTestTimeoutId = undefined;
      asyncTestCommandId = undefined;
      inactivityTimeoutId = null;
    }
  };

  sandbox.finish = function() {
    if (asyncTestRunning) {
      sandbox.asyncComplete(mn.generate_results(), sandbox.asyncTestCommandId);
    } else {
      return mn.generate_results();
    }
  };
  sandbox.marionetteScriptFinished = val =>
      sandbox.asyncComplete(val, sandbox.asyncTestCommandId);

  sandboxes[sandboxName] = sandbox;
}

/**
 * Execute the given script either as a function body (executeScript)
 * or directly (for mochitest like JS Marionette tests).
 */
function executeScript(msg, directInject) {
  // Set up inactivity timeout.
  if (msg.json.inactivityTimeout) {
    let setTimer = function() {
      inactivityTimeoutId = curContainer.frame.setTimeout(function() {
        sendError(new ScriptTimeoutError("timed out due to inactivity"), asyncTestCommandId);
      }, msg.json.inactivityTimeout);
   };

    setTimer();
    heartbeatCallback = function() {
      curContainer.frame.clearTimeout(inactivityTimeoutId);
      setTimer();
    };
  }

  asyncTestCommandId = msg.json.command_id;
  let script = msg.json.script;
  let filename = msg.json.filename;
  sandboxName = msg.json.sandboxName;

  if (msg.json.newSandbox ||
      !(sandboxName in sandboxes) ||
      (sandboxes[sandboxName].window != curContainer.frame)) {
    createExecuteContentSandbox(curContainer.frame, msg.json.timeout);
    if (!sandboxes[sandboxName]) {
      sendError(new WebDriverError("Could not create sandbox!"), asyncTestCommandId);
      return;
    }
  } else {
    sandboxes[sandboxName].asyncTestCommandId = asyncTestCommandId;
  }

  let sandbox = sandboxes[sandboxName];

  try {
    if (directInject) {
      if (importedScripts.exists()) {
        let stream = Components.classes["@mozilla.org/network/file-input-stream;1"].
                      createInstance(Components.interfaces.nsIFileInputStream);
        stream.init(importedScripts, -1, 0, 0);
        let data = NetUtil.readInputStreamToString(stream, stream.available());
        stream.close();
        script = data + script;
      }
      let res = Cu.evalInSandbox(script, sandbox, "1.8", filename ? filename : "dummy file" ,0);
      sendSyncMessage("Marionette:shareData",
                      {log: elementManager.wrapValue(marionetteLogObj.getLogs())});
      marionetteLogObj.clearLogs();

      if (res == undefined || res.passed == undefined) {
        sendError(new JavaScriptError("Marionette.finish() not called"), asyncTestCommandId);
      }
      else {
        sendResponse({value: elementManager.wrapValue(res)}, asyncTestCommandId);
      }
    }
    else {
      try {
        sandbox.__marionetteParams = Cu.cloneInto(elementManager.convertWrappedArguments(
          msg.json.args, curContainer), sandbox, { wrapReflectors: true });
      } catch (e) {
        sendError(e, asyncTestCommandId);
        return;
      }

      script = "var __marionetteFunc = function(){" + script + "};" +
                   "__marionetteFunc.apply(null, __marionetteParams);";
      if (importedScripts.exists()) {
        let stream = Components.classes["@mozilla.org/network/file-input-stream;1"].
                      createInstance(Components.interfaces.nsIFileInputStream);
        stream.init(importedScripts, -1, 0, 0);
        let data = NetUtil.readInputStreamToString(stream, stream.available());
        stream.close();
        script = data + script;
      }
      let res = Cu.evalInSandbox(script, sandbox, "1.8", filename ? filename : "dummy file", 0);
      sendSyncMessage("Marionette:shareData",
                      {log: elementManager.wrapValue(marionetteLogObj.getLogs())});
      marionetteLogObj.clearLogs();
      sendResponse({value: elementManager.wrapValue(res)}, asyncTestCommandId);
    }
  } catch (e) {
    let err = new JavaScriptError(
        e,
        "execute_script",
        msg.json.filename,
        msg.json.line,
        script);
    sendError(err, asyncTestCommandId);
  }
}

/**
 * Sets the test name, used in logging messages.
 */
function setTestName(msg) {
  marionetteTestName = msg.json.value;
  sendOk(msg.json.command_id);
}

/**
 * Execute async script
 */
function executeAsyncScript(msg) {
  executeWithCallback(msg);
}

/**
 * Receive file objects from chrome in order to complete a
 * sendKeysToElement action on a file input element.
 */
function receiveFiles(msg) {
  if ('error' in msg.json) {
    let err = new InvalidArgumentError(msg.json.error);
    sendError(err, msg.json.command_id);
    return;
  }
  if (!fileInputElement) {
    let err = new InvalidElementStateError("receiveFiles called with no valid fileInputElement");
    sendError(err, msg.json.command_id);
    return;
  }
  let fs = Array.prototype.slice.call(fileInputElement.files);
  fs.push(msg.json.file);
  fileInputElement.mozSetFileArray(fs);
  fileInputElement = null;
  sendOk(msg.json.command_id);
}

/**
 * Execute pure JS test. Handles both async and sync cases.
 */
function executeJSScript(msg) {
  if (msg.json.async) {
    executeWithCallback(msg, msg.json.async);
  }
  else {
    executeScript(msg, true);
  }
}

/**
 * This function is used by executeAsync and executeJSScript to execute a script
 * in a sandbox.
 *
 * For executeJSScript, it will return a message only when the finish() method is called.
 * For executeAsync, it will return a response when marionetteScriptFinished/arguments[arguments.length-1]
 * method is called, or if it times out.
 */
function executeWithCallback(msg, useFinish) {
  // Set up inactivity timeout.
  if (msg.json.inactivityTimeout) {
    let setTimer = function() {
      inactivityTimeoutId = curContainer.frame.setTimeout(function() {
        sandbox.asyncComplete(new ScriptTimeoutError("timed out due to inactivity"), asyncTestCommandId);
      }, msg.json.inactivityTimeout);
    };

    setTimer();
    heartbeatCallback = function() {
      curContainer.frame.clearTimeout(inactivityTimeoutId);
      setTimer();
    };
  }

  let script = msg.json.script;
  let filename = msg.json.filename;
  asyncTestCommandId = msg.json.command_id;
  sandboxName = msg.json.sandboxName;

  onunload = function() {
    sendError(new JavaScriptError("unload was called"), asyncTestCommandId);
  };
  curContainer.frame.addEventListener("unload", onunload, false);

  if (msg.json.newSandbox ||
      !(sandboxName in sandboxes) ||
      (sandboxes[sandboxName].window != curContainer.frame)) {
    createExecuteContentSandbox(curContainer.frame, msg.json.timeout);
    if (!sandboxes[sandboxName]) {
      sendError(new JavaScriptError("Could not create sandbox!"), asyncTestCommandId);
      return;
    }
  }
  else {
    sandboxes[sandboxName].asyncTestCommandId = asyncTestCommandId;
  }
  let sandbox = sandboxes[sandboxName];
  sandbox.tag = script;

  asyncTestTimeoutId = curContainer.frame.setTimeout(function() {
    sandbox.asyncComplete(new ScriptTimeoutError("timed out"), asyncTestCommandId);
  }, msg.json.timeout);

  originalOnError = curContainer.frame.onerror;
  curContainer.frame.onerror = function errHandler(msg, url, line) {
    sandbox.asyncComplete(new JavaScriptError(msg + "@" + url + ", line " + line), asyncTestCommandId);
    curContainer.frame.onerror = originalOnError;
  };

  let scriptSrc;
  if (useFinish) {
    if (msg.json.timeout == null || msg.json.timeout == 0) {
      sendError(new TimeoutError("Please set a timeout"), asyncTestCommandId);
    }
    scriptSrc = script;
  }
  else {
    try {
      sandbox.__marionetteParams = Cu.cloneInto(elementManager.convertWrappedArguments(
        msg.json.args, curContainer), sandbox, { wrapReflectors: true });
    } catch (e) {
      sendError(e, asyncTestCommandId);
      return;
    }

    scriptSrc = "__marionetteParams.push(marionetteScriptFinished);" +
                "var __marionetteFunc = function() { " + script + "};" +
                "__marionetteFunc.apply(null, __marionetteParams); ";
  }

  try {
    asyncTestRunning = true;
    if (importedScripts.exists()) {
      let stream = Cc["@mozilla.org/network/file-input-stream;1"].
                      createInstance(Ci.nsIFileInputStream);
      stream.init(importedScripts, -1, 0, 0);
      let data = NetUtil.readInputStreamToString(stream, stream.available());
      stream.close();
      scriptSrc = data + scriptSrc;
    }
    Cu.evalInSandbox(scriptSrc, sandbox, "1.8", filename ? filename : "dummy file", 0);
  } catch (e) {
    let err = new JavaScriptError(
        e,
        "execute_async_script",
        msg.json.filename,
        msg.json.line,
        scriptSrc);
    sandbox.asyncComplete(err, asyncTestCommandId);
  }
}

/**
 * This function creates a touch event given a touch type and a touch
 */
function emitTouchEvent(type, touch) {
  if (!wasInterrupted()) {
    let loggingInfo = "emitting Touch event of type " + type + " to element with id: " + touch.target.id + " and tag name: " + touch.target.tagName + " at coordinates (" + touch.clientX + ", " + touch.clientY + ") relative to the viewport";
    dumpLog(loggingInfo);
    var docShell = curContainer.frame.document.defaultView.
                   QueryInterface(Components.interfaces.nsIInterfaceRequestor).
                   getInterface(Components.interfaces.nsIWebNavigation).
                   QueryInterface(Components.interfaces.nsIDocShell);
    if (docShell.asyncPanZoomEnabled && actions.scrolling) {
      // if we're in APZ and we're scrolling, we must use injectTouchEvent to dispatch our touchmove events
      let index = sendSyncMessage("MarionetteFrame:getCurrentFrameId");
      // only call emitTouchEventForIFrame if we're inside an iframe.
      if (index != null) {
        sendSyncMessage("Marionette:emitTouchEvent",
          { index: index, type: type, id: touch.identifier,
            clientX: touch.clientX, clientY: touch.clientY,
            screenX: touch.screenX, screenY: touch.screenY,
            radiusX: touch.radiusX, radiusY: touch.radiusY,
            rotation: touch.rotationAngle, force: touch.force });
        return;
      }
    }
    // we get here if we're not in asyncPacZoomEnabled land, or if we're the main process
    /*
    Disabled per bug 888303
    marionetteLogObj.log(loggingInfo, "TRACE");
    sendSyncMessage("Marionette:shareData",
                    {log: elementManager.wrapValue(marionetteLogObj.getLogs())});
    marionetteLogObj.clearLogs();
    */
    let domWindowUtils = curContainer.frame.QueryInterface(Components.interfaces.nsIInterfaceRequestor).getInterface(Components.interfaces.nsIDOMWindowUtils);
    domWindowUtils.sendTouchEvent(type, [touch.identifier], [touch.clientX], [touch.clientY], [touch.radiusX], [touch.radiusY], [touch.rotationAngle], [touch.force], 1, 0);
  }
}

/**
 * This function generates a pair of coordinates relative to the viewport given a
 * target element and coordinates relative to that element's top-left corner.
 * @param 'x', and 'y' are the relative to the target.
 *        If they are not specified, then the center of the target is used.
 */
function coordinates(target, x, y) {
  let box = target.getBoundingClientRect();
  if (x == null) {
    x = box.width / 2;
  }
  if (y == null) {
    y = box.height / 2;
  }
  let coords = {};
  coords.x = box.left + x;
  coords.y = box.top + y;
  return coords;
}


/**
 * This function returns true if the given coordinates are in the viewport.
 * @param 'x', and 'y' are the coordinates relative to the target.
 *        If they are not specified, then the center of the target is used.
 */
function elementInViewport(el, x, y) {
  let c = coordinates(el, x, y);
  let curFrame = curContainer.frame;
  let viewPort = {top: curFrame.pageYOffset,
                  left: curFrame.pageXOffset,
                  bottom: (curFrame.pageYOffset + curFrame.innerHeight),
                  right:(curFrame.pageXOffset + curFrame.innerWidth)};
  return (viewPort.left <= c.x + curFrame.pageXOffset &&
          c.x + curFrame.pageXOffset <= viewPort.right &&
          viewPort.top <= c.y + curFrame.pageYOffset &&
          c.y + curFrame.pageYOffset <= viewPort.bottom);
}

/**
 * This function throws the visibility of the element error if the element is
 * not displayed or the given coordinates are not within the viewport.
 * @param 'x', and 'y' are the coordinates relative to the target.
 *        If they are not specified, then the center of the target is used.
 */
function checkVisible(el, x, y) {
  // Bug 1094246 - Webdriver's isShown doesn't work with content xul
  if (utils.getElementAttribute(el, "namespaceURI").indexOf("there.is.only.xul") == -1) {
    //check if the element is visible
    let visible = utils.isElementDisplayed(el);
    if (!visible) {
      return false;
    }
  }

  if (el.tagName.toLowerCase() === 'body') {
    return true;
  }
  if (!elementInViewport(el, x, y)) {
    //check if scroll function exist. If so, call it.
    if (el.scrollIntoView) {
      el.scrollIntoView(false);
      if (!elementInViewport(el)) {
        return false;
      }
    }
    else {
      return false;
    }
  }
  return true;
}


/**
 * Function that perform a single tap
 */
function singleTap(id, corx, cory) {
  let el = elementManager.getKnownElement(id, curContainer);
  // after this block, the element will be scrolled into view
  let visible = checkVisible(el, corx, cory);
  if (!visible) {
    throw new ElementNotVisibleError("Element is not currently visible and may not be manipulated");
  }
  return accessibility.getAccessibleObject(el, true).then(acc => {
    checkVisibleAccessibility(acc, el, visible);
    checkActionableAccessibility(acc, el);
    if (!curContainer.frame.document.createTouch) {
      actions.mouseEventsOnly = true;
    }
    let c = coordinates(el, corx, cory);
    if (!actions.mouseEventsOnly) {
      let touchId = actions.nextTouchId++;
      let touch = createATouch(el, c.x, c.y, touchId);
      emitTouchEvent('touchstart', touch);
      emitTouchEvent('touchend', touch);
    }
    actions.mouseTap(el.ownerDocument, c.x, c.y);
  });
}

/**
 * Check if the element's unavailable accessibility state matches the enabled
 * state
 * @param nsIAccessible object
 * @param WebElement corresponding to nsIAccessible object
 * @param Boolean enabled element's enabled state
 */
function checkEnabledAccessibility(accesible, element, enabled) {
  if (!accesible) {
    return;
  }
  let disabledAccessibility = accessibility.matchState(
    accesible, 'STATE_UNAVAILABLE');
  let explorable = curContainer.frame.document.defaultView.getComputedStyle(
    element, null).getPropertyValue('pointer-events') !== 'none';
  let message;

  if (!explorable && !disabledAccessibility) {
    message = 'Element is enabled but is not explorable via the ' +
      'accessibility API';
  } else if (enabled && disabledAccessibility) {
    message = 'Element is enabled but disabled via the accessibility API';
  } else if (!enabled && !disabledAccessibility) {
    message = 'Element is disabled but enabled via the accessibility API';
  }
  accessibility.handleErrorMessage(message, element);
}

/**
 * Check if the element's visible state corresponds to its accessibility API
 * visibility
 * @param nsIAccessible object
 * @param WebElement corresponding to nsIAccessible object
 * @param Boolean visible element's visibility state
 */
function checkVisibleAccessibility(accesible, element, visible) {
  if (!accesible) {
    return;
  }
  let hiddenAccessibility = accessibility.isHidden(accesible);
  let message;
  if (visible && hiddenAccessibility) {
    message = 'Element is not currently visible via the accessibility API ' +
      'and may not be manipulated by it';
  } else if (!visible && !hiddenAccessibility) {
    message = 'Element is currently only visible via the accessibility API ' +
      'and can be manipulated by it';
  }
  accessibility.handleErrorMessage(message, element);
}

/**
 * Check if it is possible to activate an element with the accessibility API
 * @param nsIAccessible object
 * @param WebElement corresponding to nsIAccessible object
 */
function checkActionableAccessibility(accesible, element) {
  if (!accesible) {
    return;
  }
  let message;
  if (!accessibility.hasActionCount(accesible)) {
    message = 'Element does not support any accessible actions';
  } else if (!accessibility.isActionableRole(accesible)) {
    message = 'Element does not have a correct accessibility role ' +
      'and may not be manipulated via the accessibility API';
  } else if (!accessibility.hasValidName(accesible)) {
    message = 'Element is missing an accesible name';
  } else if (!accessibility.matchState(accesible, 'STATE_FOCUSABLE')) {
    message = 'Element is not focusable via the accessibility API';
  }
  accessibility.handleErrorMessage(message, element);
}

/**
 * Check if element's selected state corresponds to its accessibility API
 * selected state.
 * @param nsIAccessible object
 * @param WebElement corresponding to nsIAccessible object
 * @param Boolean selected element's selected state
 */
function checkSelectedAccessibility(accessible, element, selected) {
  if (!accessible) {
    return;
  }
  if (!accessibility.matchState(accessible, 'STATE_SELECTABLE')) {
    // Element is not selectable via the accessibility API
    return;
  }

  let selectedAccessibility = accessibility.matchState(
    accessible, 'STATE_SELECTED');
  let message;
  if (selected && !selectedAccessibility) {
    message = 'Element is selected but not selected via the accessibility API';
  } else if (!selected && selectedAccessibility) {
    message = 'Element is not selected but selected via the accessibility API';
  }
  accessibility.handleErrorMessage(message, element);
}


/**
 * Function to create a touch based on the element
 * corx and cory are relative to the viewport, id is the touchId
 */
function createATouch(el, corx, cory, touchId) {
  let doc = el.ownerDocument;
  let win = doc.defaultView;
  let [clientX, clientY, pageX, pageY, screenX, screenY] =
    actions.getCoordinateInfo(el, corx, cory);
  let atouch = doc.createTouch(win, el, touchId, pageX, pageY, screenX, screenY, clientX, clientY);
  return atouch;
}

/**
 * Function to start action chain on one finger
 */
function actionChain(msg) {
  let command_id = msg.json.command_id;
  let args = msg.json.chain;
  let touchId = msg.json.nextId;

  let callbacks = {};
  callbacks.onSuccess = value => sendResponse(value, command_id);
  callbacks.onError = err => sendError(err, command_id);

  let touchProvider = {};
  touchProvider.createATouch = createATouch;
  touchProvider.emitTouchEvent = emitTouchEvent;

  try {
    actions.dispatchActions(
        args,
        touchId,
        curContainer,
        elementManager,
        callbacks,
        touchProvider);
  } catch (e) {
    sendError(e, command_id);
  }
}

/**
 * Function to emit touch events which allow multi touch on the screen
 * @param type represents the type of event, touch represents the current touch,touches are all pending touches
 */
function emitMultiEvents(type, touch, touches) {
  let target = touch.target;
  let doc = target.ownerDocument;
  let win = doc.defaultView;
  // touches that are in the same document
  let documentTouches = doc.createTouchList(touches.filter(function(t) {
    return ((t.target.ownerDocument === doc) && (type != 'touchcancel'));
  }));
  // touches on the same target
  let targetTouches = doc.createTouchList(touches.filter(function(t) {
    return ((t.target === target) && ((type != 'touchcancel') || (type != 'touchend')));
  }));
  // Create changed touches
  let changedTouches = doc.createTouchList(touch);
  // Create the event object
  let event = doc.createEvent('TouchEvent');
  event.initTouchEvent(type,
                       true,
                       true,
                       win,
                       0,
                       false, false, false, false,
                       documentTouches,
                       targetTouches,
                       changedTouches);
  target.dispatchEvent(event);
}

/**
 * Function to dispatch one set of actions
 * @param touches represents all pending touches, batchIndex represents the batch we are dispatching right now
 */
function setDispatch(batches, touches, command_id, batchIndex) {
  if (typeof batchIndex === "undefined") {
    batchIndex = 0;
  }
  // check if all the sets have been fired
  if (batchIndex >= batches.length) {
    multiLast = {};
    sendOk(command_id);
    return;
  }
  // a set of actions need to be done
  let batch = batches[batchIndex];
  // each action for some finger
  let pack;
  // the touch id for the finger (pack)
  let touchId;
  // command for the finger
  let command;
  // touch that will be created for the finger
  let el;
  let corx;
  let cory;
  let touch;
  let lastTouch;
  let touchIndex;
  let waitTime = 0;
  let maxTime = 0;
  let c;
  batchIndex++;
  // loop through the batch
  for (let i = 0; i < batch.length; i++) {
    pack = batch[i];
    touchId = pack[0];
    command = pack[1];
    switch (command) {
      case 'press':
        el = elementManager.getKnownElement(pack[2], curContainer);
        c = coordinates(el, pack[3], pack[4]);
        touch = createATouch(el, c.x, c.y, touchId);
        multiLast[touchId] = touch;
        touches.push(touch);
        emitMultiEvents('touchstart', touch, touches);
        break;
      case 'release':
        touch = multiLast[touchId];
        // the index of the previous touch for the finger may change in the touches array
        touchIndex = touches.indexOf(touch);
        touches.splice(touchIndex, 1);
        emitMultiEvents('touchend', touch, touches);
        break;
      case 'move':
        el = elementManager.getKnownElement(pack[2], curContainer);
        c = coordinates(el);
        touch = createATouch(multiLast[touchId].target, c.x, c.y, touchId);
        touchIndex = touches.indexOf(lastTouch);
        touches[touchIndex] = touch;
        multiLast[touchId] = touch;
        emitMultiEvents('touchmove', touch, touches);
        break;
      case 'moveByOffset':
        el = multiLast[touchId].target;
        lastTouch = multiLast[touchId];
        touchIndex = touches.indexOf(lastTouch);
        let doc = el.ownerDocument;
        let win = doc.defaultView;
        // since x and y are relative to the last touch, therefore, it's relative to the position of the last touch
        let clientX = lastTouch.clientX + pack[2],
            clientY = lastTouch.clientY + pack[3];
        let pageX = clientX + win.pageXOffset,
            pageY = clientY + win.pageYOffset;
        let screenX = clientX + win.mozInnerScreenX,
            screenY = clientY + win.mozInnerScreenY;
        touch = doc.createTouch(win, el, touchId, pageX, pageY, screenX, screenY, clientX, clientY);
        touches[touchIndex] = touch;
        multiLast[touchId] = touch;
        emitMultiEvents('touchmove', touch, touches);
        break;
      case 'wait':
        if (pack[2] != undefined ) {
          waitTime = pack[2]*1000;
          if (waitTime > maxTime) {
            maxTime = waitTime;
          }
        }
        break;
    }//end of switch block
  }//end of for loop
  if (maxTime != 0) {
    checkTimer.initWithCallback(function(){setDispatch(batches, touches, command_id, batchIndex);}, maxTime, Ci.nsITimer.TYPE_ONE_SHOT);
  }
  else {
    setDispatch(batches, touches, command_id, batchIndex);
  }
}

/**
 * Function to start multi-action
 */
function multiAction(msg) {
  let command_id = msg.json.command_id;
  let args = msg.json.value;
  // maxlen is the longest action chain for one finger
  let maxlen = msg.json.maxlen;
  try {
    // unwrap the original nested array
    let commandArray = elementManager.convertWrappedArguments(args, curContainer);
    let concurrentEvent = [];
    let temp;
    for (let i = 0; i < maxlen; i++) {
      let row = [];
      for (let j = 0; j < commandArray.length; j++) {
        if (commandArray[j][i] != undefined) {
          // add finger id to the front of each action, i.e. [finger_id, action, element]
          temp = commandArray[j][i];
          temp.unshift(j);
          row.push(temp);
        }
      }
      concurrentEvent.push(row);
    }
    // now concurrent event is made of sets where each set contain a list of actions that need to be fired.
    // note: each action belongs to a different finger
    // pendingTouches keeps track of current touches that's on the screen
    let pendingTouches = [];
    setDispatch(concurrentEvent, pendingTouches, command_id);
  } catch (e) {
    sendError(e, command_id);
  }
}

/*
 * This implements the latter part of a get request (for the case we need to resume one
 * when a remoteness update happens in the middle of a navigate request). This is most of
 * of the work of a navigate request, but doesn't assume DOMContentLoaded is yet to fire.
 */
function pollForReadyState(msg, start, callback) {
  let {pageTimeout, url, command_id} = msg.json;
  start = start ? start : new Date().getTime();

  if (!callback) {
    callback = () => {};
  }

  let end = null;
  function checkLoad() {
    navTimer.cancel();
    end = new Date().getTime();
    let aboutErrorRegex = /about:.+(error)\?/;
    let elapse = end - start;
    let doc = curContainer.frame.document;
    if (pageTimeout == null || elapse <= pageTimeout) {
      if (doc.readyState == "complete") {
        callback();
        sendOk(command_id);
      } else if (doc.readyState == "interactive" &&
                 aboutErrorRegex.exec(doc.baseURI) &&
                 !doc.baseURI.startsWith(url)) {
        // We have reached an error url without requesting it.
        callback();
        sendError(new UnknownError("Error loading page"), command_id);
      } else if (doc.readyState == "interactive" &&
                 doc.baseURI.startsWith("about:")) {
        callback();
        sendOk(command_id);
      } else {
        navTimer.initWithCallback(checkLoad, 100, Ci.nsITimer.TYPE_ONE_SHOT);
      }
    } else {
      callback();
      sendError(new TimeoutError("Error loading page, timed out (checkLoad)"), command_id);
    }
  }
  checkLoad();
}

/**
 * Navigate to the given URL.  The operation will be performed on the
 * current browsing context, which means it handles the case where we
 * navigate within an iframe.  All other navigation is handled by the
 * driver (in chrome space).
 */
function get(msg) {
  let start = new Date().getTime();

  // Prevent DOMContentLoaded events from frames from invoking this
  // code, unless the event is coming from the frame associated with
  // the current window (i.e. someone has used switch_to_frame).
  onDOMContentLoaded = function onDOMContentLoaded(event) {
    if (!event.originalTarget.defaultView.frameElement ||
        event.originalTarget.defaultView.frameElement == curContainer.frame.frameElement) {
      pollForReadyState(msg, start, () => {
        removeEventListener("DOMContentLoaded", onDOMContentLoaded, false);
        onDOMContentLoaded = null;
      });
    }
  };

  function timerFunc() {
    removeEventListener("DOMContentLoaded", onDOMContentLoaded, false);
    sendError(new TimeoutError("Error loading page, timed out (onDOMContentLoaded)"), msg.json.command_id);
  }
  if (msg.json.pageTimeout != null) {
    navTimer.initWithCallback(timerFunc, msg.json.pageTimeout, Ci.nsITimer.TYPE_ONE_SHOT);
  }
  addEventListener("DOMContentLoaded", onDOMContentLoaded, false);
  if (isB2G) {
    curContainer.frame.location = msg.json.url;
  } else {
    // We need to move to the top frame before navigating
    sendSyncMessage("Marionette:switchedToFrame", { frameValue: null });
    curContainer.frame = content;

    curContainer.frame.location = msg.json.url;
  }
}

 /**
 * Cancel the polling and remove the event listener associated with a current
 * navigation request in case we're interupted by an onbeforeunload handler
 * and navigation doesn't complete.
 */
function cancelRequest() {
  navTimer.cancel();
  if (onDOMContentLoaded) {
    removeEventListener("DOMContentLoaded", onDOMContentLoaded, false);
  }
}

/**
 * Get URL of the top-level browsing context.
 */
function getCurrentUrl(isB2G) {
  if (isB2G) {
    return curContainer.frame.location.href;
  } else {
    return content.location.href;
  }
}

/**
 * Get the title of the current browsing context.
 */
function getTitle() {
  return curContainer.frame.top.document.title;
}

/**
 * Get source of the current browsing context's DOM.
 */
function getPageSource() {
  let XMLSerializer = curContainer.frame.XMLSerializer;
  let source = new XMLSerializer().serializeToString(curContainer.frame.document);
  return source;
}

/**
 * Cause the browser to traverse one step backward in the joint history
 * of the current top-level browsing context.
 */
function goBack() {
  curContainer.frame.history.back();
}

/**
 * Go forward in history
 */
function goForward(msg) {
  curContainer.frame.history.forward();
  sendOk(msg.json.command_id);
}

/**
 * Refresh the page
 */
function refresh(msg) {
  let command_id = msg.json.command_id;
  curContainer.frame.location.reload(true);
  let listen = function() {
    removeEventListener("DOMContentLoaded", arguments.callee, false);
    sendOk(command_id);
  };
  addEventListener("DOMContentLoaded", listen, false);
}

/**
 * Find an element in the current browsing context's document using the
 * given search strategy.
 */
function findElementContent(opts) {
  return new Promise((resolve, reject) => {
    elementManager.find(
        curContainer,
        opts,
        opts.searchTimeout,
        false /* all */,
        resolve,
        reject);
  });
}

/**
 * Find elements in the current browsing context's document using the
 * given search strategy.
 */
function findElementsContent(opts) {
  return new Promise((resolve, reject) => {
    elementManager.find(
        curContainer,
        opts,
        opts.searchTimeout,
        true /* all */,
        resolve,
        reject);
  });
}

/**
 * Find and return the active element on the page.
 *
 * @return {WebElement}
 *     Reference to web element.
 */
function getActiveElement() {
  let el = curContainer.frame.document.activeElement;
  return elementManager.addToKnownElements(el);
}

/**
 * Send click event to element.
 *
 * @param {WebElement} id
 *     Reference to the web element to click.
 */
function clickElement(id) {
  let el = elementManager.getKnownElement(id, curContainer);
  let visible = checkVisible(el);
  if (!visible) {
    throw new ElementNotVisibleError("Element is not visible");
  }
  return accessibility.getAccessibleObject(el, true).then(acc => {
    checkVisibleAccessibility(acc, el, visible);
    if (utils.isElementEnabled(el)) {
      checkEnabledAccessibility(acc, el, true);
      checkActionableAccessibility(acc, el);
      utils.synthesizeMouseAtCenter(el, {}, el.ownerDocument.defaultView);
    } else {
      throw new InvalidElementStateError("Element is not Enabled");
    }
  });
}

/**
 * Get a given attribute of an element.
 *
 * @param {WebElement} id
 *     Reference to the web element to get the attribute of.
 * @param {string} name
 *     Name of the attribute.
 *
 * @return {string}
 *     The value of the attribute.
 */
function getElementAttribute(id, name) {
  let el = elementManager.getKnownElement(id, curContainer);
  return utils.getElementAttribute(el, name);
}

/**
 * Get the text of this element. This includes text from child elements.
 *
 * @param {WebElement} id
 *     Reference to web element.
 *
 * @return {string}
 *     Text of element.
 */
function getElementText(id) {
  let el = elementManager.getKnownElement(id, curContainer);
  return utils.getElementText(el);
}

/**
 * Get the tag name of an element.
 *
 * @param {WebElement} id
 *     Reference to web element.
 *
 * @return {string}
 *     Tag name of element.
 */
function getElementTagName(id) {
  let el = elementManager.getKnownElement(id, curContainer);
  return el.tagName.toLowerCase();
}

/**
 * Determine the element displayedness of the given web element.
 *
 * Also performs additional accessibility checks if enabled by session
 * capability.
 */
function isElementDisplayed(id) {
  let el = elementManager.getKnownElement(id, curContainer);
  let displayed = utils.isElementDisplayed(el);
  return accessibility.getAccessibleObject(el).then(acc => {
    checkVisibleAccessibility(acc, el, displayed);
    return displayed;
  });
}

/**
 * Retrieves the computed value of the given CSS property of the given
 * web element.
 *
 * @param {String} id
 *     Web element reference.
 * @param {String} prop
 *     The CSS property to get.
 *
 * @return {String}
 *     Effective value of the requested CSS property.
 */
function getElementValueOfCssProperty(id, prop) {
  let el = elementManager.getKnownElement(id, curContainer);
  let st = curContainer.frame.document.defaultView.getComputedStyle(el, null);
  return st.getPropertyValue(prop);
}

/**
 * Get the position and dimensions of the element.
 *
 * @param {WebElement} id
 *     Reference to web element.
 *
 * @return {Object.<string, number>}
 *     The x, y, width, and height properties of the element.
 */
function getElementRect(id) {
  let el = elementManager.getKnownElement(id, curContainer);
  let clientRect = el.getBoundingClientRect();
  return {
    x: clientRect.x + curContainer.frame.pageXOffset,
    y: clientRect.y  + curContainer.frame.pageYOffset,
    width: clientRect.width,
    height: clientRect.height
  };
}

/**
 * Check if element is enabled.
 *
 * @param {WebElement} id
 *     Reference to web element.
 *
 * @return {boolean}
 *     True if enabled, false otherwise.
 */
function isElementEnabled(id) {
  let el = elementManager.getKnownElement(id, curContainer);
  let enabled = utils.isElementEnabled(el);
  return accessibility.getAccessibleObject(el).then(acc => {
    checkEnabledAccessibility(acc, el, enabled);
    return enabled;
  });
}

/**
 * Determines if the referenced element is selected or not.
 *
 * This operation only makes sense on input elements of the Checkbox-
 * and Radio Button states, or option elements.
 */
function isElementSelected(id) {
  let el = elementManager.getKnownElement(id, curContainer);
  let selected = utils.isElementSelected(el);
  return accessibility.getAccessibleObject(el).then(acc => {
    checkSelectedAccessibility(acc, el, selected);
    return selected;
  });
}

/**
 * Send keys to element
 */
function sendKeysToElement(msg) {
  let command_id = msg.json.command_id;
  let val = msg.json.value;
  let el;

  return Promise.resolve(elementManager.getKnownElement(msg.json.id, curContainer))
    .then(knownEl => {
      el = knownEl;
      // Element should be actionable from the accessibility standpoint to be able
      // to send keys to it.
      return accessibility.getAccessibleObject(el, true)
    }).then(acc => {
      checkActionableAccessibility(acc, el);
      if (el.type == "file") {
        let p = val.join("");
        fileInputElement = el;
        // In e10s, we can only construct File objects in the parent process,
        // so pass the filename to driver.js, which in turn passes them back
        // to this frame script in receiveFiles.
        sendSyncMessage("Marionette:getFiles",
                        {value: p, command_id: command_id});
      } else {
        utils.sendKeysToElement(curContainer.frame, el, val, sendOk, sendError, command_id);
      }
    }).catch(e => sendError(e, command_id));
}

/**
 * Clear the text of an element.
 */
function clearElement(id) {
  try {
    let el = elementManager.getKnownElement(id, curContainer);
    if (el.type == "file") {
      el.value = null;
    } else {
      utils.clearElement(el);
    }
  } catch (e) {
    // Bug 964738: Newer atoms contain status codes which makes wrapping
    // this in an error prototype that has a status property unnecessary
    if (e.name == "InvalidElementStateError") {
      throw new InvalidElementStateError(e.message);
    } else {
      throw e;
    }
  }
}

/**
 * Switch the current context to the specified host's Shadow DOM.
 * @param {WebElement} id
 *     Reference to web element.
 */
function switchToShadowRoot(id) {
  if (!id) {
    // If no host element is passed, attempt to find a parent shadow root or, if
    // none found, unset the current shadow root
    if (curContainer.shadowRoot) {
      let parent = curContainer.shadowRoot.host;
      while (parent && !(parent instanceof curContainer.frame.ShadowRoot)) {
        parent = parent.parentNode;
      }
      curContainer.shadowRoot = parent;
    }
    return;
  }

  let foundShadowRoot;
  let hostEl = elementManager.getKnownElement(id, curContainer);
  foundShadowRoot = hostEl.shadowRoot;
  if (!foundShadowRoot) {
    throw new NoSuchElementError('Unable to locate shadow root: ' + id);
  }
  curContainer.shadowRoot = foundShadowRoot;
}

/**
 * Switch to the parent frame of the current Frame. If the frame is the top most
 * is the current frame then no action will happen.
 */
 function switchToParentFrame(msg) {
   let command_id = msg.json.command_id;
   curContainer.frame = curContainer.frame.parent;
   let parentElement = elementManager.addToKnownElements(curContainer.frame);

   sendSyncMessage("Marionette:switchedToFrame", { frameValue: parentElement });

   sendOk(msg.json.command_id);
 }

/**
 * Switch to frame given either the server-assigned element id,
 * its index in window.frames, or the iframe's name or id.
 */
function switchToFrame(msg) {
  let command_id = msg.json.command_id;
  function checkLoad() {
    let errorRegex = /about:.+(error)|(blocked)\?/;
    if (curContainer.frame.document.readyState == "complete") {
      sendOk(command_id);
      return;
    } else if (curContainer.frame.document.readyState == "interactive" &&
        errorRegex.exec(curContainer.frame.document.baseURI)) {
      sendError(new UnknownError("Error loading page"), command_id);
      return;
    }
    checkTimer.initWithCallback(checkLoad, 100, Ci.nsITimer.TYPE_ONE_SHOT);
  }
  let foundFrame = null;
  let frames = [];
  let parWindow = null;
  // Check of the curContainer.frame reference is dead
  try {
    frames = curContainer.frame.frames;
    //Until Bug 761935 lands, we won't have multiple nested OOP iframes. We will only have one.
    //parWindow will refer to the iframe above the nested OOP frame.
    parWindow = curContainer.frame.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
  } catch (e) {
    // We probably have a dead compartment so accessing it is going to make Firefox
    // very upset. Let's now try redirect everything to the top frame even if the
    // user has given us a frame since search doesnt look up.
    msg.json.id = null;
    msg.json.element = null;
  }

  if ((msg.json.id === null || msg.json.id === undefined) && (msg.json.element == null)) {
    // returning to root frame
    sendSyncMessage("Marionette:switchedToFrame", { frameValue: null });

    curContainer.frame = content;
    if(msg.json.focus == true) {
      curContainer.frame.focus();
    }

    checkTimer.initWithCallback(checkLoad, 100, Ci.nsITimer.TYPE_ONE_SHOT);
    return;
  }
  if (msg.json.element != undefined) {
    if (elementManager.seenItems[msg.json.element] != undefined) {
      let wantedFrame;
      try {
        wantedFrame = elementManager.getKnownElement(msg.json.element, curContainer); //Frame Element
      } catch (e) {
        sendError(e, command_id);
      }

      if (frames.length > 0) {
        for (let i = 0; i < frames.length; i++) {
          // use XPCNativeWrapper to compare elements; see bug 834266
          if (XPCNativeWrapper(frames[i].frameElement) == XPCNativeWrapper(wantedFrame)) {
            curContainer.frame = frames[i].frameElement;
            foundFrame = i;
          }
        }
      }
      if (foundFrame === null) {
        // Either the frame has been removed or we have a OOP frame
        // so lets just get all the iframes and do a quick loop before
        // throwing in the towel
        let iframes = curContainer.frame.document.getElementsByTagName("iframe");
        for (var i = 0; i < iframes.length; i++) {
          if (XPCNativeWrapper(iframes[i]) == XPCNativeWrapper(wantedFrame)) {
            curContainer.frame = iframes[i];
            foundFrame = i;
          }
        }
      }
    }
  }
  if (foundFrame === null) {
    if (typeof(msg.json.id) === 'number') {
      try {
        foundFrame = frames[msg.json.id].frameElement;
        if (foundFrame !== null) {
          curContainer.frame = foundFrame;
          foundFrame = elementManager.addToKnownElements(curContainer.frame);
        }
        else {
          // If foundFrame is null at this point then we have the top level browsing
          // context so should treat it accordingly.
          sendSyncMessage("Marionette:switchedToFrame", { frameValue: null});
          curContainer.frame = content;
          if(msg.json.focus == true) {
            curContainer.frame.focus();
          }

          checkTimer.initWithCallback(checkLoad, 100, Ci.nsITimer.TYPE_ONE_SHOT);
          return;
        }
      } catch (e) {
        // Since window.frames does not return OOP frames it will throw
        // and we land up here. Let's not give up and check if there are
        // iframes and switch to the indexed frame there
        let iframes = curContainer.frame.document.getElementsByTagName("iframe");
        if (msg.json.id >= 0 && msg.json.id < iframes.length) {
          curContainer.frame = iframes[msg.json.id];
          foundFrame = msg.json.id;
        }
      }
    }
  }

  if (foundFrame === null) {
    sendError(new NoSuchFrameError("Unable to locate frame: " + (msg.json.id || msg.json.element)), command_id);
    return true;
  }

  // send a synchronous message to let the server update the currently active
  // frame element (for getActiveFrame)
  let frameValue = elementManager.wrapValue(curContainer.frame.wrappedJSObject)['ELEMENT'];
  sendSyncMessage("Marionette:switchedToFrame", { frameValue: frameValue });

  let rv = null;
  if (curContainer.frame.contentWindow === null) {
    // The frame we want to switch to is a remote/OOP frame;
    // notify our parent to handle the switch
    curContainer.frame = content;
    rv = {win: parWindow, frame: foundFrame};
  } else {
    curContainer.frame = curContainer.frame.contentWindow;
    if (msg.json.focus)
      curContainer.frame.focus();
    checkTimer.initWithCallback(checkLoad, 100, Ci.nsITimer.TYPE_ONE_SHOT);
  }

  sendResponse({value: rv}, command_id);
}
 /**
  * Add a cookie to the document
  */
function addCookie(msg) {
  let cookie = msg.json.cookie;
  if (!cookie.expiry) {
    var date = new Date();
    var thePresent = new Date(Date.now());
    date.setYear(thePresent.getFullYear() + 20);
    cookie.expiry = date.getTime() / 1000;  // Stored in seconds.
  }

  if (!cookie.domain) {
    var location = curContainer.frame.document.location;
    cookie.domain = location.hostname;
  } else {
    var currLocation = curContainer.frame.location;
    var currDomain = currLocation.host;
    if (currDomain.indexOf(cookie.domain) == -1) {
      sendError(new InvalidCookieDomainError("You may only set cookies for the current domain"), msg.json.command_id);
    }
  }

  // The cookie's domain may include a port. Which is bad. Remove it
  // We'll catch ip6 addresses by mistake. Since no-one uses those
  // this will be okay for now. See Bug 814416
  if (cookie.domain.match(/:\d+$/)) {
    cookie.domain = cookie.domain.replace(/:\d+$/, '');
  }

  var document = curContainer.frame.document;
  if (!document || !document.contentType.match(/html/i)) {
    sendError(new UnableToSetCookieError("You may only set cookies on html documents"), msg.json.command_id);
  }

  let added = sendSyncMessage("Marionette:addCookie", {value: cookie});
  if (added[0] !== true) {
    sendError(new UnableToSetCookieError(), msg.json.command_id);
    return;
  }
  sendOk(msg.json.command_id);
}

/**
 * Get all cookies for the current domain.
 */
function getCookies() {
  let rv = [];
  let cookies = getVisibleCookies(curContainer.frame.location);

  for (let cookie of cookies) {
    let expires = cookie.expires;
    // session cookie, don't return an expiry
    if (expires == 0) {
      expires = null;
    // date before epoch time, cap to epoch
    } else if (expires == 1) {
      expires = 0;
    }
    rv.push({
      'name': cookie.name,
      'value': cookie.value,
      'path': cookie.path,
      'domain': cookie.host,
      'secure': cookie.isSecure,
      'httpOnly': cookie.httpOnly,
      'expiry': expires
    });
  }

  return rv;
}

/**
 * Delete a cookie by name
 */
function deleteCookie(msg) {
  let toDelete = msg.json.name;
  let cookies = getVisibleCookies(curContainer.frame.location);
  for (let cookie of cookies) {
    if (cookie.name == toDelete) {
      let deleted = sendSyncMessage("Marionette:deleteCookie", {value: cookie});
      if (deleted[0] !== true) {
        sendError(new UnknownError("Could not delete cookie: " + msg.json.name), msg.json.command_id);
        return;
      }
    }
  }

  sendOk(msg.json.command_id);
}

/**
 * Delete all the visibile cookies on a page
 */
function deleteAllCookies(msg) {
  let cookies = getVisibleCookies(curContainer.frame.location);
  for (let cookie of cookies) {
    let deleted = sendSyncMessage("Marionette:deleteCookie", {value: cookie});
    if (!deleted[0]) {
      sendError(new UnknownError("Could not delete cookie: " + JSON.stringify(cookie)), msg.json.command_id);
      return;
    }
  }
  sendOk(msg.json.command_id);
}

/**
 * Get all the visible cookies from a location
 */
function getVisibleCookies(location) {
  let currentPath = location.pathname || '/';
  let result = sendSyncMessage("Marionette:getVisibleCookies",
                               {value: [currentPath, location.hostname]});
  return result[0];
}

function getAppCacheStatus(msg) {
  sendResponse({ value: curContainer.frame.applicationCache.status },
               msg.json.command_id);
}

// emulator callbacks
var _emu_cb_id = 0;
var _emu_cbs = {};

function runEmulatorCmd(cmd, callback) {
  if (callback) {
    _emu_cbs[_emu_cb_id] = callback;
  }
  sendAsyncMessage("Marionette:runEmulatorCmd", {emulator_cmd: cmd, id: _emu_cb_id});
  _emu_cb_id += 1;
}

function runEmulatorShell(args, callback) {
  if (callback) {
    _emu_cbs[_emu_cb_id] = callback;
  }
  sendAsyncMessage("Marionette:runEmulatorShell", {emulator_shell: args, id: _emu_cb_id});
  _emu_cb_id += 1;
}

function emulatorCmdResult(msg) {
  let message = msg.json;
  if (!sandboxes[sandboxName]) {
    return;
  }
  let cb = _emu_cbs[message.id];
  delete _emu_cbs[message.id];
  if (!cb) {
    return;
  }
  try {
    cb(message.result);
  } catch (e) {
    sendError(e, -1);
    return;
  }
}

function importScript(msg) {
  let command_id = msg.json.command_id;
  let file;
  if (importedScripts.exists()) {
    file = FileUtils.openFileOutputStream(importedScripts,
        FileUtils.MODE_APPEND | FileUtils.MODE_WRONLY);
  }
  else {
    //Note: The permission bits here don't actually get set (bug 804563)
    importedScripts.createUnique(Components.interfaces.nsIFile.NORMAL_FILE_TYPE,
                                 parseInt("0666", 8));
    file = FileUtils.openFileOutputStream(importedScripts,
                                          FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE);
    importedScripts.permissions = parseInt("0666", 8); //actually set permissions
  }
  file.write(msg.json.script, msg.json.script.length);
  file.close();
  sendOk(command_id);
}

/**
 * Perform a screen capture in content context.
 *
 * @param {UUID=} id
 *     Optional web element reference of an element to take a screenshot
 *     of.
 * @param {boolean=} full
 *     True to take a screenshot of the entire document element.  Is not
 *     considered if {@code id} is not defined.  Defaults to true.
 * @param {Array.<UUID>=} highlights
 *     Draw a border around the elements found by their web element
 *     references.
 *
 * @return {string}
 *     Base64 encoded string of an image/png type.
 */
function takeScreenshot(id, full=true, highlights=[]) {
  let canvas;

  let highlightEls = [];
  for (let h of highlights) {
    let el = elementManager.getKnownElement(h, curContainer);
    highlightEls.push(el);
  }

  // viewport
  if (!id && !full) {
    canvas = capture.viewport(curContainer.frame.document, highlightEls);

  // element or full document element
  } else {
    let node;
    if (id) {
      node = elementManager.getKnownElement(id, curContainer);
    } else {
      node = curContainer.frame.document.documentElement;
    }

    canvas = capture.element(node, highlightEls);
  }

  return capture.toBase64(canvas);
}

// Call register self when we get loaded
registerSelf();
