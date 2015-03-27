/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

let uuidGen = Cc["@mozilla.org/uuid-generator;1"]
                .getService(Ci.nsIUUIDGenerator);

let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
               .getService(Ci.mozIJSSubScriptLoader);

loader.loadSubScript("chrome://marionette/content/simpletest.js");
loader.loadSubScript("chrome://marionette/content/common.js");
loader.loadSubScript("chrome://marionette/content/actions.js");
Cu.import("chrome://marionette/content/elements.js");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
let utils = {};
utils.window = content;
// Load Event/ChromeUtils for use with JS scripts:
loader.loadSubScript("chrome://marionette/content/EventUtils.js", utils);
loader.loadSubScript("chrome://marionette/content/ChromeUtils.js", utils);
loader.loadSubScript("chrome://marionette/content/atoms.js", utils);
loader.loadSubScript("chrome://marionette/content/sendkeys.js", utils);

loader.loadSubScript("chrome://specialpowers/content/specialpowersAPI.js");
loader.loadSubScript("chrome://specialpowers/content/specialpowers.js");

let marionetteLogObj = new MarionetteLogObj();

let isB2G = false;

let marionetteTestName;
let winUtil = content.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDOMWindowUtils);
let listenerId = null; //unique ID of this listener
let curFrame = content;
let previousFrame = null;
let elementManager = new ElementManager([]);
let accessibility = new Accessibility();
let actions = new ActionChain(utils, checkForInterrupted);
let importedScripts = null;

// The sandbox we execute test scripts in. Gets lazily created in
// createExecuteContentSandbox().
let sandbox;

// the unload handler
let onunload;

// Flag to indicate whether an async script is currently running or not.
let asyncTestRunning = false;
let asyncTestCommandId;
let asyncTestTimeoutId;

let inactivityTimeoutId = null;
let heartbeatCallback = function () {}; // Called by the simpletest methods.

let originalOnError;
//timer for doc changes
let checkTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
//timer for readystate
let readyStateTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
// timer for navigation commands.
let navTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
let onDOMContentLoaded;
// Send move events about this often
let EVENT_INTERVAL = 30; // milliseconds
// last touch for each fingerId
let multiLast = {};

Cu.import("resource://gre/modules/Log.jsm");
let logger = Log.repository.getLogger("Marionette");
logger.info("loaded listener.js");
let modalHandler = function() {
  // This gets called on the system app only since it receives the mozbrowserprompt event
  sendSyncMessage("Marionette:switchedToFrame", { frameValue: null, storePrevious: true });
  let isLocal = sendSyncMessage("MarionetteFrame:handleModal", {})[0].value;
  if (isLocal) {
    previousFrame = curFrame;
  }
  curFrame = content;
  sandbox = null;
};

/**
 * Called when listener is first started up.
 * The listener sends its unique window ID and its current URI to the actor.
 * If the actor returns an ID, we start the listeners. Otherwise, nothing happens.
 */
