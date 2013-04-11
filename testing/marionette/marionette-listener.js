/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

let uuidGen = Cc["@mozilla.org/uuid-generator;1"]
                .getService(Ci.nsIUUIDGenerator);

let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
               .getService(Ci.mozIJSSubScriptLoader);

loader.loadSubScript("chrome://marionette/content/marionette-simpletest.js");
loader.loadSubScript("chrome://marionette/content/marionette-log-obj.js");
loader.loadSubScript("chrome://marionette/content/marionette-perf.js");
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
let marionettePerf = new MarionettePerfData();

let isB2G = false;

let marionetteTestName;
let winUtil = content.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDOMWindowUtils);
let listenerId = null; //unique ID of this listener
let activeFrame = null;
let curWindow = content;
let elementManager = new ElementManager([]);
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
let originalOnError;
//timer for doc changes
let checkTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
// Send move events about this often
let EVENT_INTERVAL = 30; // milliseconds
// The current array of all pending touches
let touches = [];
// For assigning unique ids to all touches
let nextTouchId = 1000;
let touchIds = {};
// last touch for each fingerId
let multiLast = {};
// last touch for single finger
let lastTouch = null;
/**
 * Called when listener is first started up. 
 * The listener sends its unique window ID and its current URI to the actor.
 * If the actor returns an ID, we start the listeners. Otherwise, nothing happens.
 */
function registerSelf() {
  let msg = {value: winUtil.outerWindowID, href: content.location.href};
  let register = sendSyncMessage("Marionette:register", msg);

  if (register[0]) {
    listenerId = register[0].id;
    importedScripts = FileUtils.File(register[0].importedScripts);
    startListeners();
  }
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
  addMessageListenerId("Marionette:doubleTap", doubleTap);
  addMessageListenerId("Marionette:press", press);
  addMessageListenerId("Marionette:release", release);
  addMessageListenerId("Marionette:cancelTouch", cancelTouch);
  addMessageListenerId("Marionette:actionChain", actionChain);
  addMessageListenerId("Marionette:multiAction", multiAction);
  addMessageListenerId("Marionette:setSearchTimeout", setSearchTimeout);
  addMessageListenerId("Marionette:goUrl", goUrl);
  addMessageListenerId("Marionette:getUrl", getUrl);
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
  addMessageListenerId("Marionette:getElementSize", getElementSize);
  addMessageListenerId("Marionette:isElementEnabled", isElementEnabled);
  addMessageListenerId("Marionette:isElementSelected", isElementSelected);
  addMessageListenerId("Marionette:sendKeysToElement", sendKeysToElement);
  addMessageListenerId("Marionette:getElementPosition", getElementPosition);
  addMessageListenerId("Marionette:clearElement", clearElement);
  addMessageListenerId("Marionette:switchToFrame", switchToFrame);
  addMessageListenerId("Marionette:deleteSession", deleteSession);
  addMessageListenerId("Marionette:sleepSession", sleepSession);
  addMessageListenerId("Marionette:emulatorCmdResult", emulatorCmdResult);
  addMessageListenerId("Marionette:importScript", importScript);
  addMessageListenerId("Marionette:getAppCacheStatus", getAppCacheStatus);
  addMessageListenerId("Marionette:setTestName", setTestName);
  addMessageListenerId("Marionette:setState", setState);
  addMessageListenerId("Marionette:screenShot", screenShot);
  addMessageListenerId("Marionette:addCookie", addCookie);
  addMessageListenerId("Marionette:getAllCookies", getAllCookies);
  addMessageListenerId("Marionette:deleteAllCookies", deleteAllCookies);
  addMessageListenerId("Marionette:deleteCookie", deleteCookie);
}

/**
 * Called when we start a new session. It registers the
 * current environment, and resets all values
 */
function newSession(msg) {
  isB2G = msg.json.B2G;
  resetValues();
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
 * Sets script-wide state variables; used after frame switching.
 */
function setState(msg) {
  marionetteTimeout = msg.json.scriptTimeout;
  try {
    elementManager.setSearchTimeout(msg.json.searchTimeout);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, msg.json.command_id);
    return;
  }
  sendOk(msg.json.command_id);
}

/**
 * Restarts all our listeners after this listener was put to sleep
 */
