/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let Cu = Components.utils;
let uuidGen = Components.classes["@mozilla.org/uuid-generator;1"]
             .getService(Components.interfaces.nsIUUIDGenerator);

let loader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
             .getService(Components.interfaces.mozIJSSubScriptLoader);
loader.loadSubScript("chrome://marionette/content/marionette-simpletest.js");
loader.loadSubScript("chrome://marionette/content/marionette-log-obj.js");
Components.utils.import("chrome://marionette/content/marionette-elements.js");
let utils = {};
utils.window = content;
//load Event/ChromeUtils for use with JS scripts:
loader.loadSubScript("chrome://marionette/content/EventUtils.js", utils)
loader.loadSubScript("chrome://marionette/content/ChromeUtils.js", utils);
loader.loadSubScript("chrome://marionette/content/atoms.js", utils);
let marionetteLogObj = new MarionetteLogObj();

let isB2G = false;

let marionetteTimeout = null;
let winUtil = content.QueryInterface(Components.interfaces.nsIInterfaceRequestor).getInterface(Components.interfaces.nsIDOMWindowUtils);
let listenerId = null; //unique ID of this listener
let activeFrame = null;
let win = content;
let elementManager = new ElementManager([]);

/**
 * Called when listener is first started up. 
 * The listener sends its unique window ID and its current URI to the actor.
 * If the actor returns an ID, we start the listeners. Otherwise, nothing happens.
 */
function registerSelf() {
  let register = sendSyncMessage("Marionette:register", {value: winUtil.outerWindowID, href: content.location.href});
  
  if (register[0]) {
    listenerId = register[0];
    startListeners();
  }
}

/**
 * Start all message listeners
 */