function registerSelf() {
  let msg = {value: winUtil.outerWindowID}
  // register will have the ID and a boolean describing if this is the main process or not
  let register = sendSyncMessage("Marionette:register", msg);

  if (register[0]) {
    let {id, remotenessChange} = register[0][0];
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

  let domWindowUtils = curFrame.
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

/**
 * Start all message listeners
 */
function startListeners() {
  addMessageListenerId("Marionette:newSession", newSession);
  addMessageListenerId("Marionette:executeScript", executeScript);
  addMessageListenerId("Marionette:executeAsyncScript", executeAsyncScript);
  addMessageListenerId("Marionette:executeJSScript", executeJSScript);
  addMessageListenerId("Marionette:singleTap", singleTap);
  addMessageListenerId("Marionette:actionChain", actionChain);
  addMessageListenerId("Marionette:multiAction", multiAction);
  addMessageListenerId("Marionette:get", get);
  addMessageListenerId("Marionette:pollForReadyState", pollForReadyState);
  addMessageListenerId("Marionette:cancelRequest", cancelRequest);
  addMessageListenerId("Marionette:getCurrentUrl", getCurrentUrl);
  addMessageListenerId("Marionette:getTitle", getTitle);
  addMessageListenerId("Marionette:getPageSource", getPageSource);
  addMessageListenerId("Marionette:goBack", goBack);
  addMessageListenerId("Marionette:goForward", goForward);
  addMessageListenerId("Marionette:refresh", refresh);
  addMessageListenerId("Marionette:findElementContent", findElementContent);
  addMessageListenerId("Marionette:findElementsContent", findElementsContent);
  addMessageListenerId("Marionette:getActiveElement", getActiveElement);
  addMessageListenerId("Marionette:clickElement", clickElement);
  addMessageListenerId("Marionette:getElementAttribute", getElementAttribute);
  addMessageListenerId("Marionette:getElementText", getElementText);
  addMessageListenerId("Marionette:getElementTagName", getElementTagName);
  addMessageListenerId("Marionette:isElementDisplayed", isElementDisplayed);
  addMessageListenerId("Marionette:getElementValueOfCssProperty", getElementValueOfCssProperty);
  addMessageListenerId("Marionette:submitElement", submitElement);
  addMessageListenerId("Marionette:getElementSize", getElementSize);
  addMessageListenerId("Marionette:getElementRect", getElementRect);
  addMessageListenerId("Marionette:isElementEnabled", isElementEnabled);
  addMessageListenerId("Marionette:isElementSelected", isElementSelected);
  addMessageListenerId("Marionette:sendKeysToElement", sendKeysToElement);
  addMessageListenerId("Marionette:getElementLocation", getElementLocation); //deprecated
  addMessageListenerId("Marionette:clearElement", clearElement);
  addMessageListenerId("Marionette:switchToFrame", switchToFrame);
  addMessageListenerId("Marionette:deleteSession", deleteSession);
  addMessageListenerId("Marionette:sleepSession", sleepSession);
  addMessageListenerId("Marionette:emulatorCmdResult", emulatorCmdResult);
  addMessageListenerId("Marionette:importScript", importScript);
  addMessageListenerId("Marionette:getAppCacheStatus", getAppCacheStatus);
  addMessageListenerId("Marionette:setTestName", setTestName);
  addMessageListenerId("Marionette:takeScreenshot", takeScreenshot);
  addMessageListenerId("Marionette:addCookie", addCookie);
  addMessageListenerId("Marionette:getCookies", getCookies);
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
  removeMessageListenerId("Marionette:newSession", newSession);
  removeMessageListenerId("Marionette:executeScript", executeScript);
  removeMessageListenerId("Marionette:executeAsyncScript", executeAsyncScript);
  removeMessageListenerId("Marionette:executeJSScript", executeJSScript);
  removeMessageListenerId("Marionette:singleTap", singleTap);
  removeMessageListenerId("Marionette:actionChain", actionChain);
  removeMessageListenerId("Marionette:multiAction", multiAction);
  removeMessageListenerId("Marionette:get", get);
  removeMessageListenerId("Marionette:pollForReadyState", pollForReadyState);
  removeMessageListenerId("Marionette:cancelRequest", cancelRequest);
  removeMessageListenerId("Marionette:getTitle", getTitle);
  removeMessageListenerId("Marionette:getPageSource", getPageSource);
  removeMessageListenerId("Marionette:getCurrentUrl", getCurrentUrl);
  removeMessageListenerId("Marionette:goBack", goBack);
  removeMessageListenerId("Marionette:goForward", goForward);
  removeMessageListenerId("Marionette:refresh", refresh);
  removeMessageListenerId("Marionette:findElementContent", findElementContent);
  removeMessageListenerId("Marionette:findElementsContent", findElementsContent);
  removeMessageListenerId("Marionette:getActiveElement", getActiveElement);
  removeMessageListenerId("Marionette:clickElement", clickElement);
  removeMessageListenerId("Marionette:getElementAttribute", getElementAttribute);
  removeMessageListenerId("Marionette:getElementText", getElementText);
  removeMessageListenerId("Marionette:getElementTagName", getElementTagName);
  removeMessageListenerId("Marionette:isElementDisplayed", isElementDisplayed);
  removeMessageListenerId("Marionette:getElementValueOfCssProperty", getElementValueOfCssProperty);
  removeMessageListenerId("Marionette:submitElement", submitElement);
  removeMessageListenerId("Marionette:getElementSize", getElementSize);  //deprecated
  removeMessageListenerId("Marionette:getElementRect", getElementRect);
  removeMessageListenerId("Marionette:isElementEnabled", isElementEnabled);
  removeMessageListenerId("Marionette:isElementSelected", isElementSelected);
  removeMessageListenerId("Marionette:sendKeysToElement", sendKeysToElement);
  removeMessageListenerId("Marionette:getElementLocation", getElementLocation);
  removeMessageListenerId("Marionette:clearElement", clearElement);
  removeMessageListenerId("Marionette:switchToFrame", switchToFrame);
  removeMessageListenerId("Marionette:deleteSession", deleteSession);
  removeMessageListenerId("Marionette:sleepSession", sleepSession);
  removeMessageListenerId("Marionette:emulatorCmdResult", emulatorCmdResult);
  removeMessageListenerId("Marionette:importScript", importScript);
  removeMessageListenerId("Marionette:getAppCacheStatus", getAppCacheStatus);
  removeMessageListenerId("Marionette:setTestName", setTestName);
  removeMessageListenerId("Marionette:takeScreenshot", takeScreenshot);
  removeMessageListenerId("Marionette:addCookie", addCookie);
  removeMessageListenerId("Marionette:getCookies", getCookies);
  removeMessageListenerId("Marionette:deleteAllCookies", deleteAllCookies);
  removeMessageListenerId("Marionette:deleteCookie", deleteCookie);
  if (isB2G) {
    content.removeEventListener("mozbrowsershowmodalprompt", modalHandler, false);
  }
  elementManager.reset();
  // reset frame to the top-most frame
  curFrame = content;
  curFrame.focus();
  actions.touchIds = {};
}

/*
 * Helper methods
 */

/**
 * Generic method to send a message to the server
 */
function sendToServer(msg, value, command_id) {
  if (command_id) {
    value.command_id = command_id;
  }
  sendAsyncMessage(msg, value);
}

/**
 * Send response back to server
 */
function sendResponse(value, command_id) {
  sendToServer("Marionette:done", value, command_id);
}

/**
 * Send ack back to server
 */
function sendOk(command_id) {
  sendToServer("Marionette:ok", {}, command_id);
}

/**
 * Send log message to server
 */
function sendLog(msg) {
  sendToServer("Marionette:log", { message: msg });
}

/**
 * Send error message to server
 */
function sendError(msg, code, stack, cmdId) {
  let payload = {message: msg, code: code, stack: stack};
  sendToServer("Marionette:error", payload, cmdId);
}

/**
 * Clear test values after completion of test
 */
function resetValues() {
  sandbox = null;
  curFrame = content;
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
  if (previousFrame) {
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
      if (previousFrame) {
        //if previousFrame is set, then we're in a single process environment
        cuFrame = actions.frame = previousFrame;
        previousFrame = null;
        sandbox = null;
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
function createExecuteContentSandbox(aWindow, timeout) {
  let sandbox = new Cu.Sandbox(aWindow, {sandboxPrototype: aWindow});
  sandbox.global = sandbox;
  sandbox.window = aWindow;
  sandbox.document = sandbox.window.document;
  sandbox.navigator = sandbox.window.navigator;
  sandbox.testUtils = utils;
  sandbox.asyncTestCommandId = asyncTestCommandId;

  let marionette = new Marionette(this, aWindow, "content",
                                  marionetteLogObj, timeout,
                                  heartbeatCallback,
                                  marionetteTestName);
  marionette.runEmulatorCmd = (cmd, cb) => this.runEmulatorCmd(cmd, cb);
  marionette.runEmulatorShell = (args, cb) => this.runEmulatorShell(args, cb);
  sandbox.marionette = marionette;
  marionette.exports.forEach(function(fn) {
    try {
      sandbox[fn] = marionette[fn].bind(marionette);
    }
    catch(e) {
      sandbox[fn] = marionette[fn];
    }
  });

  if (aWindow.wrappedJSObject.SpecialPowers != undefined) {
    XPCOMUtils.defineLazyGetter(sandbox, 'SpecialPowers', function() {
      return aWindow.wrappedJSObject.SpecialPowers;
    });
  }
  else {
    XPCOMUtils.defineLazyGetter(sandbox, 'SpecialPowers', function() {
      return new SpecialPowers(aWindow);
    });
  }

  sandbox.asyncComplete = function sandbox_asyncComplete(value, status, stack, commandId) {
    if (commandId == asyncTestCommandId) {
      curFrame.removeEventListener("unload", onunload, false);
      curFrame.clearTimeout(asyncTestTimeoutId);

      if (inactivityTimeoutId != null) {
        curFrame.clearTimeout(inactivityTimeoutId);
      }


      sendSyncMessage("Marionette:shareData",
                      {log: elementManager.wrapValue(marionetteLogObj.getLogs())});
      marionetteLogObj.clearLogs();

      if (status == 0){
        if (Object.keys(_emu_cbs).length) {
          _emu_cbs = {};
          sendError("Emulator callback still pending when finish() called",
                    500, null, commandId);
        }
        else {
          sendResponse({value: elementManager.wrapValue(value), status: status},
                       commandId);
        }
      }
      else {
        sendError(value, status, stack, commandId);
      }

      asyncTestRunning = false;
      asyncTestTimeoutId = undefined;
      asyncTestCommandId = undefined;
      inactivityTimeoutId = null;
    }
  };
  sandbox.finish = function sandbox_finish() {
    if (asyncTestRunning) {
      sandbox.asyncComplete(marionette.generate_results(), 0, null, sandbox.asyncTestCommandId);
    } else {
      return marionette.generate_results();
    }
  };
  sandbox.marionetteScriptFinished = function sandbox_marionetteScriptFinished(value) {
    return sandbox.asyncComplete(value, 0, null, sandbox.asyncTestCommandId);
  };

  return sandbox;
}

/**
 * Execute the given script either as a function body (executeScript)
 * or directly (for 'mochitest' like JS Marionette tests)
 */
function executeScript(msg, directInject) {
  // Set up inactivity timeout.
  if (msg.json.inactivityTimeout) {
    let setTimer = function() {
        inactivityTimeoutId = curFrame.setTimeout(function() {
        sendError('timed out due to inactivity', 28, null, asyncTestCommandId);
      }, msg.json.inactivityTimeout);
   };

    setTimer();
    heartbeatCallback = function resetInactivityTimeout() {
      curFrame.clearTimeout(inactivityTimeoutId);
      setTimer();
    };
  }

  asyncTestCommandId = msg.json.command_id;
  let script = msg.json.script;

  if (msg.json.newSandbox || !sandbox) {
    sandbox = createExecuteContentSandbox(curFrame,
                                          msg.json.timeout);
    if (!sandbox) {
      sendError("Could not create sandbox!", 500, null, asyncTestCommandId);
      return;
    }
  }
  else {
    sandbox.asyncTestCommandId = asyncTestCommandId;
  }

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
      let res = Cu.evalInSandbox(script, sandbox, "1.8", "dummy file" ,0);
      sendSyncMessage("Marionette:shareData",
                      {log: elementManager.wrapValue(marionetteLogObj.getLogs())});
      marionetteLogObj.clearLogs();

      if (res == undefined || res.passed == undefined) {
        sendError("Marionette.finish() not called", 17, null, asyncTestCommandId);
      }
      else {
        sendResponse({value: elementManager.wrapValue(res)}, asyncTestCommandId);
      }
    }
    else {
      try {
        sandbox.__marionetteParams = Cu.cloneInto(elementManager.convertWrappedArguments(
          msg.json.args, curFrame), sandbox, { wrapReflectors: true });
      }
      catch(e) {
        sendError(e.message, e.code, e.stack, asyncTestCommandId);
        return;
      }

      script = "let __marionetteFunc = function(){" + script + "};" +
                   "__marionetteFunc.apply(null, __marionetteParams);";
      if (importedScripts.exists()) {
        let stream = Components.classes["@mozilla.org/network/file-input-stream;1"].
                      createInstance(Components.interfaces.nsIFileInputStream);
        stream.init(importedScripts, -1, 0, 0);
        let data = NetUtil.readInputStreamToString(stream, stream.available());
        stream.close();
        script = data + script;
      }
      let res = Cu.evalInSandbox(script, sandbox, "1.8", "dummy file", 0);
      sendSyncMessage("Marionette:shareData",
                      {log: elementManager.wrapValue(marionetteLogObj.getLogs())});
      marionetteLogObj.clearLogs();
      sendResponse({value: elementManager.wrapValue(res)}, asyncTestCommandId);
    }
  }
  catch (e) {
    // 17 = JavascriptException
    let error = createStackMessage(e,
                                   "execute_script",
                                   msg.json.filename,
                                   msg.json.line,
                                   script);
    sendError(error[0], 17, error[1], asyncTestCommandId);
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
      inactivityTimeoutId = curFrame.setTimeout(function() {
        sandbox.asyncComplete('timed out due to inactivity', 28, null, asyncTestCommandId);
      }, msg.json.inactivityTimeout);
    };

    setTimer();
    heartbeatCallback = function resetInactivityTimeout() {
      curFrame.clearTimeout(inactivityTimeoutId);
      setTimer();
    };
  }

  let script = msg.json.script;
  asyncTestCommandId = msg.json.command_id;

  onunload = function() {
    sendError("unload was called", 17, null, asyncTestCommandId);
  };
  curFrame.addEventListener("unload", onunload, false);

  if (msg.json.newSandbox || !sandbox) {
    sandbox = createExecuteContentSandbox(curFrame,
                                          msg.json.timeout);
    if (!sandbox) {
      sendError("Could not create sandbox!", 17, null, asyncTestCommandId);
      return;
    }
  }
  else {
    sandbox.asyncTestCommandId = asyncTestCommandId;
  }
  sandbox.tag = script;

  // Error code 28 is scriptTimeout, but spec says execute_async should return 21 (Timeout),
  // see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/execute_async.
  // However Selenium code returns 28, see
  // http://code.google.com/p/selenium/source/browse/trunk/javascript/firefox-driver/js/evaluate.js.
  // We'll stay compatible with the Selenium code.
  asyncTestTimeoutId = curFrame.setTimeout(function() {
    sandbox.asyncComplete('timed out', 28, null, asyncTestCommandId);
  }, msg.json.timeout);

  originalOnError = curFrame.onerror;
  curFrame.onerror = function errHandler(errMsg, url, line) {
    sandbox.asyncComplete(errMsg, 17, "@" + url + ", line " + line, asyncTestCommandId);
    curFrame.onerror = originalOnError;
  };

  let scriptSrc;
  if (useFinish) {
    if (msg.json.timeout == null || msg.json.timeout == 0) {
      sendError("Please set a timeout", 21, null, asyncTestCommandId);
    }
    scriptSrc = script;
  }
  else {
    try {
      sandbox.__marionetteParams = Cu.cloneInto(elementManager.convertWrappedArguments(
        msg.json.args, curFrame), sandbox, { wrapReflectors: true });
    }
    catch(e) {
      sendError(e.message, e.code, e.stack, asyncTestCommandId);
      return;
    }

    scriptSrc = "__marionetteParams.push(marionetteScriptFinished);" +
                "let __marionetteFunc = function() { " + script + "};" +
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
    Cu.evalInSandbox(scriptSrc, sandbox, "1.8", "dummy file", 0);
  } catch (e) {
    // 17 = JavascriptException
    let error = createStackMessage(e,
                                   "execute_async_script",
                                   msg.json.filename,
                                   msg.json.line,
                                   scriptSrc);
    sandbox.asyncComplete(error[0], 17, error[1], asyncTestCommandId);
  }
}

/**
 * This function creates a touch event given a touch type and a touch
 */
function emitTouchEvent(type, touch) {
  if (!wasInterrupted()) {
    let loggingInfo = "emitting Touch event of type " + type + " to element with id: " + touch.target.id + " and tag name: " + touch.target.tagName + " at coordinates (" + touch.clientX + ", " + touch.clientY + ") relative to the viewport";
    dumpLog(loggingInfo);
    var docShell = curFrame.document.defaultView.
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
    let domWindowUtils = curFrame.QueryInterface(Components.interfaces.nsIInterfaceRequestor).getInterface(Components.interfaces.nsIDOMWindowUtils);
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
function singleTap(msg) {
  let command_id = msg.json.command_id;
  try {
    let el = elementManager.getKnownElement(msg.json.id, curFrame);
    let acc = accessibility.getAccessibleObject(el, true);
    // after this block, the element will be scrolled into view
    let visible = checkVisible(el, msg.json.corx, msg.json.cory);
    checkVisibleAccessibility(acc, visible);
    if (!visible) {
      sendError("Element is not currently visible and may not be manipulated", 11, null, command_id);
      return;
    }
    checkActionableAccessibility(acc);
    if (!curFrame.document.createTouch) {
      actions.mouseEventsOnly = true;
    }
    let c = coordinates(el, msg.json.corx, msg.json.cory);
    if (!actions.mouseEventsOnly) {
      let touchId = actions.nextTouchId++;
      let touch = createATouch(el, c.x, c.y, touchId);
      emitTouchEvent('touchstart', touch);
      emitTouchEvent('touchend', touch);
    }
    actions.mouseTap(el.ownerDocument, c.x, c.y);
    sendOk(msg.json.command_id);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, msg.json.command_id);
  }
}

/**
 * Check if the element's unavailable accessibility state matches the enabled
 * state
 * @param nsIAccessible object
 * @param Boolean enabled element's enabled state
 */
function checkEnabledStateAccessibility(accesible, enabled) {
  if (!accesible) {
    return;
  }
  if (enabled && accessibility.matchState(accesible, 'STATE_UNAVAILABLE')) {
    accessibility.handleErrorMessage('Element is enabled but disabled via ' +
      'the accessibility API');
  }
}

/**
 * Check if the element's visible state corresponds to its accessibility API
 * visibility
 * @param nsIAccessible object
 * @param Boolean visible element's visibility state
 */
function checkVisibleAccessibility(accesible, visible) {
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
  accessibility.handleErrorMessage(message);
}

/**
 * Check if it is possible to activate an element with the accessibility API
 * @param nsIAccessible object
 */
function checkActionableAccessibility(accesible) {
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
  }
  accessibility.handleErrorMessage(message);
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
  callbacks.onSuccess = (value) => {
    sendResponse(value, command_id);
  };
  callbacks.onError = (message, code, trace) => {
    sendError(message, code, trace, msg.json.command_id);
  };

  let touchProvider = {};
  touchProvider.createATouch = createATouch;
  touchProvider.emitTouchEvent = emitTouchEvent;

  try {
    actions.dispatchActions(
        args,
        touchId,
        curFrame,
        elementManager,
        callbacks,
        touchProvider);
  } catch (e) {
    sendError(e.message, e.code, e.stack, command_id);
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
        el = elementManager.getKnownElement(pack[2], curFrame);
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
        el = elementManager.getKnownElement(pack[2], curFrame);
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
    let commandArray = elementManager.convertWrappedArguments(args, curFrame);
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
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, msg.json.command_id);
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
    if (pageTimeout == null || elapse <= pageTimeout) {
      if (curFrame.document.readyState == "complete") {
        callback();
        sendOk(command_id);
      } else if (curFrame.document.readyState == "interactive" &&
                 aboutErrorRegex.exec(curFrame.document.baseURI) &&
                 !curFrame.document.baseURI.startsWith(url)) {
        // We have reached an error url without requesting it.
        callback();
        sendError("Error loading page", 13, null, command_id);
      } else if (curFrame.document.readyState == "interactive" &&
                 curFrame.document.baseURI.startsWith("about:")) {
        callback();
        sendOk(command_id);
      } else {
        navTimer.initWithCallback(checkLoad, 100, Ci.nsITimer.TYPE_ONE_SHOT);
      }
    }
    else {
      callback();
      sendError("Error loading page, timed out (checkLoad)", 21, null,
                command_id);
    }
  }
  checkLoad();
}

/**
 * Navigate to the given URL.  The operation will be performed on the
 * current browser context, and handles the case where we navigate
 * within an iframe.  All other navigation is handled by the server
 * (in chrome space).
 */
function get(msg) {
  let start = new Date().getTime();

  // Prevent DOMContentLoaded events from frames from invoking this
  // code, unless the event is coming from the frame associated with
  // the current window (i.e. someone has used switch_to_frame).
  onDOMContentLoaded = function onDOMContentLoaded(event) {
    if (!event.originalTarget.defaultView.frameElement ||
        event.originalTarget.defaultView.frameElement == curFrame.frameElement) {
      pollForReadyState(msg, start, () => {
        removeEventListener("DOMContentLoaded", onDOMContentLoaded, false);
        onDOMContentLoaded = null;
      });
    }
  };

  function timerFunc() {
    removeEventListener("DOMContentLoaded", onDOMContentLoaded, false);
    sendError("Error loading page, timed out (onDOMContentLoaded)", 21,
              null, msg.json.command_id);
  }
  if (msg.json.pageTimeout != null) {
    navTimer.initWithCallback(timerFunc, msg.json.pageTimeout, Ci.nsITimer.TYPE_ONE_SHOT);
  }
  addEventListener("DOMContentLoaded", onDOMContentLoaded, false);
  curFrame.location = msg.json.url;
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
 * Get URL of the top level browsing context.
 */
function getCurrentUrl(msg) {
  let url;
  if (msg.json.isB2G) {
    url = curFrame.location.href;
  } else {
    url = content.location.href;
  }
  sendResponse({value: url}, msg.json.command_id);
}

/**
 * Get the current Title of the window
 */
function getTitle(msg) {
  sendResponse({value: curFrame.top.document.title}, msg.json.command_id);
}

/**
 * Get the current page source
 */
function getPageSource(msg) {
  var XMLSerializer = curFrame.XMLSerializer;
  var pageSource = new XMLSerializer().serializeToString(curFrame.document);
  sendResponse({value: pageSource}, msg.json.command_id);
}

/**
 * Go back in history
 */
function goBack(msg) {
  curFrame.history.back();
  sendOk(msg.json.command_id);
}

/**
 * Go forward in history
 */
function goForward(msg) {
  curFrame.history.forward();
  sendOk(msg.json.command_id);
}

/**
 * Refresh the page
 */
function refresh(msg) {
  let command_id = msg.json.command_id;
  curFrame.location.reload(true);
  let listen = function() {
    removeEventListener("DOMContentLoaded", arguments.callee, false);
    sendOk(command_id);
  };
  addEventListener("DOMContentLoaded", listen, false);
}

/**
 * Find an element in the document using requested search strategy
 */
function findElementContent(msg) {
  let command_id = msg.json.command_id;
  try {
    let on_success = function(el, cmd_id) { sendResponse({value: el}, cmd_id) };
    let on_error = function(e, cmd_id) { sendError(e.message, e.code, null, cmd_id); };
    elementManager.find(curFrame, msg.json, msg.json.searchTimeout,
                        false /* all */, on_success, on_error, command_id);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, command_id);
  }
}

/**
 * Find elements in the document using requested search strategy
 */
function findElementsContent(msg) {
  let command_id = msg.json.command_id;
  try {
    let on_success = function(els, cmd_id) { sendResponse({value: els}, cmd_id); };
    let on_error = function(e, cmd_id) { sendError(e.message, e.code, null, cmd_id); };
    elementManager.find(curFrame, msg.json, msg.json.searchTimeout,
                        true /* all */, on_success, on_error, command_id);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, command_id);
  }
}