function restart(msg) {
  removeMessageListener("Marionette:restart", restart);
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
  removeMessageListenerId("Marionette:doubleTap", doubleTap);
  removeMessageListenerId("Marionette:press", press);
  removeMessageListenerId("Marionette:release", release);
  removeMessageListenerId("Marionette:cancelTouch", cancelTouch);
  removeMessageListenerId("Marionette:actionChain", actionChain);
  removeMessageListenerId("Marionette:multiAction", multiAction);
  removeMessageListenerId("Marionette:setSearchTimeout", setSearchTimeout);
  removeMessageListenerId("Marionette:goUrl", goUrl);
  removeMessageListenerId("Marionette:getTitle", getTitle);
  removeMessageListenerId("Marionette:getPageSource", getPageSource);
  removeMessageListenerId("Marionette:getUrl", getUrl);
  removeMessageListenerId("Marionette:goBack", goBack);
  removeMessageListenerId("Marionette:goForward", goForward);
  removeMessageListenerId("Marionette:refresh", refresh);
  removeMessageListenerId("Marionette:findElementContent", findElementContent);
  removeMessageListenerId("Marionette:findElementsContent", findElementsContent);
  removeMessageListenerId("Marionette:getActiveElement", getActiveElement);
  removeMessageListenerId("Marionette:clickElement", clickElement);
  removeMessageListenerId("Marionette:getElementAttribute", getElementAttribute);
  removeMessageListenerId("Marionette:getElementTagName", getElementTagName);
  removeMessageListenerId("Marionette:isElementDisplayed", isElementDisplayed);
  removeMessageListenerId("Marionette:getElementSize", getElementSize);
  removeMessageListenerId("Marionette:isElementEnabled", isElementEnabled);
  removeMessageListenerId("Marionette:isElementSelected", isElementSelected);
  removeMessageListenerId("Marionette:sendKeysToElement", sendKeysToElement);
  removeMessageListenerId("Marionette:getElementPosition", getElementPosition);
  removeMessageListenerId("Marionette:clearElement", clearElement);
  removeMessageListenerId("Marionette:switchToFrame", switchToFrame);
  removeMessageListenerId("Marionette:deleteSession", deleteSession);
  removeMessageListenerId("Marionette:sleepSession", sleepSession);
  removeMessageListenerId("Marionette:emulatorCmdResult", emulatorCmdResult);
  removeMessageListenerId("Marionette:importScript", importScript);
  removeMessageListenerId("Marionette:getAppCacheStatus", getAppCacheStatus);
  removeMessageListenerId("Marionette:setTestName", setTestName);
  removeMessageListenerId("Marionette:setState", setState);
  removeMessageListenerId("Marionette:screenShot", screenShot);
  removeMessageListenerId("Marionette:addCookie", addCookie);
  removeMessageListenerId("Marionette:getAllCookies", getAllCookies);
  removeMessageListenerId("Marionette:deleteAllCookies", deleteAllCookies);
  removeMessageListenerId("Marionette:deleteCookie", deleteCookie);
  this.elementManager.reset();
  // reset frame to the top-most frame
  curWindow = content;
  curWindow.focus();
  touches = [];
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
  curWindow = content;
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
                                  marionetteLogObj, marionettePerf,
                                  timeout, marionetteTestName);
  sandbox.marionette = marionette;
  marionette.exports.forEach(function(fn) {
    try {
      sandbox[fn] = marionette[fn].bind(marionette);
    }
    catch(e) {
      sandbox[fn] = marionette[fn];
    }
  });

  XPCOMUtils.defineLazyGetter(sandbox, 'SpecialPowers', function() {
    return new SpecialPowers(aWindow);
  });

  sandbox.asyncComplete = function sandbox_asyncComplete(value, status, stack, commandId) {
    if (commandId == asyncTestCommandId) {
      curWindow.removeEventListener("unload", onunload, false);
      curWindow.clearTimeout(asyncTestTimeoutId);

      sendSyncMessage("Marionette:shareData", {log: elementManager.wrapValue(marionetteLogObj.getLogs()),
                                               perf: elementManager.wrapValue(marionettePerf.getPerfData())});
      marionetteLogObj.clearLogs();
      marionettePerf.clearPerfData();

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
  asyncTestCommandId = msg.json.command_id;
  let script = msg.json.value;

  if (msg.json.newSandbox || !sandbox) {
    sandbox = createExecuteContentSandbox(curWindow,
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
        script = data + script;
      }
      let res = Cu.evalInSandbox(script, sandbox, "1.8");
      sendSyncMessage("Marionette:shareData", {log: elementManager.wrapValue(marionetteLogObj.getLogs()),
                                               perf: elementManager.wrapValue(marionettePerf.getPerfData())});
      marionetteLogObj.clearLogs();
      marionettePerf.clearPerfData();
      if (res == undefined || res.passed == undefined) {
        sendError("Marionette.finish() not called", 17, null, asyncTestCommandId);
      }
      else {
        sendResponse({value: elementManager.wrapValue(res)}, asyncTestCommandId);
      }
    }
    else {
      try {
        sandbox.__marionetteParams = elementManager.convertWrappedArguments(
          msg.json.args, curWindow);
      }
      catch(e) {
        sendError(e.message, e.code, e.stack, asyncTestCommandId);
        return;
      }

      let scriptSrc = "let __marionetteFunc = function(){" + script + "};" +
                      "__marionetteFunc.apply(null, __marionetteParams);";
      if (importedScripts.exists()) {
        let stream = Components.classes["@mozilla.org/network/file-input-stream;1"].  
                      createInstance(Components.interfaces.nsIFileInputStream);
        stream.init(importedScripts, -1, 0, 0);
        let data = NetUtil.readInputStreamToString(stream, stream.available());
        scriptSrc = data + scriptSrc;
      }
      let res = Cu.evalInSandbox(scriptSrc, sandbox, "1.8");
      sendSyncMessage("Marionette:shareData", {log: elementManager.wrapValue(marionetteLogObj.getLogs()),
                                               perf: elementManager.wrapValue(marionettePerf.getPerfData())});
      marionetteLogObj.clearLogs();
      marionettePerf.clearPerfData();
      sendResponse({value: elementManager.wrapValue(res)}, asyncTestCommandId);
    }
  }
  catch (e) {
    // 17 = JavascriptException
    sendError(e.name + ': ' + e.message, 17, e.stack, asyncTestCommandId);
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
  let script = msg.json.value;
  asyncTestCommandId = msg.json.command_id;

  onunload = function() {
    sendError("unload was called", 17, null, asyncTestCommandId);
  };
  curWindow.addEventListener("unload", onunload, false);

  if (msg.json.newSandbox || !sandbox) {
    sandbox = createExecuteContentSandbox(curWindow,
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
  asyncTestTimeoutId = curWindow.setTimeout(function() {
    sandbox.asyncComplete('timed out', 28, null, asyncTestCommandId);
  }, msg.json.timeout);

  originalOnError = curWindow.onerror;
  curWindow.onerror = function errHandler(errMsg, url, line) {
    sandbox.asyncComplete(errMsg, 17, "@" + url + ", line " + line, asyncTestCommandId);
    curWindow.onerror = originalOnError;
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
      sandbox.__marionetteParams = elementManager.convertWrappedArguments(
        msg.json.args, curWindow);
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
      scriptSrc = data + scriptSrc;
    }
    Cu.evalInSandbox(scriptSrc, sandbox, "1.8");
  } catch (e) {
    // 17 = JavascriptException
    sandbox.asyncComplete(e.name + ': ' + e.message, 17,
                          e.stack, asyncTestCommandId);
  }
}

/**
 * This function creates a touch event given a touch type and a touch
 */
function emitTouchEvent(type, touch) {
  // Using domWindowUtils
  let domWindowUtils = curWindow.QueryInterface(Components.interfaces.nsIInterfaceRequestor).getInterface(Components.interfaces.nsIDOMWindowUtils);
  domWindowUtils.sendTouchEvent(type, [touch.identifier], [touch.screenX], [touch.screenY], [touch.radiusX], [touch.radiusY], [touch.rotationAngle], [touch.force], 1, 0);
}

/**
 * This function creates a touch and emit touch events
 * @param 'xt' and 'yt' are two-element array [from, to] and then is a callback that will be invoked after touchend event is sent
 */
function touch(target, duration, xt, yt, then) {
  let doc = target.ownerDocument;
  let win = doc.defaultView;
  let touchId = nextTouchId++;
  let x = xt;
  if (typeof xt !== 'function') {
    x = function(t) { return xt[0] + t / duration * (xt[1] - xt[0]); };
  }
  let y = yt;
  if (typeof yt !== 'function') {
    y = function(t) { return yt[0] + t / duration * (yt[1] - yt[0]); };
  }
  // viewport coordinates
  let clientX = Math.round(x(0)), clientY = Math.round(y(0));
  // document coordinates
  let pageX = clientX + win.pageXOffset,
      pageY = clientY + win.pageYOffset;
  // screen coordinates
  let screenX = clientX + win.mozInnerScreenX,
      screenY = clientY + win.mozInnerScreenY;
  // Remember the coordinates
  let lastX = clientX, lastY = clientY;
  // Create the touch object
  let touch = doc.createTouch(win, target, touchId,
                              pageX, pageY,
                              screenX, screenY,
                              clientX, clientY);
  // Add this new touch to the list of touches
  touches.push(touch);
  // Send the start event
  emitTouchEvent('touchstart', touch);
  let startTime = Date.now();
  checkTimer.initWithCallback(nextEvent, EVENT_INTERVAL, Ci.nsITimer.TYPE_ONE_SHOT);
  function nextEvent() {
  // Figure out if this is the last of the touchmove events
    let time = Date.now();
    let dt = time - startTime;
    let last = dt + EVENT_INTERVAL / 2 > duration;
    // Find our touch object in the touches[] array.
    // Note that its index may have changed since we pushed it
    let touchIndex = touches.indexOf(touch);
    // If this is the last move event, make sure we move all the way
    if (last)
       dt = duration;
    // New coordinates of the touch
    clientX = Math.round(x(dt));
    clientY = Math.round(y(dt));
    // If we've moved, send a move event
    if (clientX !== lastX || clientY !== lastY) { // If we moved
      lastX = clientX;
      lastY = clientY;
      pageX = clientX + win.pageXOffset;
      pageY = clientY + win.pageYOffset;
      screenX = clientX + win.mozInnerScreenX;
      screenY = clientY + win.mozInnerScreenY;
      // Since we moved, we've got to create a new Touch object
      // with the new coordinates
      touch = doc.createTouch(win, target, touchId,
                              pageX, pageY,
                              screenX, screenY,
                              clientX, clientY);
      // Replace the old touch object with the new one
      touches[touchIndex] = touch;
      // And send the touchmove event
      emitTouchEvent('touchmove', touch);
    }
    // If that was the last move, send the touchend event
    // and call the callback 
    if (last) {
      touches.splice(touchIndex, 1);
      emitTouchEvent('touchend', touch);
      if (then)
        checkTimer.initWithCallback(then, 0, Ci.nsITimer.TYPE_ONE_SHOT);
    }
    // Otherwise, schedule the next event
    else {
      checkTimer.initWithCallback(nextEvent, EVENT_INTERVAL, Ci.nsITimer.TYPE_ONE_SHOT);
    }
  }
}

/**
 * This function generates the coordinates of the element
 * @param 'x0', 'y0', 'x1', and 'y1' are the relative to the viewport.
 *        If they are not specified, then the center of the target is used.
 */
function coordinates(target, x0, y0, x1, y1) {
  let coords = {};
  let box = target.getBoundingClientRect();
  let tx0 = typeof x0;
  let ty0 = typeof y0;
  let tx1 = typeof x1;
  let ty1 = typeof y1; 
  function percent(s, x) {
    s = s.trim();
    let f = parseFloat(s);
    if (s[s.length - 1] === '%')
      f = f * x / 100;
      return f;
  }
  function relative(s, x) {
    let factor;
    if (s[0] === '+')
      factor = 1;
    else
      factor = -1;
      return factor * percent(s.substring(1), x);
  }
  if (tx0 === 'number')
    coords.x0 = box.left + x0;
  else if (tx0 === 'string')
    coords.x0 = box.left + percent(x0, box.width);
  //check tx1 point
  if (tx1 === 'number')
    coords.x1 = box.left + x1;
  else if (tx1 === 'string') {
    x1 = x1.trim();
    if (x1[0] === '+' || x1[0] === '-')
      coords.x1 = coords.x0 + relative(x1, box.width);
    else
      coords.x1 = box.left + percent(x1, box.width);
  }
  // check ty0
  if (ty0 === 'number')
    coords.y0 = box.top + y0;
  else if (ty0 === 'string')
    coords.y0 = box.top + percent(y0, box.height);
  //check ty1
  if (ty1 === 'number')
    coords.y1 = box.top + y1;
  else if (ty1 === 'string') {
    y1 = y1.trim();
    if (y1[0] === '+' || y1[0] === '-')
      coords.y1 = coords.y0 + relative(y1, box.height);
    else
      coords.y1 = box.top + percent(y1, box.height);
  }
  return coords;
}

/**
 * This function returns if the element is in viewport 
 */
function elementInViewport(el) {
  let top = el.offsetTop;
  let left = el.offsetLeft;
  let width = el.offsetWidth;
  let height = el.offsetHeight;
  while(el.offsetParent) {
    el = el.offsetParent;
    top += el.offsetTop;
    left += el.offsetLeft;
  }
  return (top >= curWindow.pageYOffset &&
          left >= curWindow.pageXOffset &&
          (top + height) <= (curWindow.pageYOffset + curWindow.innerHeight) &&
          (left + width) <= (curWindow.pageXOffset + curWindow.innerWidth)
         );
}

/**
 * This function throws the visibility of the element error
 */
function checkVisible(el, command_id) {
  //check if the element is visible
  let visible = utils.isElementDisplayed(el);
  if (!visible) {
    return false;
  }
  if (el.tagName.toLowerCase() === 'body') {
    return true;
  }
  if (!elementInViewport(el)) {
    //check if scroll function exist. If so, call it.
    if (el.scrollIntoView) {
      el.scrollIntoView(true);
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
  let el;
  try {
    el = elementManager.getKnownElement(msg.json.value, curWindow);
    let x = msg.json.corx;
    let y = msg.json.cory;
    if (!checkVisible(el, command_id)) {
      sendError("Element is not currently visible and may not be manipulated", 11, null, command_id);
      return;
    }
    if (x == null) {
      x = '50%';
    }
    if (y == null) {
      y = '50%';
    }
    let c = coordinates(el, x, y);
    touch(el, 3000, [c.x0, c.x0], [c.y0, c.y0], null);
    sendOk(msg.json.command_id);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, msg.json.command_id);
  }
}

/**
 * Function that performs a double tap
 */
function doubleTap(msg) {
  let command_id = msg.json.command_id;
  let el;
  try {
    el = elementManager.getKnownElement(msg.json.value, curWindow);
    let x = msg.json.corx;
    let y = msg.json.cory;
    if (!checkVisible(el, command_id)) {
      sendError("Element is not currently visible and may not be manipulated", 11, null, command_id);
      return;
    }
    if (x == null){
      x = '50%';
    }
    if (y == null){
      y = '50%';
    }
    let c = coordinates(el, x, y);
    touch(el, 25, [c.x0, c.x0], [c.y0, c.y0], function() {
      // When the first tap is done, start a timer for interval ms
      checkTimer.initWithCallback(function() {
          //After interval ms, send the second tap
          touch(el, 25, [c.x0, c.x0], [c.y0, c.y0], null);
      }, 50, Ci.nsITimer.TYPE_ONE_SHOT);
    });
    sendOk(msg.json.command_id);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, msg.json.command_id);
  }
}

/**
 * Function to create a touch based on the element
 * corx and cory are related to the el, id is the touchId
 */
function createATouch(el, corx, cory, id) {
  let doc = el.ownerDocument;
  let win = doc.defaultView;
  if (corx == null) {
    corx = '50%';
  }
  if (cory == null){
    cory = '50%';
  }
  // corx and cory are relative to the el target. They must be within the same viewport
  // c are the coordinates relative to the current viewport
  let c = coordinates(el, corx, cory);
  let clientX = Math.round(c.x0),
      clientY = Math.round(c.y0);
  let pageX = clientX + win.pageXOffset,
      pageY = clientY + win.pageYOffset;
  let screenX = clientX + win.mozInnerScreenX,
      screenY = clientY + win.mozInnerScreenY;
  let atouch = doc.createTouch(win, el, id, pageX, pageY, screenX, screenY, clientX, clientY);
  return atouch;
}

/**
 * Function to start a touch event
 * corx and cory are relative to the element
 */
function press(msg) {
  let command_id = msg.json.command_id;
  let el;
  try {
    el = elementManager.getKnownElement(msg.json.value, curWindow);
    let corx = msg.json.corx;
    let cory = msg.json.cory;
    if (!checkVisible(el, command_id)) {
      sendError("Element is not currently visible and may not be manipulated", 11, null, command_id);
      return;
    }
    let touchId = nextTouchId++;
    let touch = createATouch(el, corx, cory, touchId);
    emitTouchEvent('touchstart', touch);
    touchIds[touchId] = touch;
    sendResponse({value: touch.identifier}, command_id);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, msg.json.command_id);
  }
}

/**
 * Function to end a touch event
 */
function release(msg) {
  let command_id = msg.json.command_id;
  let el;
  try {
    let id = msg.json.touchId;
    if (id in touchIds) {
      let startTouch = touchIds[id];
      el = startTouch.target;
      let corx = msg.json.corx;
      let cory = msg.json.cory;
      if (!checkVisible(el, command_id)) {
        sendError("Element is not currently visible and may not be manipulated", 11, null, command_id);
        return;
      }
      let touch = createATouch(el, corx, cory, id);
      if (touch.clientX != startTouch.clientX ||
          touch.clientY != startTouch.clientY) {
        emitTouchEvent('touchmove', touch);
      }
      emitTouchEvent('touchend', touch);
      delete touchIds[id];
      sendOk(msg.json.command_id);
    }
    else {
      sendError("Element has not been pressed: no such element", 7, null, command_id);
    }
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, msg.json.command_id);
  }
}

