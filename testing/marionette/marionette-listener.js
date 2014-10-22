/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

let uuidGen = Cc["@mozilla.org/uuid-generator;1"]
                .getService(Ci.nsIUUIDGenerator);

let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
               .getService(Ci.mozIJSSubScriptLoader);

loader.loadSubScript("chrome://marionette/content/marionette-simpletest.js");
loader.loadSubScript("chrome://marionette/content/marionette-common.js");
Cu.import("chrome://marionette/content/marionette-elements.js");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
let utils = {};
utils.window = content;
// Load Event/ChromeUtils for use with JS scripts:
loader.loadSubScript("chrome://marionette/content/EventUtils.js", utils);
loader.loadSubScript("chrome://marionette/content/ChromeUtils.js", utils);
loader.loadSubScript("chrome://marionette/content/atoms.js", utils);
loader.loadSubScript("chrome://marionette/content/marionette-sendkeys.js", utils);

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
let importedScripts = null;
let inputSource = null;

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
// Send move events about this often
let EVENT_INTERVAL = 30; // milliseconds
// For assigning unique ids to all touches
let nextTouchId = 1000;
//Keep track of active Touches
let touchIds = {};
// last touch for each fingerId
let multiLast = {};
let lastCoordinates = null;
let isTap = false;
let scrolling = false;
// whether to send mouse event
let mouseEventsOnly = false;

Cu.import("resource://gre/modules/Log.jsm");
let logger = Log.repository.getLogger("Marionette");
logger.info("loaded marionette-listener.js");
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
  let msg = {value: winUtil.outerWindowID, href: content.location.href};
  // register will have the ID and a boolean describing if this is the main process or not
  let register = sendSyncMessage("Marionette:register", msg);

  if (register[0]) {
    listenerId = register[0][0].id;
    if (typeof listenerId != "undefined") {
      // check if we're the main process
      if (register[0][1] == true) {
        addMessageListener("MarionetteMainListener:emitTouchEvent", emitTouchEventForIFrame);
      }
      importedScripts = FileUtils.getDir('TmpD', [], false);
      importedScripts.append('marionetteContentScripts');
      startListeners();
    }
  }
}