/**
 * Find and return the active element on the page
 */
function getActiveElement(msg) {
  let command_id = msg.json.command_id;
  var element = curFrame.document.activeElement;
  var id = elementManager.addToKnownElements(element);
  sendResponse({value: id}, command_id);
}

/**
 * Send click event to element
 */
function clickElement(msg) {
  let command_id = msg.json.command_id;
  let el;
  try {
    el = elementManager.getKnownElement(msg.json.id, curFrame);
    let acc = accessibility.getAccessibleObject(el, true);
    let visible = checkVisible(el);
    checkVisibleAccessibility(acc, visible);
    if (visible) {
      checkActionableAccessibility(acc);
      if (utils.isElementEnabled(el)) {
        utils.synthesizeMouseAtCenter(el, {}, el.ownerDocument.defaultView)
      }
      else {
        sendError("Element is not Enabled", 12, null, command_id)
      }
    }
    else {
      sendError("Element is not visible", 11, null, command_id)
    }
    sendOk(command_id);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, command_id);
  }
}

/**
 * Get a given attribute of an element
 */
function getElementAttribute(msg) {
  let command_id = msg.json.command_id;
  try {
    let el = elementManager.getKnownElement(msg.json.id, curFrame);
    sendResponse({value: utils.getElementAttribute(el, msg.json.name)},
                 command_id);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, command_id);
  }
}