/**
 * Function to cancel a touch event
 */
function cancelTouch(msg) {
  let command_id = msg.json.command_id;
  try {
    let id = msg.json.touchId;
    if (id in touchIds) {
      let startTouch = touchIds[id];
      emitTouchEvent('touchcancel', startTouch);
      delete touchIds[id];
      sendOk(msg.json.command_id);
    }
    else {
      sendError("Element not previously interacted with", 7, null, command_id);
    }
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, msg.json.command_id);
  }
}

/**
 * Function to emit touch events for each finger. e.g. finger=[['press', id], ['wait', 5], ['release']]
 * touchId represents the finger id, i keeps track of the current action of the finger
 */
function actions(finger, touchId, command_id, i){
  if (typeof i === "undefined") {
    i = 0;
  }
  if (i == finger.length) {
    sendResponse({value: touchId}, command_id);
    return;
  }
  let pack = finger[i];
  let command = pack[0];
  // el has the id
  let el;
  let corx;
  let cory;
  let touch;
  i++;
  switch(command) {
    case 'press':
      el = elementManager.getKnownElement(pack[1], curWindow);
      corx = pack[2];
      cory = pack[3];
      // after this block, the element will be scrolled into view
      if (!checkVisible(el, command_id)) {
         sendError("Element is not currently visible and may not be manipulated", 11, null, command_id);
         return;
      }
      touch = createATouch(el, corx, cory, touchId);
      lastTouch = touch;
      emitTouchEvent('touchstart', touch);
      // check if it's a long press
      // standard waiting time to fire contextmenu
      let standard = Services.prefs.getIntPref("ui.click_hold_context_menus.delay");
      // long press only happens when wait follows press
      if (finger[i] != undefined && finger[i][0] == 'wait' && finger[i][1] != null && finger[i][1]*1000 >= standard) {
        finger[i][1] = finger[i][1] - standard/1000;
        finger.splice(i, 0, ['wait', standard/1000], ['longPress']);
      }
      actions(finger,touchId, command_id, i);
      break;
    case 'release':
      if (lastTouch == null) {
        sendError("Element has not been pressed: no such element", 7, null, command_id);
        return;
      }
      touch = lastTouch;
      lastTouch = null;
      emitTouchEvent('touchend', touch);
      actions(finger, touchId, command_id, i);
      break;
    case 'move':
      if (lastTouch == null) {
        sendError("Element has not been pressed: no such element", 7, null, command_id);
        return;
      }
      el = elementManager.getKnownElement(pack[1], curWindow);
      let boxTarget = el.getBoundingClientRect();
      let startElement = lastTouch.target;
      let boxStart = startElement.getBoundingClientRect();
      corx = boxTarget.left - boxStart.left + boxTarget.width * 0.5;
      cory = boxTarget.top - boxStart.top + boxTarget.height * 0.5;
      touch = createATouch(startElement, corx, cory, touchId);
      lastTouch = touch;
      emitTouchEvent('touchmove', touch);
      actions(finger, touchId, command_id, i);
      break;
    case 'moveByOffset':
      if (lastTouch == null) {
        sendError("Element has not been pressed: no such element", 7, null, command_id);
        return;
      }
      el = lastTouch.target;
      let doc = el.ownerDocument;
      let win = doc.defaultView;
      let clientX = lastTouch.clientX + pack[1],
          clientY = lastTouch.clientY + pack[2];
      let pageX = clientX + win.pageXOffset,
          pageY = clientY + win.pageYOffset;
      let screenX = clientX + win.mozInnerScreenX,
          screenY = clientY + win.mozInnerScreenY;
      touch = doc.createTouch(win, el, touchId, pageX, pageY, screenX, screenY, clientX, clientY);
      lastTouch = touch;
      emitTouchEvent('touchmove', touch);
      actions(finger, touchId, command_id, i);
      break;
    case 'wait':
      if (pack[1] != null ) {
        let time = pack[1]*1000;
        checkTimer.initWithCallback(function(){actions(finger, touchId, command_id, i);}, time, Ci.nsITimer.TYPE_ONE_SHOT);
      }
      else {
        actions(finger, touchId, command_id, i);
      }
      break;
    case 'cancel':
      touch = lastTouch;
      emitTouchEvent('touchcancel', touch);
      lastTouch = null;
      actions(finger, touchId, command_id, i);
      break;
    case 'longPress':
      let event = curWindow.document.createEvent('HTMLEvents');
      event.initEvent('contextmenu',
                      true,
                      true);
      lastTouch.target.dispatchEvent(event);
      actions(finger, touchId, command_id, i);
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
    let commandArray = elementManager.convertWrappedArguments(args, curWindow);
    // loop the action array [ ['press', id], ['move', id], ['release', id] ]
    if (touchId == null) {
      touchId = nextTouchId++;
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
    return t.target.ownerDocument === doc;
  }));
  // touches on the same target
  let targetTouches = doc.createTouchList(touches.filter(function(t) {
    return t.target === target;
  }));
  // Create changed touches
  let changedTouches = doc.createTouchList(touch);
  // Create the event object
  let event = curWindow.document.createEvent('TouchEvent');
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
  batchIndex++;
  // loop through the batch
  for (let i = 0; i < batch.length; i++) {
    pack = batch[i];
    touchId = pack[0];
    command = pack[1];
    switch (command) {
      case 'press':
        el = elementManager.getKnownElement(pack[2], curWindow);
        // after this block, the element will be scrolled into view
        if (!checkVisible(el, command_id)) {
           sendError("Element is not currently visible and may not be manipulated", 11, null, command_id);
           return;
        }
        corx = pack[3];
        cory = pack[4];
        touch = createATouch(el, corx, cory, touchId);
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
        el = elementManager.getKnownElement(pack[2], curWindow);
        lastTouch = multiLast[touchId];
        let boxTarget = el.getBoundingClientRect();
        let startTarget = lastTouch.target;
        let boxStart = startTarget.getBoundingClientRect();
        // note here corx and cory are relative to the target, not the viewport
        // we always want to touch the center of the element if the element is specified
        corx = boxTarget.left - boxStart.left + boxTarget.width * 0.5;
        cory = boxTarget.top - boxStart.top + boxTarget.height * 0.5;
        touch = createATouch(startTarget, corx, cory, touchId);
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
    let commandArray = elementManager.convertWrappedArguments(args, curWindow);
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
 * Function to set the timeout period for element searching 
 */
function setSearchTimeout(msg) {
  try {
    elementManager.setSearchTimeout(msg.json.value);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, msg.json.command_id);
    return;
  }
  sendOk(msg.json.command_id);
}

/**
 * Navigate to URI. Handles the case where we navigate within an iframe.
 * All other navigation is handled by the server (in chrome space).
 */
function goUrl(msg) {
  let command_id = msg.json.command_id;
  // Prevent DOMContentLoaded events from frames from invoking this code,
  // unless the event is coming from the frame associated with the current
  // window (i.e., someone has used switch_to_frame).
  let onDOMContentLoaded = function onDOMContentLoaded(event){
    if (msg.json.pageTimeout != null){
      checkTimer.cancel();
    }
    if (!event.originalTarget.defaultView.frameElement ||
      event.originalTarget.defaultView.frameElement == curWindow.frameElement) {
      removeEventListener("DOMContentLoaded", onDOMContentLoaded, false);
      let errorRegex = /about:.+(error)|(blocked)\?/;
      if (curWindow.document.readyState == "interactive" && errorRegex.exec(curWindow.document.baseURI)) {
        sendError("Error loading page", 13, null, command_id);
        return;
      }
      sendOk(command_id);
    }
  };
  function timerFunc(){
    sendError("Error loading page", 13, null, command_id);
    removeEventListener("DOMContentLoaded", onDOMContentLoaded, false);
  }
  if (msg.json.pageTimeout != null){
    checkTimer.initWithCallback(timerFunc, msg.json.pageTimeout, Ci.nsITimer.TYPE_ONE_SHOT);
  }
  addEventListener("DOMContentLoaded", onDOMContentLoaded, false);
  curWindow.location = msg.json.value;
}

/**
 * Get the current URI
 */
function getUrl(msg) {
  sendResponse({value: curWindow.location.href}, msg.json.command_id);
}

/**
 * Get the current Title of the window
 */
function getTitle(msg) {
  sendResponse({value: curWindow.top.document.title}, msg.json.command_id);
}

/**
 * Get the current page source 
 */
function getPageSource(msg) {
  var XMLSerializer = curWindow.XMLSerializer;
  var pageSource = new XMLSerializer().serializeToString(curWindow.document);
  sendResponse({value: pageSource}, msg.json.command_id);
}

/**
 * Go back in history 
 */
function goBack(msg) {
  curWindow.history.back();
  sendOk(msg.json.command_id);
}

/**
 * Go forward in history 
 */
function goForward(msg) {
  curWindow.history.forward();
  sendOk(msg.json.command_id);
}

/**
 * Refresh the page
 */
function refresh(msg) {
  let command_id = msg.json.command_id;
  curWindow.location.reload(true);
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
    elementManager.find(curWindow, msg.json, on_success, on_error, false, command_id);
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
    elementManager.find(curWindow, msg.json, on_success, on_error, true, command_id);
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
  var element = curWindow.document.activeElement;
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
    el = elementManager.getKnownElement(msg.json.element, curWindow);
    if (checkVisible(el, command_id)) {
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
    let el = elementManager.getKnownElement(msg.json.element, curWindow);
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
    let el = elementManager.getKnownElement(msg.json.element, curWindow);
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
    let el = elementManager.getKnownElement(msg.json.element, curWindow);
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
    let el = elementManager.getKnownElement(msg.json.element, curWindow);
    sendResponse({value: utils.isElementDisplayed(el)}, command_id);
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
    let el = elementManager.getKnownElement(msg.json.element, curWindow);
    let clientRect = el.getBoundingClientRect();  
    sendResponse({value: {width: clientRect.width, height: clientRect.height}},
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
    let el = elementManager.getKnownElement(msg.json.element, curWindow);
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
    let el = elementManager.getKnownElement(msg.json.element, curWindow);
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
  try {
    let el = elementManager.getKnownElement(msg.json.element, curWindow);
    if (checkVisible(el, command_id)) {
      utils.type(curWindow.document, el, msg.json.value.join(""), true);
      sendOk(command_id);
    }
    else {
      sendError("Element is not visible", 11, null, command_id)
    }
  }
  catch (e) {
    sendError(e.message, e.code, e.stack, command_id);
  }
}

/**
 * Get the position of an element
 */
function getElementPosition(msg) {
  let command_id = msg.json.command_id;
  try{
    let el = elementManager.getKnownElement(msg.json.element, curWindow);
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
    let el = elementManager.getKnownElement(msg.json.element, curWindow);
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
    if (curWindow.document.readyState == "complete") {
      sendOk(command_id);
      return;
    } 
    else if (curWindow.document.readyState == "interactive" && errorRegex.exec(curWindow.document.baseURI)) {
      sendError("Error loading page", 13, null, command_id);
      return;
    }
    checkTimer.initWithCallback(checkLoad, 100, Ci.nsITimer.TYPE_ONE_SHOT);
  }
  let foundFrame = null;
  let frames = []; //curWindow.document.getElementsByTagName("iframe");
  let parWindow = null; //curWindow.QueryInterface(Ci.nsIInterfaceRequestor)
  // Check of the curWindow reference is dead
  try {
    frames = curWindow.document.getElementsByTagName("iframe");
    //Until Bug 761935 lands, we won't have multiple nested OOP iframes. We will only have one.
    //parWindow will refer to the iframe above the nested OOP frame.
    parWindow = curWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
  } catch (e) {
    // We probably have a dead compartment so accessing it is going to make Firefox
    // very upset. Let's now try redirect everything to the top frame even if the 
    // user has given us a frame since search doesnt look up.
    msg.json.value = null;
    msg.json.element = null;
  }
  if ((msg.json.value == null) && (msg.json.element == null)) {
    curWindow = content;
    if(msg.json.focus == true) {
      curWindow.focus();
    }
    sandbox = null;
    checkTimer.initWithCallback(checkLoad, 100, Ci.nsITimer.TYPE_ONE_SHOT);
    return;
  }
  if (msg.json.element != undefined) {
    if (elementManager.seenItems[msg.json.element] != undefined) {
      let wantedFrame;
      try {
        wantedFrame = elementManager.getKnownElement(msg.json.element, curWindow); //HTMLIFrameElement
      }
      catch(e) {
        sendError(e.message, e.code, e.stack, command_id);
      }
      for (let i = 0; i < frames.length; i++) {
        // use XPCNativeWrapper to compare elements; see bug 834266
        if (XPCNativeWrapper(frames[i]) == XPCNativeWrapper(wantedFrame)) {
          curWindow = frames[i]; 
          foundFrame = i;
        }
      }
    }
  }
  if (foundFrame == null) {
    switch(typeof(msg.json.value)) {
      case "string" :
        let foundById = null;
        for (let i = 0; i < frames.length; i++) {
          //give precedence to name
          let frame = frames[i];
          let name = utils.getElementAttribute(frame, 'name');
          let id = utils.getElementAttribute(frame, 'id');
          if (name == msg.json.value) {
            foundFrame = i;
            break;
          } else if ((foundById == null) && (id == msg.json.value)) {
            foundById = i;
          }
        }
        if ((foundFrame == null) && (foundById != null)) {
          foundFrame = foundById;
          curWindow = frames[foundFrame];
        }
        break;
      case "number":
        if (frames[msg.json.value] != undefined) {
          foundFrame = msg.json.value;
          curWindow = frames[foundFrame];
        }
        break;
    }
  }
  if (foundFrame == null) {
    sendError("Unable to locate frame: " + msg.json.value, 8, null, command_id);
    return;
  }

  sandbox = null;

  if (curWindow.contentWindow == null) {
    // The frame we want to switch to is a remote frame; notify our parent to handle
    // the switch.
    curWindow = content;
    sendToServer('Marionette:switchToFrame', {frame: foundFrame,
                                              win: parWindow,
                                              command_id: command_id});
  }
  else {
    curWindow = curWindow.contentWindow;
    if(msg.json.focus == true) {
      curWindow.focus();
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
    var location = curWindow.document.location;
    cookie.domain = location.hostname;
  }
  else {
    var currLocation = curWindow.location;
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

  var document = curWindow.document;
  if (!document || !document.contentType.match(/html/i)) {
    sendError('You may only set cookies on html documents', 25, null, msg.json.command_id);
  }
  var cookieManager = Cc['@mozilla.org/cookiemanager;1'].
                        getService(Ci.nsICookieManager2);
  cookieManager.add(cookie.domain, cookie.path, cookie.name, cookie.value,
                   cookie.secure, false, false, cookie.expiry);
  sendOk(msg.json.command_id);
}

/**
 * Get All the cookies for a location
 */
function getAllCookies(msg) {
  var toReturn = [];
  var cookies = getVisibleCookies(curWindow.location);
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
  var cookieManager = Cc['@mozilla.org/cookiemanager;1'].
                        getService(Ci.nsICookieManager);

  var cookies = getVisibleCookies(curWindow.location);
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
  let cookieManager = Cc['@mozilla.org/cookiemanager;1'].
                        getService(Ci.nsICookieManager);
  let cookies = getVisibleCookies(curWindow.location);
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

  let cookieManager = Cc['@mozilla.org/cookiemanager;1'].
                        getService(Ci.nsICookieManager);
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
  sendResponse({ value: curWindow.applicationCache.status },
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
 * Saves a screenshot and returns a Base64 string
 */
function screenShot(msg) {
  let node = null;
  if (msg.json.element) {
    try {
      node = elementManager.getKnownElement(msg.json.element, curWindow)
    }
    catch (e) {
      sendResponse(e.message, e.code, e.stack, msg.json.command_id);
      return;
    }
  }
  else {
      node = curWindow;
  }
  let highlights = msg.json.highlights;

  var document = curWindow.document;
  var rect, win, width, height, left, top, needsOffset;
  // node can be either a window or an arbitrary DOM node
  if (node == curWindow) {
    // node is a window
    win = node;
    width = win.innerWidth;
    height = win.innerHeight;
    top = 0;
    left = 0;
    // offset needed for highlights to take 'outerHeight' of window into account
    needsOffset = true;
  }
  else {
    // node is an arbitrary DOM node
    win = node.ownerDocument.defaultView;
    rect = node.getBoundingClientRect();
    width = rect.width;
    height = rect.height;
    top = rect.top;
    left = rect.left;
    // offset for highlights not needed as they will be relative to this node
    needsOffset = false;
  }

  var canvas = document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
  canvas.width = width;
  canvas.height = height;
  var ctx = canvas.getContext("2d");
  // Draws the DOM contents of the window to the canvas
  ctx.drawWindow(win, left, top, width, height, 'rgb(255,255,255)');

  // This section is for drawing a red rectangle around each element passed in via the highlights array
  if (highlights) {
    ctx.lineWidth = "2";
    ctx.strokeStyle = "red";
    ctx.save();

    for (var i = 0; i < highlights.length; ++i) {
      var elem = highlights[i];
      rect = elem.getBoundingClientRect();

      var offsetY = 0, offsetX = 0;
      if (needsOffset) {
        var offset = getChromeOffset(elem);
        offsetX = offset.x;
        offsetY = offset.y;
      } else {
        // Don't need to offset the window chrome, just make relative to containing node
        offsetY = -top;
        offsetX = -left;
      }

      // Draw the rectangle
      ctx.strokeRect(rect.left + offsetX, rect.top + offsetY, rect.width, rect.height);
    }
  }

  // Return the Base64 String back to the client bindings and they can manage
  // saving the file to disk if it is required
  sendResponse({value:canvas.toDataURL("image/png","")}, msg.json.command_id);
}

//call register self when we get loaded
registerSelf();

