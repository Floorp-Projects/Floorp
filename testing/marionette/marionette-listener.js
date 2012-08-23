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
  addMessageListenerId("Marionette:isElementEnabled", isElementEnabled);
  addMessageListenerId("Marionette:isElementSelected", isElementSelected);
  addMessageListenerId("Marionette:sendKeysToElement", sendKeysToElement);
  addMessageListenerId("Marionette:clearElement", clearElement);
  addMessageListenerId("Marionette:switchToFrame", switchToFrame);
  addMessageListenerId("Marionette:deleteSession", deleteSession);
  addMessageListenerId("Marionette:sleepSession", sleepSession);
  addMessageListenerId("Marionette:emulatorCmdResult", emulatorCmdResult);
  addMessageListenerId("Marionette:importScript", importScript);
  addMessageListenerId("Marionette:getAppCacheStatus", getAppCacheStatus);
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
  removeMessageListenerId("Marionette:isElementEnabled", isElementEnabled);
  removeMessageListenerId("Marionette:isElementSelected", isElementSelected);
  removeMessageListenerId("Marionette:sendKeysToElement", sendKeysToElement);
  removeMessageListenerId("Marionette:clearElement", clearElement);
  removeMessageListenerId("Marionette:switchToFrame", switchToFrame);
  removeMessageListenerId("Marionette:deleteSession", deleteSession);
  removeMessageListenerId("Marionette:sleepSession", sleepSession);
  removeMessageListenerId("Marionette:emulatorCmdResult", emulatorCmdResult);
  removeMessageListenerId("Marionette:importScript", importScript);
  removeMessageListenerId("Marionette:getAppCacheStatus", getAppCacheStatus);
  this.elementManager.reset();
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
  curWin = content;
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

  let marionette = new Marionette(this, aWindow, "content", marionetteLogObj, marionettePerf);
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
  sandbox.marionette.timeout = marionetteTimeout;

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
  let foundFrame = null;
  if ((msg.json.value == null) && (msg.json.element == null)) {
    curWindow = content;
    curWindow.focus();
    sendOk();
    return;
  }
  if (msg.json.element != undefined) {
    if (elementManager.seenItems[msg.json.element] != undefined) {
      let wantedFrame = elementManager.getKnownElement(msg.json.element, curWindow); //HTMLIFrameElement
      let numFrames = curWindow.frames.length;
      for (let i = 0; i < numFrames; i++) {
        if (curWindow.frames[i].frameElement == wantedFrame) {
          curWindow = curWindow.frames[i]; 
          curWindow.focus();
          sendOk();
          return;
        }
      }
    }
  }
  let frames = curWindow.document.getElementsByTagName("iframe");
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
  if (foundFrame == null) {
    sendError("Unable to locate frame: " + msg.json.value, 8, null);
    return;
  }

  sandbox = null;

  if (curWindow.contentWindow == null) {
    // The frame we want to switch to is a remote frame; notify our parent to handle
    // the switch.
    curWindow = content;
    sendToServer('Marionette:switchToFrame', {win: winUtil.outerWindowID, frame: foundFrame});
  }
  else {
    curWindow = curWindow.contentWindow;
    curWindow.focus();
    sendOk();
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
    file = FileUtils.openFileOutputStream(importedScripts, FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE);
  }
  file.write(msg.json.script, msg.json.script.length);
  file.close();
  sendOk();
}

//call register self when we get loaded
registerSelf();