/**
 * Get the text of this element. This includes text from child elements.
 */
function getElementText(msg) {
  let command_id = msg.json.command_id;
  try {
    let el = elementManager.getKnownElement(msg.json.id, curFrame);
    sendResponse({value: utils.getElementText(el)}, command_id);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, command_id);
  }
}

/**
 * Get the tag name of an element.
 */
function getElementTagName(msg) {
  let command_id = msg.json.command_id;
  try {
    let el = elementManager.getKnownElement(msg.json.id, curFrame);
    sendResponse({value: el.tagName.toLowerCase()}, command_id);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, command_id);
  }
}

/**
 * Check if element is displayed
 */
function isElementDisplayed(msg) {
  let command_id = msg.json.command_id;
  try {
    let el = elementManager.getKnownElement(msg.json.id, curFrame);
    let displayed = utils.isElementDisplayed(el);
    checkVisibleAccessibility(accessibility.getAccessibleObject(el), displayed);
    sendResponse({value: displayed}, command_id);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, command_id);
  }
}

/**
 * Return the property of the computed style of an element
 *
 * @param object aRequest
 *               'element' member holds the reference id to
 *               the element that will be checked
 *               'propertyName' is the CSS rule that is being requested
 */
function getElementValueOfCssProperty(msg){
  let command_id = msg.json.command_id;
  let propertyName = msg.json.propertyName;
  try {
    let el = elementManager.getKnownElement(msg.json.id, curFrame);
    sendResponse({value: curFrame.document.defaultView.getComputedStyle(el, null).getPropertyValue(propertyName)},
                 command_id);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, command_id);
  }
}