function emitTouchEventForIFrame(message) {
  message = message.json;
  let frames = curFrame.document.getElementsByTagName("iframe");
  let iframe = frames[message.index];
  let identifier = nextTouchId;
  let tabParent = iframe.QueryInterface(Components.interfaces.nsIFrameLoaderOwner).frameLoader.tabParent;
  tabParent.injectTouchEvent(message.type, [identifier],
                             [message.clientX], [message.clientY],
                             [message.radiusX], [message.radiusY],
                             [message.rotationAngle], [message.force],
                             1, 0);
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
  resetValues();
  if (isB2G) {
    readyStateTimer.initWithCallback(waitForReady, 100, Ci.nsITimer.TYPE_ONE_SHOT);
    // We have to set correct mouse event source to MOZ_SOURCE_TOUCH
    // to offer a way for event listeners to differentiate
    // events being the result of a physical mouse action.
    // This is especially important for the touch event shim,
    // in order to prevent creating touch event for these fake mouse events.
    inputSource = Ci.nsIDOMMouseEvent.MOZ_SOURCE_TOUCH;
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
  touchIds = {};
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
function sendError(message, status, trace, command_id) {
  let error_msg = { message: message, status: status, stacktrace: trace };
  sendToServer("Marionette:error", error_msg, command_id);
}

/**
 * Clear test values after completion of test
 */
function resetValues() {
  sandbox = null;
  curFrame = content;
  mouseEventsOnly = false;
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
    if (docShell.asyncPanZoomEnabled && scrolling) {
      // if we're in APZ and we're scrolling, we must use injectTouchEvent to dispatch our touchmove events
      let index = sendSyncMessage("MarionetteFrame:getCurrentFrameId");
      // only call emitTouchEventForIFrame if we're inside an iframe.
      if (index != null) {
        sendSyncMessage("Marionette:emitTouchEvent", {index: index, type: type, id: touch.identifier,
                                                      clientX: touch.clientX, clientY: touch.clientY,
                                                      radiusX: touch.radiusX, radiusY: touch.radiusY,
                                                      rotation: touch.rotationAngle, force: touch.force});
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
 * This function emit mouse event
 *   @param: doc is the current document
 *           type is the type of event to dispatch
 *           clickCount is the number of clicks, button notes the mouse button
 *           elClientX and elClientY are the coordinates of the mouse relative to the viewport
 */
function emitMouseEvent(doc, type, elClientX, elClientY, clickCount, button) {
  if (!wasInterrupted()) {
    let loggingInfo = "emitting Mouse event of type " + type + " at coordinates (" + elClientX + ", " + elClientY + ") relative to the viewport";
    dumpLog(loggingInfo);
    /*
    Disabled per bug 888303
    marionetteLogObj.log(loggingInfo, "TRACE");
    sendSyncMessage("Marionette:shareData",
                    {log: elementManager.wrapValue(marionetteLogObj.getLogs())});
    marionetteLogObj.clearLogs();
    */
    let win = doc.defaultView;
    let domUtils = win.QueryInterface(Components.interfaces.nsIInterfaceRequestor).getInterface(Components.interfaces.nsIDOMWindowUtils);
    domUtils.sendMouseEvent(type, elClientX, elClientY, button || 0, clickCount || 1, 0, false, 0, inputSource);
  }
}

/**
 * Helper function that perform a mouse tap
 */
function mousetap(doc, x, y) {
  emitMouseEvent(doc, 'mousemove', x, y);
  emitMouseEvent(doc, 'mousedown', x, y);
  emitMouseEvent(doc, 'mouseup', x, y);
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
  //check if the element is visible
  let visible = utils.isElementDisplayed(el);
  if (!visible) {
    return false;
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

//x and y are coordinates relative to the viewport
function generateEvents(type, x, y, touchId, target) {
  lastCoordinates = [x, y];
  let doc = curFrame.document;
  switch (type) {
    case 'tap':
      if (mouseEventsOnly) {
        mousetap(target.ownerDocument, x, y);
      }
      else {
        let touchId = nextTouchId++;
        let touch = createATouch(target, x, y, touchId);
        emitTouchEvent('touchstart', touch);
        emitTouchEvent('touchend', touch);
        mousetap(target.ownerDocument, x, y);
      }
      lastCoordinates = null;
      break;
    case 'press':
      isTap = true;
      if (mouseEventsOnly) {
        emitMouseEvent(doc, 'mousemove', x, y);
        emitMouseEvent(doc, 'mousedown', x, y);
      }
      else {
        let touchId = nextTouchId++;
        let touch = createATouch(target, x, y, touchId);
        emitTouchEvent('touchstart', touch);
        touchIds[touchId] = touch;
        return touchId;
      }
      break;
    case 'release':
      if (mouseEventsOnly) {
        emitMouseEvent(doc, 'mouseup', lastCoordinates[0], lastCoordinates[1]);
      }
      else {
        let touch = touchIds[touchId];
        touch = createATouch(touch.target, lastCoordinates[0], lastCoordinates[1], touchId);
        emitTouchEvent('touchend', touch);
        if (isTap) {
          mousetap(touch.target.ownerDocument, touch.clientX, touch.clientY);
        }
        delete touchIds[touchId];
      }
      isTap = false;
      lastCoordinates = null;
      break;
    case 'cancel':
      isTap = false;
      if (mouseEventsOnly) {
        emitMouseEvent(doc, 'mouseup', lastCoordinates[0], lastCoordinates[1]);
      }
      else {
        emitTouchEvent('touchcancel', touchIds[touchId]);
        delete touchIds[touchId];
      }
      lastCoordinates = null;
      break;
    case 'move':
      isTap = false;
      if (mouseEventsOnly) {
        emitMouseEvent(doc, 'mousemove', x, y);
      }
      else {
        touch = createATouch(touchIds[touchId].target, x, y, touchId);
        touchIds[touchId] = touch;
        emitTouchEvent('touchmove', touch);
      }
      break;
    case 'contextmenu':
      isTap = false;
      let event = curFrame.document.createEvent('MouseEvents');
      if (mouseEventsOnly) {
        target = doc.elementFromPoint(lastCoordinates[0], lastCoordinates[1]);
      }
      else {
        target = touchIds[touchId].target;
      }
      let [ clientX, clientY,
            pageX, pageY,
            screenX, screenY ] = getCoordinateInfo(target, x, y);
      event.initMouseEvent('contextmenu', true, true,
                           target.ownerDocument.defaultView, 1,
                           screenX, screenY, clientX, clientY,
                           false, false, false, false, 0, null);
      target.dispatchEvent(event);
      break;
    default:
      throw {message:"Unknown event type: " + type, code: 500, stack:null};
  }
  if (wasInterrupted()) {
    if (previousFrame) {
      //if previousFrame is set, then we're in a single process environment
      curFrame = previousFrame;
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

/**
 * Function that perform a single tap
 */
function singleTap(msg) {
  let command_id = msg.json.command_id;
  try {
    let el = elementManager.getKnownElement(msg.json.id, curFrame);
    // after this block, the element will be scrolled into view
    if (!checkVisible(el, msg.json.corx, msg.json.cory)) {
       sendError("Element is not currently visible and may not be manipulated", 11, null, command_id);
       return;
    }
    if (!curFrame.document.createTouch) {
      mouseEventsOnly = true;
    }
    c = coordinates(el, msg.json.corx, msg.json.cory);
    generateEvents('tap', c.x, c.y, null, el);
    sendOk(msg.json.command_id);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, msg.json.command_id);
  }
}

/**
 * Given an element and a pair of coordinates, returns an array of the form
 * [ clientX, clientY, pageX, pageY, screenX, screenY ]
 */
function getCoordinateInfo(el, corx, cory) {
  let win = el.ownerDocument.defaultView;
  return [ corx, // clientX
           cory, // clientY
           corx + win.pageXOffset, // pageX
           cory + win.pageYOffset, // pageY
           corx + win.mozInnerScreenX, // screenX
           cory + win.mozInnerScreenY // screenY
         ];
}

/**
 * Function to create a touch based on the element
 * corx and cory are relative to the viewport, id is the touchId
 */
function createATouch(el, corx, cory, touchId) {
  let doc = el.ownerDocument;
  let win = doc.defaultView;
  let [clientX, clientY, pageX, pageY, screenX, screenY] =
    getCoordinateInfo(el, corx, cory);
  let atouch = doc.createTouch(win, el, touchId, pageX, pageY, screenX, screenY, clientX, clientY);
  return atouch;
}

/**
 * Function to emit touch events for each finger. e.g. finger=[['press', id], ['wait', 5], ['release']]
 * touchId represents the finger id, i keeps track of the current action of the chain
 */
function actions(chain, touchId, command_id, i) {
  if (typeof i === "undefined") {
    i = 0;
  }
  if (i == chain.length) {
    sendResponse({value: touchId}, command_id);
    return;
  }
  let pack = chain[i];
  let command = pack[0];
  let el;
  let c;
  i++;
  if (command != 'press' && command != 'wait') {
    //if mouseEventsOnly, then touchIds isn't used
    if (!(touchId in touchIds) && !mouseEventsOnly) {
      sendError("Element has not been pressed", 500, null, command_id);
      return;
    }
  }
  switch(command) {
    case 'press':
      if (lastCoordinates) {
        generateEvents('cancel', lastCoordinates[0], lastCoordinates[1], touchId);
        sendError("Invalid Command: press cannot follow an active touch event", 500, null, command_id);
        return;
      }
      // look ahead to check if we're scrolling. Needed for APZ touch dispatching.
      if ((i != chain.length) && (chain[i][0].indexOf('move') !== -1)) {
        scrolling = true;
      }
      el = elementManager.getKnownElement(pack[1], curFrame);
      c = coordinates(el, pack[2], pack[3]);
      touchId = generateEvents('press', c.x, c.y, null, el);
      actions(chain, touchId, command_id, i);
      break;
    case 'release':
      generateEvents('release', lastCoordinates[0], lastCoordinates[1], touchId);
      actions(chain, null, command_id, i);
      scrolling =  false;
      break;
    case 'move':
      el = elementManager.getKnownElement(pack[1], curFrame);
      c = coordinates(el);
      generateEvents('move', c.x, c.y, touchId);
      actions(chain, touchId, command_id, i);
      break;
    case 'moveByOffset':
      generateEvents('move', lastCoordinates[0] + pack[1], lastCoordinates[1] + pack[2], touchId);
      actions(chain, touchId, command_id, i);
      break;
    case 'wait':
      if (pack[1] != null ) {
        let time = pack[1]*1000;
        // standard waiting time to fire contextmenu
        let standard = 750;
        try {
          standard = Services.prefs.getIntPref("ui.click_hold_context_menus.delay");
        }
        catch (e){}
        if (time >= standard && isTap) {
            chain.splice(i, 0, ['longPress'], ['wait', (time-standard)/1000]);
            time = standard;
        }
        checkTimer.initWithCallback(function(){actions(chain, touchId, command_id, i);}, time, Ci.nsITimer.TYPE_ONE_SHOT);
      }
      else {
        actions(chain, touchId, command_id, i);
      }
      break;
    case 'cancel':
      generateEvents('cancel', lastCoordinates[0], lastCoordinates[1], touchId);
      actions(chain, touchId, command_id, i);
      scrolling = false;
      break;
    case 'longPress':
      generateEvents('contextmenu', lastCoordinates[0], lastCoordinates[1], touchId);
      actions(chain, touchId, command_id, i);
      break;
  }
}

/**
 * Function to start action chain on one finger
 */
function actionChain(msg) {
  let command_id = msg.json.command_id;
  let args = msg.json.chain;
  let touchId = msg.json.nextId;
  try {
    let commandArray = elementManager.convertWrappedArguments(args, curFrame);
    // loop the action array [ ['press', id], ['move', id], ['release', id] ]
    if (touchId == null) {
      touchId = nextTouchId++;
    }
    if (!curFrame.document.createTouch) {
      mouseEventsOnly = true;
    }
    actions(commandArray, touchId, command_id);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, msg.json.command_id);
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

/**
 * Navigate to the given URL.  The operation will be performed on the
 * current browser context, and handles the case where we navigate
 * within an iframe.  All other navigation is handled by the server
 * (in chrome space).
 */
function get(msg) {
  let command_id = msg.json.command_id;

  let checkTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  let start = new Date().getTime();
  let end = null;
  function checkLoad() {
    checkTimer.cancel();
    end = new Date().getTime();
    let errorRegex = /about:.+(error)|(blocked)\?/;
    let elapse = end - start;
    if (msg.json.pageTimeout == null || elapse <= msg.json.pageTimeout) {
      if (curFrame.document.readyState == "complete") {
        removeEventListener("DOMContentLoaded", onDOMContentLoaded, false);
        sendOk(command_id);
      }
      else if (curFrame.document.readyState == "interactive" &&
               errorRegex.exec(curFrame.document.baseURI)) {
        removeEventListener("DOMContentLoaded", onDOMContentLoaded, false);
        sendError("Error loading page", 13, null, command_id);
      }
      else {
        checkTimer.initWithCallback(checkLoad, 100, Ci.nsITimer.TYPE_ONE_SHOT);
      }
    }
    else {
      removeEventListener("DOMContentLoaded", onDOMContentLoaded, false);
      sendError("Error loading page, timed out (checkLoad)", 21, null,
                command_id);
    }
  }
  // Prevent DOMContentLoaded events from frames from invoking this
  // code, unless the event is coming from the frame associated with
  // the current window (i.e. someone has used switch_to_frame).
  let onDOMContentLoaded = function onDOMContentLoaded(event) {
    if (!event.originalTarget.defaultView.frameElement ||
        event.originalTarget.defaultView.frameElement == curFrame.frameElement) {
      checkLoad();
    }
  };

  function timerFunc() {
    removeEventListener("DOMContentLoaded", onDOMContentLoaded, false);
    sendError("Error loading page, timed out (onDOMContentLoaded)", 21,
              null, command_id);
  }
  if (msg.json.pageTimeout != null) {
    checkTimer.initWithCallback(timerFunc, msg.json.pageTimeout, Ci.nsITimer.TYPE_ONE_SHOT);
  }
  addEventListener("DOMContentLoaded", onDOMContentLoaded, false);
  curFrame.location = msg.json.url;
}

/**
 * Get URL of the top level browsing context.
 */
function getCurrentUrl(msg) {
  sendResponse({value: curFrame.location.href}, msg.json.command_id);
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
    let on_success = function(id, cmd_id) { sendResponse({value:id}, cmd_id); };
    let on_error = sendError;
    elementManager.find(curFrame, msg.json, msg.json.searchTimeout,
                        on_success, on_error, false, command_id);
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
    let on_success = function(id, cmd_id) { sendResponse({value:id}, cmd_id); };
    let on_error = sendError;
    elementManager.find(curFrame, msg.json, msg.json.searchTimeout,
                        on_success, on_error, true, command_id);
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
    if (checkVisible(el)) {
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
    sendResponse({value: utils.isElementDisplayed(el)}, command_id);
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
    sendResponse({value: utils.isElementEnabled(el)}, command_id);
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
        curFrame = iframes[msg.json.id];
        foundFrame = msg.json.id
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

  if (curFrame.contentWindow == null) {
    // The frame we want to switch to is a remote (out-of-process) frame;
    // notify our parent to handle the switch.
    curFrame = content;
    sendToServer('Marionette:switchToFrame', {win: parWindow,
                                              frame: foundFrame,
                                              command_id: command_id});
  }
  else {
    curFrame = curFrame.contentWindow;
    if(msg.json.focus == true) {
      curFrame.focus();
    }
    checkTimer.initWithCallback(checkLoad, 100, Ci.nsITimer.TYPE_ONE_SHOT);
  }
}
 /**
  * Add a cookie to the document
  */
function addCookie(msg) {
  cookie = msg.json.cookie;

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
  let cookieManager;
  try {
    // Retrieving the cookie manager fails with e10s enabled.
    cookieManager = Cc['@mozilla.org/cookiemanager;1'].
                      getService(Ci.nsICookieManager2);
  } catch (ex) {
    sendError("Error retrieving cookie manager: " + ex, 13, null, msg.json.command_id);
    return;
  }
  cookieManager.add(cookie.domain, cookie.path, cookie.name, cookie.value,
                   cookie.secure, false, false, cookie.expiry);
  sendOk(msg.json.command_id);
}

/**
 * Get all cookies for the current domain.
 */
function getCookies(msg) {
  var toReturn = [];
  var cookies = getVisibleCookies(curFrame.location);
  if (typeof cookies == "undefined") {
    sendError("Error retrieving cookie manager", 13, null, msg.json.command_id);
    return;
  }
  for (var i = 0; i < cookies.length; i++) {
    var cookie = cookies[i];
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
  var toDelete = msg.json.name;

  let cookieManager;
  try {
    // Retrieving the cookie manager fails with e10s enabled.
    cookieManager = Cc['@mozilla.org/cookiemanager;1'].
                      getService(Ci.nsICookieManager);
  } catch (ex) {
    sendError("Error retrieving cookie manager: " + ex, 13, null, msg.json.command_id);
    return;
  }

  var cookies = getVisibleCookies(curFrame.location);
  for (var i = 0; i < cookies.length; i++) {
    var cookie = cookies[i];
    if (cookie.name == toDelete) {
      cookieManager.remove(cookie.host, cookie.name, cookie.path, false);
    }
  }

  sendOk(msg.json.command_id);
}

/**
 * Delete all the visibile cookies on a page
 */
function deleteAllCookies(msg) {
  let cookieManager;
  try {
    // Retrieving the cookie manager fails with e10s enabled.
    cookieManager = Cc['@mozilla.org/cookiemanager;1'].
                      getService(Ci.nsICookieManager);
  } catch (ex) {
    sendError("Error retrieving cookie manager: " + ex, 13, null, msg.json.command_id);
    return;
  }
  let cookies = getVisibleCookies(curFrame.location);
  for (let i = 0; i < cookies.length; i++) {
    let cookie = cookies[i];
    cookieManager.remove(cookie.host, cookie.name, cookie.path, false);
  }
  sendOk(msg.json.command_id);
}

/**
 * Get all the visible cookies from a location
 */
function getVisibleCookies(location) {
  let results = [];
  let currentPath = location.pathname;
  if (!currentPath) currentPath = '/';
  let isForCurrentPath = function(aPath) {
    return currentPath.indexOf(aPath) != -1;
  }

  let cookieManager;
  try {
    // Retrieving the cookie manager fails with e10s enabled.
    cookieManager = Cc['@mozilla.org/cookiemanager;1'].
                      getService(Ci.nsICookieManager);
  } catch (ex) {
    return;
  }

  let enumerator = cookieManager.enumerator;
  while (enumerator.hasMoreElements()) {
    let cookie = enumerator.getNext().QueryInterface(Ci['nsICookie']);

    // Take the hostname and progressively shorten
    let hostname = location.hostname;
    do {
      if ((cookie.host == '.' + hostname || cookie.host == hostname)
          && isForCurrentPath(cookie.path)) {
          results.push(cookie);
          break;
      }
      hostname = hostname.replace(/^.*?\./, '');
    } while (hostname.indexOf('.') != -1);
  }

  return results;
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
    width = document.body.scrollWidth;
    height = document.body.scrollHeight;
    top = 0;
    left = 0;
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