function startListeners() {
  addMessageListener("Marionette:newSession" + listenerId, newSession);
  addMessageListener("Marionette:executeScript" + listenerId, executeScript);
  addMessageListener("Marionette:setScriptTimeout" + listenerId, setScriptTimeout);
  addMessageListener("Marionette:executeAsyncScript" + listenerId, executeAsyncScript);
  addMessageListener("Marionette:executeJSScript" + listenerId, executeJSScript);
  addMessageListener("Marionette:setSearchTimeout" + listenerId, setSearchTimeout);
  addMessageListener("Marionette:goUrl" + listenerId, goUrl);
  addMessageListener("Marionette:getUrl" + listenerId, getUrl);
  addMessageListener("Marionette:goBack" + listenerId, goBack);
  addMessageListener("Marionette:goForward" + listenerId, goForward);
  addMessageListener("Marionette:refresh" + listenerId, refresh);
  addMessageListener("Marionette:findElementContent" + listenerId, findElementContent);
  addMessageListener("Marionette:findElementsContent" + listenerId, findElementsContent);
  addMessageListener("Marionette:clickElement" + listenerId, clickElement);
  addMessageListener("Marionette:getAttributeValue" + listenerId, getAttributeValue);
  addMessageListener("Marionette:getElementText" + listenerId, getElementText);
  addMessageListener("Marionette:isElementDisplayed" + listenerId, isElementDisplayed);
  addMessageListener("Marionette:isElementEnabled" + listenerId, isElementEnabled);
  addMessageListener("Marionette:isElementSelected" + listenerId, isElementSelected);
  addMessageListener("Marionette:sendKeysToElement" + listenerId, sendKeysToElement);
  addMessageListener("Marionette:clearElement" + listenerId, clearElement);
  addMessageListener("Marionette:switchToFrame" + listenerId, switchToFrame);
  addMessageListener("Marionette:deleteSession" + listenerId, deleteSession);
  addMessageListener("Marionette:sleepSession" + listenerId, sleepSession);
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
  removeMessageListener("Marionette:newSession" + listenerId, newSession);
  removeMessageListener("Marionette:executeScript" + listenerId, executeScript);
  removeMessageListener("Marionette:setScriptTimeout" + listenerId, setScriptTimeout);
  removeMessageListener("Marionette:executeAsyncScript" + listenerId, executeAsyncScript);
  removeMessageListener("Marionette:executeJSScript" + listenerId, executeJSScript);
  removeMessageListener("Marionette:setSearchTimeout" + listenerId, setSearchTimeout);
  removeMessageListener("Marionette:goUrl" + listenerId, goUrl);
  removeMessageListener("Marionette:getUrl" + listenerId, getUrl);
  removeMessageListener("Marionette:goBack" + listenerId, goBack);
  removeMessageListener("Marionette:goForward" + listenerId, goForward);
  removeMessageListener("Marionette:refresh" + listenerId, refresh);
  removeMessageListener("Marionette:findElementContent" + listenerId, findElementContent);
  removeMessageListener("Marionette:findElementsContent" + listenerId, findElementsContent);
  removeMessageListener("Marionette:clickElement" + listenerId, clickElement);
  removeMessageListener("Marionette:getAttributeValue" + listenerId, getAttributeValue);
  removeMessageListener("Marionette:getElementText" + listenerId, getElementText);
  removeMessageListener("Marionette:isElementDisplayed" + listenerId, isElementDisplayed);
  removeMessageListener("Marionette:isElementEnabled" + listenerId, isElementEnabled);
  removeMessageListener("Marionette:isElementSelected" + listenerId, isElementSelected);
  removeMessageListener("Marionette:sendKeysToElement" + listenerId, sendKeysToElement);
  removeMessageListener("Marionette:clearElement" + listenerId, clearElement);
  removeMessageListener("Marionette:switchToFrame" + listenerId, switchToFrame);
  removeMessageListener("Marionette:deleteSession" + listenerId, deleteSession);
  removeMessageListener("Marionette:sleepSession" + listenerId, sleepSession);
  this.elementManager.reset();
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
  marionetteTimeout = null;
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
function createExecuteContentSandbox(aWindow, marionette, args) {
  try {
    args = elementManager.convertWrappedArguments(args, aWindow);
  }
  catch(e) {
    sendError(e.message, e.num, e.stack);
    return;
  }

  let sandbox = new Cu.Sandbox(aWindow);
  sandbox.window = aWindow;
  sandbox.document = sandbox.window.document;
  sandbox.navigator = sandbox.window.navigator;
  sandbox.__namedArgs = elementManager.applyNamedArgs(args);
  sandbox.__marionetteParams = args;
  sandbox.__proto__ = sandbox.window;
  sandbox.testUtils = utils;

  marionette.exports.forEach(function(fn) {
    sandbox[fn] = marionette[fn].bind(marionette);
  });

  return sandbox;
}

/**
 * Execute the given script either as a function body (executeScript)
 * or directly (for 'mochitest' like JS Marionette tests)
 */
function executeScript(msg, directInject) {
  let script = msg.json.value;
  let marionette = new Marionette(false, win, "content", marionetteLogObj);

  let sandbox = createExecuteContentSandbox(win, marionette, msg.json.args);
  if (!sandbox)
    return;

  sandbox.finish = function sandbox_finish() {
    return marionette.generate_results();
  };

  try {
    if (directInject) {
      let res = Cu.evalInSandbox(script, sandbox, "1.8");
      sendSyncMessage("Marionette:testLog", {value: elementManager.wrapValue(marionetteLogObj.getLogs())});
      marionetteLogObj.clearLogs();
      if (res == undefined || res.passed == undefined) {
        sendError("Marionette.finish() not called", 17, null);
      }
      else {
        sendResponse({value: elementManager.wrapValue(res)});
      }
    }
    else {
      let scriptSrc = "let __marionetteFunc = function(){" + script + "};" +
                      "__marionetteFunc.apply(null, __marionetteParams);";
      let res = Cu.evalInSandbox(scriptSrc, sandbox, "1.8");
      sendSyncMessage("Marionette:testLog", {value: elementManager.wrapValue(marionetteLogObj.getLogs())});
      marionetteLogObj.clearLogs();
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
  win.addEventListener("unload", errUnload, false);
  let script = msg.json.value;
  let command_id = msg.json.id;

  // Error code 28 is scriptTimeout, but spec says execute_async should return 21 (Timeout),
  // see http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/execute_async.
  // However Selenium code returns 28, see
  // http://code.google.com/p/selenium/source/browse/trunk/javascript/firefox-driver/js/evaluate.js.
  // We'll stay compatible with the Selenium code.
  let timeoutId = win.setTimeout(function() {
    contentAsyncReturnFunc('timed out', 28);
  }, marionetteTimeout);
  win.addEventListener('error', function win__onerror(evt) {
    win.removeEventListener('error', win__onerror, true);
    contentAsyncReturnFunc(evt, 17);
    return true;
  }, true);

  function contentAsyncReturnFunc(value, status) {
    win.removeEventListener("unload", errUnload, false);

    /* clear all timeouts potentially generated by the script*/
    for(let i=0; i<=timeoutId; i++) {
      win.clearTimeout(i);
    }

    sendSyncMessage("Marionette:testLog", {value: elementManager.wrapValue(marionetteLogObj.getLogs())});
    marionetteLogObj.clearLogs();
    if (status == 0){
      sendResponse({value: elementManager.wrapValue(value), status: status}, command_id);
    }
    else {
      sendError(value, status, null, command_id);
    }
  };

  let scriptSrc;
  if (timeout) {
    if (marionetteTimeout == null || marionetteTimeout == 0) {
      sendError("Please set a timeout", 21, null);
    }
    scriptSrc = script;
  }
  else {
    scriptSrc = "let marionetteScriptFinished = function(value) { return asyncComplete(value,0);};" +
                "__marionetteParams.push(marionetteScriptFinished);" +
                "let __marionetteFunc = function() { " + script + "};" +
                "__marionetteFunc.apply(null, __marionetteParams); ";
  }

  let marionette = new Marionette(true, win, "content", marionetteLogObj);

  let sandbox = createExecuteContentSandbox(win, marionette, msg.json.args);
  if (!sandbox)
    return;

  sandbox.asyncComplete = contentAsyncReturnFunc;
  sandbox.finish = function sandbox_finish() {
    contentAsyncReturnFunc(marionette.generate_results(), 0);
  };

  try {
   Cu.evalInSandbox(scriptSrc, sandbox, "1.8");
  } catch (e) {
    // 17 = JavascriptException
    sendError(e.name + ': ' + e.message, 17, e.stack);
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
    sendError(e.message, e.num, e.stack);
    return;
  }
  sendOk();
}

/**
 * Navigate to URI. Handles the case where we navigate within an iframe.
 * All other navigation is handled by the server (in chrome space).
 */
function goUrl(msg) {
  if (activeFrame != null) {
    win.document.location = msg.json.value;
    //TODO: replace this with event firing when Bug 720714 is resolved
    let checkTimer = Components.classes["@mozilla.org/timer;1"].createInstance(Components.interfaces.nsITimer);
    let checkLoad = function () { 
                      if (win.document.readyState == "complete") { 
                        sendOk();
                      } 
                      else { 
                        checkTimer.initWithCallback(checkLoad, 100, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
                      }
                    };
    checkLoad();
  }
  else {
    sendAsyncMessage("Marionette:goUrl", {value: msg.json.value});
  }
}

/**
 * Get the current URI
 */
function getUrl(msg) {
  sendResponse({value: win.location.href});
}

/**
 * Go back in history 
 */
function goBack(msg) {
  win.history.back();
  sendOk();
}

/**
 * Go forward in history 
 */
function goForward(msg) {
  win.history.forward();
  sendOk();
}

/**
 * Refresh the page
 */
function refresh(msg) {
  win.location.reload(true);
  let listen = function() { removeEventListener("DOMContentLoaded", arguments.callee, false); sendOk() } ;
  addEventListener("DOMContentLoaded", listen, false);
}

/**
 * Find an element in the document using requested search strategy 
 */
function findElementContent(msg) {
  let id;
  try {
    let notify = function(id) { sendResponse({value:id});};
    let curWin = activeFrame ? win.frames[activeFrame] : win;
    id = elementManager.find(curWin, msg.json, notify, false);
  }
  catch (e) {
    sendError(e.message, e.num, e.stack);
  }
}

/**
 * Find elements in the document using requested search strategy 
 */
function findElementsContent(msg) {
  let id;
  try {
    let notify = function(id) { sendResponse({value:id});};
    let curWin = activeFrame ? win.frames[activeFrame] : win;
    id = elementManager.find(curWin, msg.json, notify, true);
  }
  catch (e) {
    sendError(e.message, e.num, e.stack);
  }
}

/**
 * Send click event to element
 */
function clickElement(msg) {
  let el;
  try {
    //el = elementManager.click(msg.json.element, win);
    el = elementManager.getKnownElement(msg.json.element, win);
    utils.click(el);
    sendOk();
  }
  catch (e) {
    sendError(e.message, e.num, e.stack);
  }
}

/**
 * Get a given attribute of an element
 */
function getAttributeValue(msg) {
  try {
    let el = elementManager.getKnownElement(msg.json.element, win);
    sendResponse({value: utils.getAttributeValue(el, msg.json.name)});
  }
  catch (e) {
    sendError(e.message, e.num, e.stack);
  }
}

/**
 * Get the text of this element. This includes text from child elements.
 */
function getElementText(msg) {
  try {
    let el = elementManager.getKnownElement(msg.json.element, win);
    sendResponse({value: utils.getElementText(el)});
  }
  catch (e) {
    sendError(e.message, e.num, e.stack);
  }
}

/**
 * Check if element is displayed
 */
function isElementDisplayed(msg) {
  try {
    let el = elementManager.getKnownElement(msg.json.element, win);
    sendResponse({value: utils.isElementDisplayed(el)});
  }
  catch (e) {
    sendError(e.message, e.num, e.stack);
  }
}

/**
 * Check if element is enabled
 */
function isElementEnabled(msg) {
  try {
    let el = elementManager.getKnownElement(msg.json.element, win);
    sendResponse({value: utils.isElementEnabled(el)});
  }
  catch (e) {
    sendError(e.message, e.num, e.stack);
  }
}

/**
 * Check if element is selected
 */
function isElementSelected(msg) {
  try {
    let el = elementManager.getKnownElement(msg.json.element, win);
    sendResponse({value: utils.isElementSelected(el)});
  }
  catch (e) {
    sendError(e.message, e.num, e.stack);
  }
}

/**
 * Send keys to element
 */
function sendKeysToElement(msg) {
  try {
    let el = elementManager.getKnownElement(msg.json.element, win);
    utils.sendKeysToElement(el, msg.json.value);
    sendOk();
  }
  catch (e) {
    sendError(e.message, e.num, e.stack);
  }
}

/**
 * Clear the text of an element
 */
function clearElement(msg) {
  try {
    let el = elementManager.getKnownElement(msg.json.element, win);
    utils.clearElement(el);
    sendOk();
  }
  catch (e) {
    sendError(e.message, e.num, e.stack);
  }
}

/**
 * Switch to frame given either the server-assigned element id,
 * its index in window.frames, or the iframe's name or id.
 */
function switchToFrame(msg) {
  let foundFrame = null;
  if ((msg.json.value == null) && (msg.json.element == null)) {
    win = content;
    activeFrame = null;
    content.focus();
    sendOk();
    return;
  }
  if (msg.json.element != undefined) {
    if (elementManager.seenItems[msg.json.element] != undefined) {
      let wantedFrame = elementManager.getKnownElement(msg.json.element, win);//HTMLIFrameElement
      let numFrames = win.frames.length;
      for (let i = 0; i < numFrames; i++) {
        if (win.frames[i].frameElement == wantedFrame) {
          win = win.frames[i]; 
          activeFrame = i;
          win.focus();
          sendOk();
          return;
        }
      }
    }
  }
  switch(typeof(msg.json.value)) {
    case "string" :
      let foundById = null;
      let numFrames = win.frames.length;
      for (let i = 0; i < numFrames; i++) {
        //give precedence to name
        let frame = win.frames[i];
        let frameElement = frame.frameElement;
        if (frameElement.name == msg.json.value) {
          foundFrame = i;
          break;
        } else if ((foundById == null) && (frameElement.id == msg.json.value)) {
          foundById = i;
        }
      }
      if ((foundFrame == null) && (foundById != null)) {
        foundFrame = foundById;
      }
      break;
    case "number":
      if (win.frames[msg.json.value] != undefined) {
        foundFrame = msg.json.value;
      }
      break;
  }
  //TODO: implement index
  if (foundFrame != null) {
    let frameWindow = win.frames[foundFrame];
    activeFrame = foundFrame;
    win = frameWindow;
    win.focus();
    sendOk();
  } else {
    sendError("Unable to locate frame: " + msg.json.value, 8, null);
  }
}

//call register self when we get loaded
registerSelf();