/**
  * Submit a form on a content page by either using form or element in a form
  * @param object msg
  *               'json' JSON object containing 'id' member of the element
  */
function submitElement (msg) {
  let command_id = msg.json.command_id;
  try {
    let el = elementManager.getKnownElement(msg.json.id, curFrame);
    while (el.parentNode != null && el.tagName.toLowerCase() != 'form') {
      el = el.parentNode;
    }
    if (el.tagName && el.tagName.toLowerCase() == 'form') {
      el.submit();
      sendOk(command_id);
    }
    else {
      sendError("Element is not a form element or in a form", 7, null, command_id);
    }

  }
  catch (e) {
    sendError(e.message, e.code, e.stack, command_id);
  }
}

/**
 * Get the size of the element and return it
 */
function getElementSize(msg){
  let command_id = msg.json.command_id;
  try {
    let el = elementManager.getKnownElement(msg.json.id, curFrame);
    let clientRect = el.getBoundingClientRect();
    sendResponse({value: {width: clientRect.width, height: clientRect.height}},
                 command_id);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, command_id);
  }
}

/**
 * Get the size of the element and return it
 */
function getElementRect(msg){
  let command_id = msg.json.command_id;
  try {
    let el = elementManager.getKnownElement(msg.json.id, curFrame);
    let clientRect = el.getBoundingClientRect();
    sendResponse({value: {x: clientRect.x + curFrame.pageXOffset,
                          y: clientRect.y  + curFrame.pageYOffset,
                          width: clientRect.width,
                          height: clientRect.height}},
                 command_id);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, command_id);
  }
}

