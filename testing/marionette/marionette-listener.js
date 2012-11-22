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

let marionetteTimeout = null;
let marionetteTestName;
let winUtil = content.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDOMWindowUtils);
let listenerId = null; //unique ID of this listener
let activeFrame = null;
let curWindow = content;
let elementManager = new ElementManager([]);
let importedScripts = FileUtils.getFile('TmpD', ['marionettescript']);

// The sandbox we execute test scripts in. Gets lazily created in
// createExecuteContentSandbox().
let sandbox;

// Flag to indicate whether an async script is currently running or not.
let asyncTestRunning = false;
let asyncTestCommandId;
let asyncTestTimeoutId;
//timer for doc changes
let checkTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

/**
 * Called when listener is first started up. 
 * The listener sends its unique window ID and its current URI to the actor.
 * If the actor returns an ID, we start the listeners. Otherwise, nothing happens.
 */
function registerSelf() {
  Services.io.manageOfflineStatus = false;
  Services.io.offline = false;
  let register = sendSyncMessage("Marionette:register", {value: winUtil.outerWindowID, href: content.location.href});
  
  if (register[0]) {
    listenerId = register[0];
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
  addMessageListenerId("Marionette:setScriptTimeout", setScriptTimeout);
  addMessageListenerId("Marionette:executeAsyncScript", executeAsyncScript);
  addMessageListenerId("Marionette:executeJSScript", executeJSScript);
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
    sendError(e.message, e.code, e.stack);
    return;
  }
  sendOk();
}

/**
 * Restarts all our listeners after this listener was put to sleep
 */
function restart() {
  removeMessageListener("Marionette:restart", restart);
  registerSelf();
}

/**
 * Removes all listeners
 */
function deleteSession(msg) {
  removeMessageListenerId("Marionette:newSession", newSession);
  removeMessageListenerId("Marionette:executeScript", executeScript);
  removeMessageListenerId("Marionette:setScriptTimeout", setScriptTimeout);
  removeMessageListenerId("Marionette:executeAsyncScript", executeAsyncScript);
  removeMessageListenerId("Marionette:executeJSScript", executeJSScript);
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
  this.elementManager.reset();
  // reset frame to the top-most frame
  curWindow = content;
  curWindow.focus();
  try {
    importedScripts.remove(false);
  }
  catch (e) {
  }
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
  marionetteTimeout = null;
  curWindow = content;
}

/**
 * send error when we detect an unload event during async scripts
 */
function errUnload() {
  sendError("unload was called", 17, null);
}

/*
 * Marionette Methods
 */

/**
 * Returns a content sandbox that can be used by the execute_foo functions.
 */
function createExecuteContentSandbox(aWindow) {
  let sandbox = new Cu.Sandbox(aWindow);
  sandbox.global = sandbox;
  sandbox.window = aWindow;
  sandbox.document = sandbox.window.document;
  sandbox.navigator = sandbox.window.navigator;
  sandbox.__proto__ = sandbox.window;
  sandbox.testUtils = utils;

  let marionette = new Marionette(this, aWindow, "content",
                                  marionetteLogObj, marionettePerf,
                                  marionetteTimeout, marionetteTestName);
  sandbox.marionette = marionette;
  marionette.exports.forEach(function(fn) {
    try {
      sandbox[fn] = marionette[fn].bind(marionette);
    }
    catch(e) {
      sandbox[fn] = marionette[fn];
    }
  });

  sandbox.SpecialPowers = new SpecialPowers(aWindow);

  sandbox.asyncComplete = function sandbox_asyncComplete(value, status) {
    if (Object.keys(_emu_cbs).length) {
      _emu_cbs = {};
      value = "Emulator callback still pending when finish() called";
      status = 500;
    }

    curWindow.removeEventListener("unload", errUnload, false);

    /* clear all timeouts potentially generated by the script*/
    for (let i = 0; i <= asyncTestTimeoutId; i++) {
      curWindow.clearTimeout(i);
    }

    sendSyncMessage("Marionette:shareData", {log: elementManager.wrapValue(marionetteLogObj.getLogs()),
                                             perf: elementManager.wrapValue(marionettePerf.getPerfData())});
    marionetteLogObj.clearLogs();
    marionettePerf.clearPerfData();
    if (status == 0){
      sendResponse({value: elementManager.wrapValue(value), status: status}, asyncTestCommandId);
    }
    else {
      sendError(value, status, null, asyncTestCommandId);
    }

    asyncTestRunning = false;
    asyncTestTimeoutId = undefined;
    asyncTestCommandId = undefined;
  };
  sandbox.finish = function sandbox_finish() {
    if (asyncTestRunning) {
      sandbox.asyncComplete(marionette.generate_results(), 0);
    } else {
      return marionette.generate_results();
    }
  };
  sandbox.marionetteScriptFinished = function sandbox_marionetteScriptFinished(value) {
    return sandbox.asyncComplete(value, 0);
  };

  return sandbox;
}