/**
 * Check if element is enabled
 */
function isElementEnabled(msg) {
  let command_id = msg.json.command_id;
  try {
    let el = elementManager.getKnownElement(msg.json.id, curFrame);
    let enabled = utils.isElementEnabled(el);
    checkEnabledStateAccessibility(accessibility.getAccessibleObject(el),
      enabled);
    sendResponse({value: enabled}, command_id);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, command_id);
  }
}

/**
 * Check if element is selected
 */
function isElementSelected(msg) {
  let command_id = msg.json.command_id;
  try {
    let el = elementManager.getKnownElement(msg.json.id, curFrame);
    sendResponse({value: utils.isElementSelected(el)}, command_id);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, command_id);
  }
}

/**
 * Send keys to element
 */
function sendKeysToElement(msg) {
  let command_id = msg.json.command_id;

  let el = elementManager.getKnownElement(msg.json.id, curFrame);
  let keysToSend = msg.json.value;

  utils.sendKeysToElement(curFrame, el, keysToSend, sendOk, sendError, command_id);
}

/**
 * Get the element's top left-hand corner point.
 */
function getElementLocation(msg) {
  let command_id = msg.json.command_id;
  try {
    let el = elementManager.getKnownElement(msg.json.id, curFrame);
    let rect = el.getBoundingClientRect();

    let location = {};
    location.x = rect.left;
    location.y = rect.top;

    sendResponse({value: location}, command_id);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, command_id);
  }
}

/**
 * Clear the text of an element
 */
function clearElement(msg) {
  let command_id = msg.json.command_id;
  try {
    let el = elementManager.getKnownElement(msg.json.id, curFrame);
    utils.clearElement(el);
    sendOk(command_id);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, command_id);
  }
}

/**
 * Switch to frame given either the server-assigned element id,
 * its index in window.frames, or the iframe's name or id.
 */
function switchToFrame(msg) {
  let command_id = msg.json.command_id;
  function checkLoad() {
    let errorRegex = /about:.+(error)|(blocked)\?/;
    if (curFrame.document.readyState == "complete") {
      sendOk(command_id);
      return;
    }
    else if (curFrame.document.readyState == "interactive" && errorRegex.exec(curFrame.document.baseURI)) {
      sendError("Error loading page", 13, null, command_id);
      return;
    }
    checkTimer.initWithCallback(checkLoad, 100, Ci.nsITimer.TYPE_ONE_SHOT);
  }
  let foundFrame = null;
  let frames = [];
  let parWindow = null;
  // Check of the curFrame reference is dead
  try {
    frames = curFrame.frames;
    //Until Bug 761935 lands, we won't have multiple nested OOP iframes. We will only have one.
    //parWindow will refer to the iframe above the nested OOP frame.
    parWindow = curFrame.QueryInterface(Ci.nsIInterfaceRequestor)
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

    curFrame = content;
    if(msg.json.focus == true) {
      curFrame.focus();
    }
    sandbox = null;
    checkTimer.initWithCallback(checkLoad, 100, Ci.nsITimer.TYPE_ONE_SHOT);
    return;
  }
  if (msg.json.element != undefined) {
    if (elementManager.seenItems[msg.json.element] != undefined) {
      let wantedFrame;
      try {
        wantedFrame = elementManager.getKnownElement(msg.json.element, curFrame); //Frame Element
      }
      catch(e) {
        sendError(e.message, e.code, e.stack, command_id);
      }

      if (frames.length > 0) {
        for (let i = 0; i < frames.length; i++) {
          // use XPCNativeWrapper to compare elements; see bug 834266
          if (XPCNativeWrapper(frames[i].frameElement) == XPCNativeWrapper(wantedFrame)) {
            curFrame = frames[i].frameElement;
            foundFrame = i;
          }
        }
      }
      if (foundFrame === null) {
        // Either the frame has been removed or we have a OOP frame
        // so lets just get all the iframes and do a quick loop before
        // throwing in the towel
        let iframes = curFrame.document.getElementsByTagName("iframe");
        for (var i = 0; i < iframes.length; i++) {
          if (XPCNativeWrapper(iframes[i]) == XPCNativeWrapper(wantedFrame)) {
            curFrame = iframes[i];
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
          curFrame = foundFrame;
          foundFrame = elementManager.addToKnownElements(curFrame);
        }
        else {
          // If foundFrame is null at this point then we have the top level browsing
          // context so should treat it accordingly.
          sendSyncMessage("Marionette:switchedToFrame", { frameValue: null});
          curFrame = content;
          if(msg.json.focus == true) {
            curFrame.focus();
          }
          sandbox = null;
          checkTimer.initWithCallback(checkLoad, 100, Ci.nsITimer.TYPE_ONE_SHOT);
          return;
        }
      } catch (e) {
        // Since window.frames does not return OOP frames it will throw
        // and we land up here. Let's not give up and check if there are
        // iframes and switch to the indexed frame there
        let iframes = curFrame.document.getElementsByTagName("iframe");
        if (msg.json.id >= 0 && msg.json.id < iframes.length) {
          curFrame = iframes[msg.json.id];
          foundFrame = msg.json.id;
        }
      }
    }
  }
  if (foundFrame === null) {
    sendError("Unable to locate frame: " + (msg.json.id || msg.json.element), 8, null, command_id);
    return true;
  }

  sandbox = null;

  // send a synchronous message to let the server update the currently active
  // frame element (for getActiveFrame)
  let frameValue = elementManager.wrapValue(curFrame.wrappedJSObject)['ELEMENT'];
  sendSyncMessage("Marionette:switchedToFrame", { frameValue: frameValue });

  let rv = null;
  if (curFrame.contentWindow === null) {
    // The frame we want to switch to is a remote/OOP frame;
    // notify our parent to handle the switch
    curFrame = content;
    rv = {win: parWindow, frame: foundFrame};
  } else {
    curFrame = curFrame.contentWindow;
    if (msg.json.focus)
      curFrame.focus();
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
    var location = curFrame.document.location;
    cookie.domain = location.hostname;
  }
  else {
    var currLocation = curFrame.location;
    var currDomain = currLocation.host;
    if (currDomain.indexOf(cookie.domain) == -1) {
      sendError("You may only set cookies for the current domain", 24, null, msg.json.command_id);
    }
  }

  // The cookie's domain may include a port. Which is bad. Remove it
  // We'll catch ip6 addresses by mistake. Since no-one uses those
  // this will be okay for now. See Bug 814416
  if (cookie.domain.match(/:\d+$/)) {
    cookie.domain = cookie.domain.replace(/:\d+$/, '');
  }

  var document = curFrame.document;
  if (!document || !document.contentType.match(/html/i)) {
    sendError('You may only set cookies on html documents', 25, null, msg.json.command_id);
  }

  let added = sendSyncMessage("Marionette:addCookie", {value: cookie});
  if (added[0] !== true) {
    sendError("Error setting cookie", 13, null, msg.json.command_id);
    return;
  }
  sendOk(msg.json.command_id);
}

/**
 * Get all cookies for the current domain.
 */
function getCookies(msg) {
  var toReturn = [];
  var cookies = getVisibleCookies(curFrame.location);
  for (let cookie of cookies) {
    var expires = cookie.expires;
    if (expires == 0) {  // Session cookie, don't return an expiry.
      expires = null;
    } else if (expires == 1) { // Date before epoch time, cap to epoch.
      expires = 0;
    }
    toReturn.push({
      'name': cookie.name,
      'value': cookie.value,
      'path': cookie.path,
      'domain': cookie.host,
      'secure': cookie.isSecure,
      'expiry': expires
    });
  }
  sendResponse({value: toReturn}, msg.json.command_id);
}

/**
 * Delete a cookie by name
 */
function deleteCookie(msg) {
  let toDelete = msg.json.name;
  let cookies = getVisibleCookies(curFrame.location);
  for (let cookie of cookies) {
    if (cookie.name == toDelete) {
      let deleted = sendSyncMessage("Marionette:deleteCookie", {value: cookie});
      if (deleted[0] !== true) {
        sendError("Could not delete cookie: " + msg.json.name, 13, null, msg.json.command_id);
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
  let cookies = getVisibleCookies(curFrame.location);
  for (let cookie of cookies) {
    let deleted = sendSyncMessage("Marionette:deleteCookie", {value: cookie});
    if (!deleted[0]) {
      sendError("Could not delete cookie: " + JSON.stringify(cookie), 13, null, msg.json.command_id);
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
  sendResponse({ value: curFrame.applicationCache.status },
               msg.json.command_id);
}

// emulator callbacks
let _emu_cb_id = 0;
let _emu_cbs = {};

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
  if (!sandbox) {
    return;
  }
  let cb = _emu_cbs[message.id];
  delete _emu_cbs[message.id];
  if (!cb) {
    return;
  }
  try {
    cb(message.result);
  }
  catch(e) {
    sendError(e.message, e.code, e.stack, -1);
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
 * Takes a screen capture of the given web element if <code>id</code>
 * property exists in the message's JSON object, or if null captures
 * the bounding box of the current frame.
 *
 * If given an array of web element references in
 * <code>msg.json.highlights</code>, a red box will be painted around
 * them to highlight their position.
 */
function takeScreenshot(msg) {
  let node = null;
  if (msg.json.id) {
    try {
      node = elementManager.getKnownElement(msg.json.id, curFrame)
    }
    catch (e) {
      sendResponse(e.message, e.code, e.stack, msg.json.command_id);
      return;
    }
  }
  else {
    node = curFrame;
  }
  let highlights = msg.json.highlights;

  var document = curFrame.document;
  var rect, win, width, height, left, top;
  // node can be either a window or an arbitrary DOM node
  if (node == curFrame) {
    // node is a window
    win = node;
    if (msg.json.full) {
      // the full window
      width = document.body.scrollWidth;
      height = document.body.scrollHeight;
      top = 0;
      left = 0;
    }
    else {
      // only the viewport
      width = document.documentElement.clientWidth;
      height = document.documentElement.clientHeight;
      left = curFrame.pageXOffset;
      top = curFrame.pageYOffset;
    }
  }
  else {
    // node is an arbitrary DOM node
    win = node.ownerDocument.defaultView;
    rect = node.getBoundingClientRect();
    width = rect.width;
    height = rect.height;
    top = rect.top;
    left = rect.left;
  }

  var canvas = document.createElementNS("http://www.w3.org/1999/xhtml",
                                        "canvas");
  canvas.width = width;
  canvas.height = height;
  var ctx = canvas.getContext("2d");
  // Draws the DOM contents of the window to the canvas
  ctx.drawWindow(win, left, top, width, height, "rgb(255,255,255)");

  // This section is for drawing a red rectangle around each element
  // passed in via the highlights array
  if (highlights) {
    ctx.lineWidth = "2";
    ctx.strokeStyle = "red";
    ctx.save();

    for (var i = 0; i < highlights.length; ++i) {
      var elem = elementManager.getKnownElement(highlights[i], curFrame);
      rect = elem.getBoundingClientRect();

      var offsetY = -top;
      var offsetX = -left;

      // Draw the rectangle
      ctx.strokeRect(rect.left + offsetX,
                     rect.top + offsetY,
                     rect.width,
                     rect.height);
    }
  }

  // Return the Base64 encoded string back to the client so that it
  // can save the file to disk if it is required
  var dataUrl = canvas.toDataURL("image/png", "");
  var data = dataUrl.substring(dataUrl.indexOf(",") + 1);
  sendResponse({value: data}, msg.json.command_id);
}

// Call register self when we get loaded
registerSelf();