/**
 * Execute the given script either as a function body (executeScript)
 * or directly (for 'mochitest' like JS Marionette tests)
 */
function executeScript(msg, directInject) {
  let script = msg.json.value;

  if (msg.json.newSandbox || !sandbox) {
    sandbox = createExecuteContentSandbox(curWindow);
    if (!sandbox) {
      sendError("Could not create sandbox!");
      return;
    }
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
        sendError("Marionette.finish() not called", 17, null);
      }
      else {
        sendResponse({value: elementManager.wrapValue(res)});
      }
    }
    else {
      try {
        sandbox.__marionetteParams = elementManager.convertWrappedArguments(
          msg.json.args, curWindow);
      }
      catch(e) {
        sendError(e.message, e.code, e.stack);
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
      sendResponse({value: elementManager.wrapValue(res)});
    }
  }
  catch (e) {
    // 17 = JavascriptException
    sendError(e.name + ': ' + e.message, 17, e.stack);
  }
}

/**
 * Sets the test name, used in logging messages.
 */
function setTestName(msg) {
  marionetteTestName = msg.json.value;
  sendOk();
}

/**
 * Function to set the timeout of asynchronous scripts
 */
function setScriptTimeout(msg) {
  marionetteTimeout = msg.json.value;
  sendOk();
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
  if (msg.json.timeout) {
    executeWithCallback(msg, msg.json.timeout);
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
function executeWithCallback(msg, timeout) {
  curWindow.addEventListener("unload", errUnload, false);
  let script = msg.json.value;
  asyncTestCommandId = msg.json.id;

  if (msg.json.newSandbox || !sandbox) {
    sandbox = createExecuteContentSandbox(curWindow);
    if (!sandbox) {
      sendError("Could not create sandbox!");
      return;
    }
  }

  // Error code 28 is scriptTimeout, but spec says execute_async should return 21 (Timeout),
  // see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/execute_async.
  // However Selenium code returns 28, see
  // http://code.google.com/p/selenium/source/browse/trunk/javascript/firefox-driver/js/evaluate.js.
  // We'll stay compatible with the Selenium code.
  asyncTestTimeoutId = curWindow.setTimeout(function() {
    sandbox.asyncComplete('timed out', 28);
  }, marionetteTimeout);

  curWindow.addEventListener('error', function win__onerror(evt) {
    curWindow.removeEventListener('error', win__onerror, true);
    sandbox.asyncComplete(evt, 17);
    return true;
  }, true);

  let scriptSrc;
  if (timeout) {
    if (marionetteTimeout == null || marionetteTimeout == 0) {
      sendError("Please set a timeout", 21, null);
    }
    scriptSrc = script;
  }
  else {
    try {
      sandbox.__marionetteParams = elementManager.convertWrappedArguments(
        msg.json.args, curWindow);
    }
    catch(e) {
      sendError(e.message, e.code, e.stack);
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
    sandbox.asyncComplete(e.name + ': ' + e.message, 17);
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
    sendError(e.message, e.code, e.stack);
    return;
  }
  sendOk();
}

/**
 * Navigate to URI. Handles the case where we navigate within an iframe.
 * All other navigation is handled by the server (in chrome space).
 */
function goUrl(msg) {
  addEventListener("DOMContentLoaded", function onDOMContentLoaded(event) {
    // Prevent DOMContentLoaded events from frames from invoking this code,
    // unless the event is coming from the frame associated with the current
    // window (i.e., someone has used switch_to_frame).
    if (!event.originalTarget.defaultView.frameElement || 
        event.originalTarget.defaultView.frameElement == curWindow.frameElement) {
      removeEventListener("DOMContentLoaded", onDOMContentLoaded, false);

      let errorRegex = /about:.+(error)|(blocked)\?/;
      if (curWindow.document.readyState == "interactive" && errorRegex.exec(curWindow.document.baseURI)) {
        sendError("Error loading page", 13, null);
        return;
      }

      sendOk();
    }
  }, false);
  curWindow.location = msg.json.value;
}

/**
 * Get the current URI
 */
function getUrl(msg) {
  sendResponse({value: curWindow.location.href});
}

/**
 * Get the current Title of the window
 */
function getTitle(msg) {
  sendResponse({value: curWindow.top.document.title});
}

/**
 * Get the current page source 
 */
function getPageSource(msg) {
  var XMLSerializer = curWindow.XMLSerializer;
  var pageSource = new XMLSerializer().serializeToString(curWindow.document);
  sendResponse({value: pageSource });
}

/**
 * Go back in history 
 */
function goBack(msg) {
  curWindow.history.back();
  sendOk();
}

/**
 * Go forward in history 
 */
function goForward(msg) {
  curWindow.history.forward();
  sendOk();
}

/**
 * Refresh the page
 */
function refresh(msg) {
  curWindow.location.reload(true);
  let listen = function() { removeEventListener("DOMContentLoaded", arguments.callee, false); sendOk() } ;
  addEventListener("DOMContentLoaded", listen, false);
}

/**
 * Find an element in the document using requested search strategy 
 */
function findElementContent(msg) {
  try {
    let on_success = function(id) { sendResponse({value:id}); };
    let on_error = sendError;
    elementManager.find(curWindow, msg.json, on_success, on_error, false);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack);
  }
}

/**
 * Find elements in the document using requested search strategy 
 */
function findElementsContent(msg) {
  try {
    let on_success = function(id) { sendResponse({value:id}); };
    let on_error = sendError;
    elementManager.find(curWindow, msg.json, on_success, on_error, true);
  }
  catch (e) {
    sendError(e.message, e.code, e.stack);
  }
}

/**
 * Send click event to element
 */
function clickElement(msg) {
  let el;
  try {
    el = elementManager.getKnownElement(msg.json.element, curWindow);
    utils.click(el);
    sendOk();
  }
  catch (e) {
    sendError(e.message, e.code, e.stack);
  }
}

/**
 * Get a given attribute of an element
 */
function getElementAttribute(msg) {
  try {
    let el = elementManager.getKnownElement(msg.json.element, curWindow);
    sendResponse({value: utils.getElementAttribute(el, msg.json.name)});
  }
  catch (e) {
    sendError(e.message, e.code, e.stack);
  }
}

/**
 * Get the text of this element. This includes text from child elements.
 */
function getElementText(msg) {
  try {
    let el = elementManager.getKnownElement(msg.json.element, curWindow);
    sendResponse({value: utils.getElementText(el)});
  }
  catch (e) {
    sendError(e.message, e.code, e.stack);
  }
}

/**
 * Get the tag name of an element.
 */
function getElementTagName(msg) {
  try {
    let el = elementManager.getKnownElement(msg.json.element, curWindow);
    sendResponse({value: el.tagName.toLowerCase()});
  }
  catch (e) {
    sendError(e.message, e.code, e.stack);
  }
}

/**
 * Check if element is displayed
 */
function isElementDisplayed(msg) {
  try {
    let el = elementManager.getKnownElement(msg.json.element, curWindow);
    sendResponse({value: utils.isElementDisplayed(el)});
  }
  catch (e) {
    sendError(e.message, e.code, e.stack);
  }
}

/**
 * Get the size of the element and return it
 */
function getElementSize(msg){
  try {
    let el = elementManager.getKnownElement(msg.json.element, curWindow);
    let clientRect = el.getBoundingClientRect();  
    sendResponse({value: {width: clientRect.width, height: clientRect.height}});
  }
  catch (e) {
    sendError(e.message, e.code, e.stack);
  }
}

/**
 * Check if element is enabled
 */
function isElementEnabled(msg) {
  try {
    let el = elementManager.getKnownElement(msg.json.element, curWindow);
    sendResponse({value: utils.isElementEnabled(el)});
  }
  catch (e) {
    sendError(e.message, e.code, e.stack);
  }
}

/**
 * Check if element is selected
 */
function isElementSelected(msg) {
  try {
    let el = elementManager.getKnownElement(msg.json.element, curWindow);
    sendResponse({value: utils.isElementSelected(el)});
  }
  catch (e) {
    sendError(e.message, e.code, e.stack);
  }
}

/**
 * Send keys to element
 */
function sendKeysToElement(msg) {
  try {
    let el = elementManager.getKnownElement(msg.json.element, curWindow);
    utils.type(curWindow.document, el, msg.json.value.join(""), true);
    sendOk();
  }
  catch (e) {
    sendError(e.message, e.code, e.stack);
  }
}

/**
 * Get the position of an element
 */
function getElementPosition(msg) {
  try{
    let el = elementManager.getKnownElement(msg.json.element, curWindow);
    var x = el.offsetLeft;
    var y = el.offsetTop;
    var elementParent = el.offsetParent;
    while (elementParent != null) {
      if (elementParent.tagName == "TABLE") {
        var parentBorder = parseInt(elementParent.border);
        if (isNaN(parentBorder)) {
          var parentFrame = elementParent.getAttribute('frame');
          if (parentFrame != null) {
            x += 1;
            y += 1;
          }
        } else if (parentBorder > 0) {
          x += parentBorder;
          y += parentBorder;
        }
      }
      x += elementParent.offsetLeft;
      y += elementParent.offsetTop;
      elementParent = elementParent.offsetParent;
    }

    let location = {};
    location.x = x;
    location.y = y;

    sendResponse({value: location});
  }
  catch (e) {
    sendError(e.message, e.code, e.stack);
  }
}

/**
 * Clear the text of an element
 */
function clearElement(msg) {
  try {
    let el = elementManager.getKnownElement(msg.json.element, curWindow);
    utils.clearElement(el);
    sendOk();
  }
  catch (e) {
    sendError(e.message, e.code, e.stack);
  }
}

/**
 * Switch to frame given either the server-assigned element id,
 * its index in window.frames, or the iframe's name or id.
 */
function switchToFrame(msg) {
  function checkLoad() { 
    let errorRegex = /about:.+(error)|(blocked)\?/;
    if (curWindow.document.readyState == "complete") {
      sendOk();
      return;
    } 
    else if (curWindow.document.readyState == "interactive" && errorRegex.exec(curWindow.document.baseURI)) {
      sendError("Error loading page", 13, null);
      return;
    }
    checkTimer.initWithCallback(checkLoad, 100, Ci.nsITimer.TYPE_ONE_SHOT);
  }
  let foundFrame = null;
  let frames = curWindow.document.getElementsByTagName("iframe");
  //Until Bug 761935 lands, we won't have multiple nested OOP iframes. We will only have one.
  //parWindow will refer to the iframe above the nested OOP frame.
  let parWindow = curWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
  if ((msg.json.value == null) && (msg.json.element == null)) {
    curWindow = content;
    curWindow.focus();
    checkTimer.initWithCallback(checkLoad, 100, Ci.nsITimer.TYPE_ONE_SHOT);
    return;
  }
  if (msg.json.element != undefined) {
    if (elementManager.seenItems[msg.json.element] != undefined) {
      let wantedFrame = elementManager.getKnownElement(msg.json.element, curWindow); //HTMLIFrameElement
      for (let i = 0; i < frames.length; i++) {
        if (frames[i] == wantedFrame) {
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
    sendError("Unable to locate frame: " + msg.json.value, 8, null);
    return;
  }

  sandbox = null;

  if (curWindow.contentWindow == null) {
    // The frame we want to switch to is a remote frame; notify our parent to handle
    // the switch.
    curWindow = content;
    sendToServer('Marionette:switchToFrame', {frame: foundFrame, win: parWindow});
  }
  else {
    curWindow = curWindow.contentWindow;
    curWindow.focus();
    checkTimer.initWithCallback(checkLoad, 100, Ci.nsITimer.TYPE_ONE_SHOT);
  }
}

function getAppCacheStatus() {
  sendResponse({ value: curWindow.applicationCache.status });  
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
    sendError(e.message, e.code, e.stack);
    return;
  }
}

function importScript(msg) {
  let file;
  if (importedScripts.exists()) {
    file = FileUtils.openFileOutputStream(importedScripts, FileUtils.MODE_APPEND | FileUtils.MODE_WRONLY);
  }
  else {
    //Note: The permission bits here don't actually get set (bug 804563)
    importedScripts.createUnique(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, parseInt("0666", 8));
    file = FileUtils.openFileOutputStream(importedScripts, FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE);
    importedScripts.permissions = parseInt("0666", 8); //actually set permissions
  }
  file.write(msg.json.script, msg.json.script.length);
  file.close();
  sendOk();
}

//call register self when we get loaded
registerSelf();

