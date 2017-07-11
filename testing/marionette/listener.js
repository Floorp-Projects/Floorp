/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */
/* global XPCNativeWrapper */

"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

var uuidGen = Cc["@mozilla.org/uuid-generator;1"]
    .getService(Ci.nsIUUIDGenerator);

var loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
    .getService(Ci.mozIJSSubScriptLoader);

Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("chrome://marionette/content/accessibility.js");
Cu.import("chrome://marionette/content/action.js");
Cu.import("chrome://marionette/content/atom.js");
Cu.import("chrome://marionette/content/capture.js");
Cu.import("chrome://marionette/content/element.js");
const {
  ElementNotInteractableError,
  error,
  InsecureCertificateError,
  InvalidArgumentError,
  InvalidElementStateError,
  InvalidSelectorError,
  NoSuchElementError,
  NoSuchFrameError,
  TimeoutError,
  UnknownError,
} = Cu.import("chrome://marionette/content/error.js", {});
Cu.import("chrome://marionette/content/evaluate.js");
Cu.import("chrome://marionette/content/event.js");
Cu.import("chrome://marionette/content/interaction.js");
Cu.import("chrome://marionette/content/legacyaction.js");
Cu.import("chrome://marionette/content/navigate.js");
Cu.import("chrome://marionette/content/proxy.js");
Cu.import("chrome://marionette/content/session.js");

Cu.importGlobalProperties(["URL"]);

var marionetteTestName;
var winUtil = content.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIDOMWindowUtils);
var listenerId = null; // unique ID of this listener
var curContainer = {frame: content, shadowRoot: null};
var previousContainer = null;

var seenEls = new element.Store();
var SUPPORTED_STRATEGIES = new Set([
  element.Strategy.ClassName,
  element.Strategy.Selector,
  element.Strategy.ID,
  element.Strategy.Name,
  element.Strategy.LinkText,
  element.Strategy.PartialLinkText,
  element.Strategy.TagName,
  element.Strategy.XPath,
]);

var capabilities;

var legacyactions = new legacyaction.Chain(checkForInterrupted);

// the unload handler
var onunload;

// Flag to indicate whether an async script is currently running or not.
var asyncTestRunning = false;
var asyncTestCommandId;
var asyncTestTimeoutId;

var inactivityTimeoutId = null;

var originalOnError;
// Send move events about this often
var EVENT_INTERVAL = 30; // milliseconds
// last touch for each fingerId
var multiLast = {};
var asyncChrome = proxy.toChromeAsync({
  addMessageListener: addMessageListenerId.bind(this),
  removeMessageListener: removeMessageListenerId.bind(this),
  sendAsyncMessage: sendAsyncMessage.bind(this),
});
var syncChrome = proxy.toChrome(sendSyncMessage.bind(this));

var logger = Log.repository.getLogger("Marionette");
// Append only once to avoid duplicated output after listener.js gets reloaded
if (logger.ownAppenders.length == 0) {
  logger.addAppender(new Log.DumpAppender());
}

var modalHandler = function() {
  // This gets called on the system app only since it receives the
  // mozbrowserprompt event
  sendSyncMessage("Marionette:switchedToFrame",
      {frameValue: null, storePrevious: true});
  let isLocal = sendSyncMessage("MarionetteFrame:handleModal", {})[0].value;
  if (isLocal) {
    previousContainer = curContainer;
  }
  curContainer = {frame: content, shadowRoot: null};
};

// sandbox storage and name of the current sandbox
var sandboxes = new Sandboxes(() => curContainer.frame);
var sandboxName = "default";

/**
 * The load listener singleton helps to keep track of active page load
 * activities, and can be used by any command which might cause a navigation
 * to happen. In the specific case of a reload of the frame script it allows
 * to continue observing the current page load.
 */
var loadListener = {
  command_id: null,
  seenBeforeUnload: false,
  seenUnload: false,
  timeout: null,
  timerPageLoad: null,
  timerPageUnload: null,

  /**
   * Start listening for page unload/load events.
   *
   * @param {number} command_id
   *     ID of the currently handled message between the driver and
   *     listener.
   * @param {number} timeout
   *     Timeout in seconds the method has to wait for the page being
   *     finished loading.
   * @param {number} startTime
   *     Unix timestap when the navitation request got triggered.
   * @param {boolean=} waitForUnloaded
   *     If true wait for page unload events, otherwise only for page
   *     load events.
   */
  start(command_id, timeout, startTime, waitForUnloaded = true) {
    this.command_id = command_id;
    this.timeout = timeout;

    this.seenBeforeUnload = false;
    this.seenUnload = false;

    this.timerPageLoad = Cc["@mozilla.org/timer;1"]
        .createInstance(Ci.nsITimer);
    this.timerPageUnload = null;

    // In case the frame script has been reloaded, wait the remaining time
    timeout = startTime + timeout - new Date().getTime();

    if (timeout <= 0) {
      this.notify(this.timerPageLoad);
      return;
    }

    if (waitForUnloaded) {
      addEventListener("hashchange", this, false);
      addEventListener("pagehide", this, false);

      // The events can only be received when the event listeners are
      // added to the currently selected frame.
      curContainer.frame.addEventListener("beforeunload", this);
      curContainer.frame.addEventListener("unload", this);

      Services.obs.addObserver(this, "outer-window-destroyed");
    } else {
      addEventListener("DOMContentLoaded", loadListener);
      addEventListener("pageshow", loadListener);
    }

    this.timerPageLoad.initWithCallback(
        this, timeout, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  /**
   * Stop listening for page unload/load events.
   */
  stop() {
    if (this.timerPageLoad) {
      this.timerPageLoad.cancel();
    }

    if (this.timerPageUnload) {
      this.timerPageUnload.cancel();
    }

    removeEventListener("hashchange", this);
    removeEventListener("pagehide", this);
    removeEventListener("DOMContentLoaded", this);
    removeEventListener("pageshow", this);

    // If the original content window, where the navigation was triggered,
    // doesn't exist anymore, exceptions can be silently ignored.
    try {
      curContainer.frame.removeEventListener("beforeunload", this);
      curContainer.frame.removeEventListener("unload", this);
    } catch (e) {
      if (e.name != "TypeError") {
        throw e;
      }
    }

    // In case the observer was added before the frame script has been
    // reloaded, it will no longer be available. Exceptions can be ignored.
    try {
      Services.obs.removeObserver(this, "outer-window-destroyed");
    } catch (e) {}
  },

  /**
   * Callback for registered DOM events.
   */
  handleEvent(event) {
    // Only care about events from the currently selected browsing context,
    // whereby some of those do not bubble up to the window.
    if (event.target != curContainer.frame &&
        event.target != curContainer.frame.document) {
      return;
    }

    let location = event.target.documentURI || event.target.location.href;
    logger.debug(`Received DOM event "${event.type}" for "${location}"`);

    switch (event.type) {
      case "beforeunload":
        this.seenBeforeUnload = true;
        break;

      case "unload":
        this.seenUnload = true;
        break;

      case "pagehide":
        this.seenUnload = true;

        removeEventListener("hashchange", this);
        removeEventListener("pagehide", this);

        // Now wait until the target page has been loaded
        addEventListener("DOMContentLoaded", this, false);
        addEventListener("pageshow", this, false);

        break;

      case "hashchange":
        this.stop();
        sendOk(this.command_id);

        break;

      case "DOMContentLoaded":
        if (event.target.documentURI.startsWith("about:certerror")) {
          this.stop();
          sendError(new InsecureCertificateError(), this.command_id);

        } else if (/about:.*(error)\?/.exec(event.target.documentURI)) {
          this.stop();
          sendError(new UnknownError("Reached error page: " +
              event.target.documentURI), this.command_id);

        // Return early with a page load strategy of eager, and also
        // special-case about:blocked pages which should be treated as
        // non-error pages but do not raise a pageshow event.
        } else if ((capabilities.get("pageLoadStrategy") ===
            session.PageLoadStrategy.Eager) ||
            /about:blocked\?/.exec(event.target.documentURI)) {
          this.stop();
          sendOk(this.command_id);
        }

        break;

      case "pageshow":
        this.stop();
        sendOk(this.command_id);

        break;
    }
  },

  /**
   * Callback for navigation timeout timer.
   */
  notify(timer) {
    switch (timer) {
      case this.timerPageUnload:
        // In the case when a document has a beforeunload handler
        // registered, the currently active command will return immediately
        // due to the modal dialog observer in proxy.js.
        //
        // Otherwise the timeout waiting for the document to start
        // navigating is increased by 5000 ms to ensure a possible load
        // event is not missed. In the common case such an event should
        // occur pretty soon after beforeunload, and we optimise for this.
        if (this.seenBeforeUnload) {
          this.seenBeforeUnload = null;
          this.timerPageUnload.initWithCallback(
              this, 5000, Ci.nsITimer.TYPE_ONE_SHOT)

        // If no page unload has been detected, ensure to properly stop
        // the load listener, and return from the currently active command.
        } else if (!this.seenUnload) {
          logger.debug("Canceled page load listener because no navigation " +
              "has been detected");
          this.stop();
          sendOk(this.command_id);
        }
        break;

      case this.timerPageLoad:
        this.stop();
        sendError(
            new TimeoutError(`Timeout loading page after ${this.timeout}ms`),
            this.command_id);
        break;
    }
  },

  observe(subject, topic, data) {
    const win = curContainer.frame;
    const winID = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
    const curWinID = win.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindowUtils).outerWindowID;

    logger.debug(`Received observer notification "${topic}" for "${winID}"`);

    switch (topic) {
      // In the case when the currently selected frame is closed,
      // there will be no further load events. Stop listening immediately.
      case "outer-window-destroyed":
        if (curWinID === winID) {
          this.stop();
          sendOk(this.command_id);
        }
        break;
    }
  },

  /**
   * Continue to listen for page load events after the frame script has been
   * reloaded.
   *
   * @param {number} command_id
   *     ID of the currently handled message between the driver and
   *     listener.
   * @param {number} timeout
   *     Timeout in milliseconds the method has to wait for the page
   *     being finished loading.
   * @param {number} startTime
   *     Unix timestap when the navitation request got triggered.
   */
  waitForLoadAfterFramescriptReload(command_id, timeout, startTime) {
    this.start(command_id, timeout, startTime, false);
  },

  /**
   * Use a trigger callback to initiate a page load, and attach listeners if
   * a page load is expected.
   *
   * @param {function} trigger
   *     Callback that triggers the page load.
   * @param {number} command_id
   *     ID of the currently handled message between the driver and listener.
   * @param {number} pageTimeout
   *     Timeout in milliseconds the method has to wait for the page
   *    finished loading.
   * @param {boolean=} loadEventExpected
   *     Optional flag, which indicates that navigate has to wait for the page
   *     finished loading.
   * @param {string=} url
   *     Optional URL, which is used to check if a page load is expected.
   */
  navigate(trigger, command_id, timeout, loadEventExpected = true,
      useUnloadTimer = false) {

    // Only wait if the page load strategy is not `none`
    loadEventExpected = loadEventExpected &&
        (capabilities.get("pageLoadStrategy") !==
        session.PageLoadStrategy.None);

    if (loadEventExpected) {
      let startTime = new Date().getTime();
      this.start(command_id, timeout, startTime, true);
    }

    return Task.spawn(function* () {
      yield trigger();

    }).then(val => {
      if (!loadEventExpected) {
        sendOk(command_id);
        return;
      }

      // If requested setup a timer to detect a possible page load
      if (useUnloadTimer) {
        this.timerPageUnload = Cc["@mozilla.org/timer;1"]
            .createInstance(Ci.nsITimer);
        this.timerPageUnload.initWithCallback(
            this, 200, Ci.nsITimer.TYPE_ONE_SHOT);
      }

    }).catch(err => {
      if (loadEventExpected) {
        this.stop();
      }

      sendError(err, command_id);

    });
  },
}

/**
 * Called when listener is first started up.  The listener sends its
 * unique window ID and its current URI to the actor.  If the actor returns
 * an ID, we start the listeners. Otherwise, nothing happens.
 */
function registerSelf() {
  let msg = {value: winUtil.outerWindowID};
  logger.debug(`Register listener.js for window ${msg.value}`);

  // register will have the ID and a boolean describing if this is the
  // main process or not
  let register = sendSyncMessage("Marionette:register", msg);
  if (register[0]) {
    listenerId = register[0][0];
    capabilities = session.Capabilities.fromJSON(register[0][1]);
    if (typeof listenerId != "undefined") {
      startListeners();
      sendAsyncMessage("Marionette:listenersAttached",
          {"listenerId": listenerId});
    }
  }
}

// Eventually we will not have a closure for every single command, but
// use a generic dispatch for all listener commands.
//
// Perhaps one could even conceive having a separate instance of
// CommandProcessor for the listener, because the code is mostly the same.
function dispatch(fn) {
  if (typeof fn != "function") {
    throw new TypeError("Provided dispatch handler is not a function");
  }

  return function(msg) {
    let id = msg.json.command_id;

    let req = Task.spawn(function* () {
      if (typeof msg.json == "undefined" || msg.json instanceof Array) {
        return yield fn.apply(null, msg.json);
      }
      return yield fn(msg.json);
    });

    let okOrValueResponse = rv => {
      if (typeof rv == "undefined") {
        sendOk(id);
      } else {
        sendResponse(rv, id);
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

var getPageSourceFn = dispatch(getPageSource);
var getActiveElementFn = dispatch(getActiveElement);
var getElementAttributeFn = dispatch(getElementAttribute);
var getElementPropertyFn = dispatch(getElementProperty);
var getElementTextFn = dispatch(getElementText);
var getElementTagNameFn = dispatch(getElementTagName);
var getElementRectFn = dispatch(getElementRect);
var isElementEnabledFn = dispatch(isElementEnabled);
var findElementContentFn = dispatch(findElementContent);
var findElementsContentFn = dispatch(findElementsContent);
var isElementSelectedFn = dispatch(isElementSelected);
var clearElementFn = dispatch(clearElement);
var isElementDisplayedFn = dispatch(isElementDisplayed);
var getElementValueOfCssPropertyFn = dispatch(getElementValueOfCssProperty);
var switchToShadowRootFn = dispatch(switchToShadowRoot);
var singleTapFn = dispatch(singleTap);
var takeScreenshotFn = dispatch(takeScreenshot);
var performActionsFn = dispatch(performActions);
var releaseActionsFn = dispatch(releaseActions);
var actionChainFn = dispatch(actionChain);
var multiActionFn = dispatch(multiAction);
var executeFn = dispatch(execute);
var executeInSandboxFn = dispatch(executeInSandbox);
var sendKeysToElementFn = dispatch(sendKeysToElement);
var reftestWaitFn = dispatch(reftestWait);

/**
 * Start all message listeners
 */
function startListeners() {
  addMessageListenerId("Marionette:newSession", newSession);
  addMessageListenerId("Marionette:execute", executeFn);
  addMessageListenerId("Marionette:executeInSandbox", executeInSandboxFn);
  addMessageListenerId("Marionette:singleTap", singleTapFn);
  addMessageListenerId("Marionette:performActions", performActionsFn);
  addMessageListenerId("Marionette:releaseActions", releaseActionsFn);
  addMessageListenerId("Marionette:actionChain", actionChainFn);
  addMessageListenerId("Marionette:multiAction", multiActionFn);
  addMessageListenerId("Marionette:get", get);
  addMessageListenerId("Marionette:waitForPageLoaded", waitForPageLoaded);
  addMessageListenerId("Marionette:cancelRequest", cancelRequest);
  addMessageListenerId("Marionette:getPageSource", getPageSourceFn);
  addMessageListenerId("Marionette:goBack", goBack);
  addMessageListenerId("Marionette:goForward", goForward);
  addMessageListenerId("Marionette:refresh", refresh);
  addMessageListenerId("Marionette:findElementContent", findElementContentFn);
  addMessageListenerId(
      "Marionette:findElementsContent", findElementsContentFn);
  addMessageListenerId("Marionette:getActiveElement", getActiveElementFn);
  addMessageListenerId("Marionette:clickElement", clickElement);
  addMessageListenerId(
      "Marionette:getElementAttribute", getElementAttributeFn);
  addMessageListenerId("Marionette:getElementProperty", getElementPropertyFn);
  addMessageListenerId("Marionette:getElementText", getElementTextFn);
  addMessageListenerId("Marionette:getElementTagName", getElementTagNameFn);
  addMessageListenerId("Marionette:isElementDisplayed", isElementDisplayedFn);
  addMessageListenerId(
      "Marionette:getElementValueOfCssProperty",
      getElementValueOfCssPropertyFn);
  addMessageListenerId("Marionette:getElementRect", getElementRectFn);
  addMessageListenerId("Marionette:isElementEnabled", isElementEnabledFn);
  addMessageListenerId("Marionette:isElementSelected", isElementSelectedFn);
  addMessageListenerId("Marionette:sendKeysToElement", sendKeysToElementFn);
  addMessageListenerId("Marionette:clearElement", clearElementFn);
  addMessageListenerId("Marionette:switchToFrame", switchToFrame);
  addMessageListenerId("Marionette:switchToParentFrame", switchToParentFrame);
  addMessageListenerId("Marionette:switchToShadowRoot", switchToShadowRootFn);
  addMessageListenerId("Marionette:deleteSession", deleteSession);
  addMessageListenerId("Marionette:sleepSession", sleepSession);
  addMessageListenerId("Marionette:takeScreenshot", takeScreenshotFn);
  addMessageListenerId("Marionette:reftestWait", reftestWaitFn);
}

/**
 * Called when we start a new session. It registers the
 * current environment, and resets all values
 */
function newSession(msg) {
  capabilities = session.Capabilities.fromJSON(msg.json);
  resetValues();
}

/**
 * Puts the current session to sleep, so all listeners are removed except
 * for the 'restart' listener.
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
  registerSelf();
}

/**
 * Removes all listeners
 */
function deleteSession(msg) {
  removeMessageListenerId("Marionette:newSession", newSession);
  removeMessageListenerId("Marionette:execute", executeFn);
  removeMessageListenerId("Marionette:executeInSandbox", executeInSandboxFn);
  removeMessageListenerId("Marionette:singleTap", singleTapFn);
  removeMessageListenerId("Marionette:performActions", performActionsFn);
  removeMessageListenerId("Marionette:releaseActions", releaseActionsFn);
  removeMessageListenerId("Marionette:actionChain", actionChainFn);
  removeMessageListenerId("Marionette:multiAction", multiActionFn);
  removeMessageListenerId("Marionette:get", get);
  removeMessageListenerId("Marionette:waitForPageLoaded", waitForPageLoaded);
  removeMessageListenerId("Marionette:cancelRequest", cancelRequest);
  removeMessageListenerId("Marionette:getPageSource", getPageSourceFn);
  removeMessageListenerId("Marionette:goBack", goBack);
  removeMessageListenerId("Marionette:goForward", goForward);
  removeMessageListenerId("Marionette:refresh", refresh);
  removeMessageListenerId(
      "Marionette:findElementContent", findElementContentFn);
  removeMessageListenerId(
      "Marionette:findElementsContent", findElementsContentFn);
  removeMessageListenerId("Marionette:getActiveElement", getActiveElementFn);
  removeMessageListenerId("Marionette:clickElement", clickElement);
  removeMessageListenerId(
      "Marionette:getElementAttribute", getElementAttributeFn);
  removeMessageListenerId(
      "Marionette:getElementProperty", getElementPropertyFn);
  removeMessageListenerId(
      "Marionette:getElementText", getElementTextFn);
  removeMessageListenerId(
      "Marionette:getElementTagName", getElementTagNameFn);
  removeMessageListenerId(
      "Marionette:isElementDisplayed", isElementDisplayedFn);
  removeMessageListenerId(
      "Marionette:getElementValueOfCssProperty",
      getElementValueOfCssPropertyFn);
  removeMessageListenerId("Marionette:getElementRect", getElementRectFn);
  removeMessageListenerId("Marionette:isElementEnabled", isElementEnabledFn);
  removeMessageListenerId(
      "Marionette:isElementSelected", isElementSelectedFn);
  removeMessageListenerId(
      "Marionette:sendKeysToElement", sendKeysToElementFn);
  removeMessageListenerId("Marionette:clearElement", clearElementFn);
  removeMessageListenerId("Marionette:switchToFrame", switchToFrame);
  removeMessageListenerId(
      "Marionette:switchToParentFrame", switchToParentFrame);
  removeMessageListenerId(
      "Marionette:switchToShadowRoot", switchToShadowRootFn);
  removeMessageListenerId("Marionette:deleteSession", deleteSession);
  removeMessageListenerId("Marionette:sleepSession", sleepSession);
  removeMessageListenerId("Marionette:takeScreenshot", takeScreenshotFn);

  seenEls.clear();
  // reset container frame to the top-most frame
  curContainer = {frame: content, shadowRoot: null};
  curContainer.frame.focus();
  legacyactions.touchIds = {};
  if (action.inputStateMap !== undefined) {
    action.inputStateMap.clear();
  }
  if (action.inputsToCancel !== undefined) {
    action.inputsToCancel.length = 0;
  }
}

/**
 * Send asynchronous reply to chrome.
 *
 * @param {UUID} uuid
 *     Unique identifier of the request.
 * @param {AsyncContentSender.ResponseType} type
 *     Type of response.
 * @param {?=} data
 *     JSON serialisable object to accompany the message.  Defaults to
 *     an empty dictionary.
 */
function sendToServer(uuid, data = undefined) {
  let channel = new proxy.AsyncMessageChannel(
      () => this,
      sendAsyncMessage.bind(this));
  channel.reply(uuid, data);
}

/**
 * Send asynchronous reply with value to chrome.
 *
 * @param {?} obj
 *     JSON serialisable object of arbitrary type and complexity.
 * @param {UUID} uuid
 *     Unique identifier of the request.
 */
function sendResponse(obj, uuid) {
  sendToServer(uuid, obj);
}

/**
 * Send asynchronous reply to chrome.
 *
 * @param {UUID} uuid
 *     Unique identifier of the request.
 */
function sendOk(uuid) {
  sendToServer(uuid);
}

/**
 * Send asynchronous error reply to chrome.
 *
 * @param {Error} err
 *     Error to notify chrome of.
 * @param {UUID} uuid
 *     Unique identifier of the request.
 */
function sendError(err, uuid) {
  sendToServer(uuid, err);
}

/**
 * Clear test values after completion of test
 */
function resetValues() {
  sandboxes.clear();
  curContainer = {frame: content, shadowRoot: null};
  legacyactions.mouseEventsOnly = false;
  action.inputStateMap = new Map();
  action.inputsToCancel = [];
}

function wasInterrupted() {
  if (previousContainer) {
    let element = content.document.elementFromPoint(
        (content.innerWidth / 2), (content.innerHeight / 2));
    if (element.id.indexOf("modal-dialog") == -1) {
      return true;
    }
    return false;
  }
  return sendSyncMessage("MarionetteFrame:getInterruptedState", {})[0].value;
}

function checkForInterrupted() {
  if (wasInterrupted()) {
    // if previousContainer is set, then we're in a single process
    // environment
    if (previousContainer) {
      curContainer = legacyactions.container = previousContainer;
      previousContainer = null;
    // else we're in OOP environment, so we'll switch to the original
    // OOP frame
    } else {
      sendSyncMessage("Marionette:switchToModalOrigin");
    }
    sendSyncMessage("Marionette:switchedToFrame", {restorePrevious: true});
  }
}

function* execute(script, args, timeout, opts) {
  opts.timeout = timeout;

  let sb = sandbox.createMutable(curContainer.frame);
  let wargs = evaluate.fromJSON(
      args, seenEls, curContainer.frame, curContainer.shadowRoot);
  let res = yield evaluate.sandbox(sb, script, wargs, opts);

  return evaluate.toJSON(res, seenEls);
}

function* executeInSandbox(script, args, timeout, opts) {
  opts.timeout = timeout;

  let sb = sandboxes.get(opts.sandboxName, opts.newSandbox);
  let wargs = evaluate.fromJSON(
      args, seenEls, curContainer.frame, curContainer.shadowRoot);
  let evaluatePromise = evaluate.sandbox(sb, script, wargs, opts);

  let res = yield evaluatePromise;
  return evaluate.toJSON(res, seenEls);
}

function emitTouchEvent(type, touch) {
  if (!wasInterrupted()) {
    logger.info(`Emitting Touch event of type ${type} ` +
        `to element with id: ${touch.target.id} ` +
        `and tag name: ${touch.target.tagName} ` +
        `at coordinates (${touch.clientX}), ` +
        `${touch.clientY}) relative to the viewport`);

    const win = curContainer.frame;
    let docShell = win.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIWebNavigation)
        .QueryInterface(Ci.nsIDocShell);
    if (docShell.asyncPanZoomEnabled && legacyactions.scrolling) {
      // if we're in APZ and we're scrolling, we must use
      // sendNativeTouchPoint to dispatch our touchmove events
      let index = sendSyncMessage("MarionetteFrame:getCurrentFrameId");

      // only call emitTouchEventForIFrame if we're inside an iframe.
      if (index != null) {
        let ev = {
          index,
          type,
          id: touch.identifier,
          clientX: touch.clientX,
          clientY: touch.clientY,
          screenX: touch.screenX,
          screenY: touch.screenY,
          radiusX: touch.radiusX,
          radiusY: touch.radiusY,
          rotation: touch.rotationAngle,
          force: touch.force,
        };
        sendSyncMessage("Marionette:emitTouchEvent", ev);
        return;
      }
    }

    // we get here if we're not in asyncPacZoomEnabled land, or if we're
    // the main process
    let domWindowUtils = win.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindowUtils);
    domWindowUtils.sendTouchEvent(
        type,
        [touch.identifier],
        [touch.clientX],
        [touch.clientY],
        [touch.radiusX],
        [touch.radiusY],
        [touch.rotationAngle],
        [touch.force],
        1,
        0);
  }
}

/**
 * Function that perform a single tap
 */
function singleTap(id, corx, cory) {
  let el = seenEls.get(id, curContainer);
  // after this block, the element will be scrolled into view
  let visible = element.isVisible(el, corx, cory);
  if (!visible) {
    throw new ElementNotInteractableError(
        "Element is not currently visible and may not be manipulated");
  }

  let a11y = accessibility.get(capabilities.get("moz:accessibilityChecks"));
  return a11y.getAccessible(el, true).then(acc => {
    a11y.assertVisible(acc, el, visible);
    a11y.assertActionable(acc, el);
    if (!curContainer.frame.document.createTouch) {
      legacyactions.mouseEventsOnly = true;
    }
    let c = element.coordinates(el, corx, cory);
    if (!legacyactions.mouseEventsOnly) {
      let touchId = legacyactions.nextTouchId++;
      let touch = createATouch(el, c.x, c.y, touchId);
      emitTouchEvent("touchstart", touch);
      emitTouchEvent("touchend", touch);
    }
    legacyactions.mouseTap(el.ownerDocument, c.x, c.y);
  });
}

/**
 * Function to create a touch based on the element
 * corx and cory are relative to the viewport, id is the touchId
 */
function createATouch(el, corx, cory, touchId) {
  let doc = el.ownerDocument;
  let win = doc.defaultView;
  let [clientX, clientY, pageX, pageY, screenX, screenY] =
      legacyactions.getCoordinateInfo(el, corx, cory);
  let atouch = doc.createTouch(
      win,
      el,
      touchId,
      pageX,
      pageY,
      screenX,
      screenY,
      clientX,
      clientY);
  return atouch;
}

/**
 * Perform a series of grouped actions at the specified points in time.
 *
 * @param {obj} msg
 *      Object with an |actions| attribute that is an Array of objects
 *      each of which represents an action sequence.
 */
function* performActions(msg) {
  let chain = action.Chain.fromJson(msg.actions);
  yield action.dispatch(chain, seenEls, curContainer);
}

/**
 * The Release Actions command is used to release all the keys and pointer
 * buttons that are currently depressed. This causes events to be fired
 * as if the state was released by an explicit series of actions. It also
 * clears all the internal state of the virtual devices.
 */
function* releaseActions() {
  yield action.dispatchTickActions(
      action.inputsToCancel.reverse(), 0, seenEls, curContainer);
  action.inputsToCancel.length = 0;
  action.inputStateMap.clear();
}

/**
 * Start action chain on one finger.
 */
function actionChain(chain, touchId) {
  let touchProvider = {};
  touchProvider.createATouch = createATouch;
  touchProvider.emitTouchEvent = emitTouchEvent;

  return legacyactions.dispatchActions(
      chain,
      touchId,
      curContainer,
      seenEls,
      touchProvider);
}

function emitMultiEvents(type, touch, touches) {
  let target = touch.target;
  let doc = target.ownerDocument;
  let win = doc.defaultView;
  // touches that are in the same document
  let documentTouches = doc.createTouchList(touches.filter(function(t) {
    return ((t.target.ownerDocument === doc) && (type != "touchcancel"));
  }));
  // touches on the same target
  let targetTouches = doc.createTouchList(touches.filter(function(t) {
    return ((t.target === target) &&
        ((type != "touchcancel") || (type != "touchend")));
  }));
  // Create changed touches
  let changedTouches = doc.createTouchList(touch);
  // Create the event object
  let event = doc.createEvent("TouchEvent");
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

function setDispatch(batches, touches, batchIndex = 0) {
  // check if all the sets have been fired
  if (batchIndex >= batches.length) {
    multiLast = {};
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
  let touch;
  let lastTouch;
  let touchIndex;
  let waitTime = 0;
  let maxTime = 0;
  let c;

  // loop through the batch
  batchIndex++;
  for (let i = 0; i < batch.length; i++) {
    pack = batch[i];
    touchId = pack[0];
    command = pack[1];

    switch (command) {
      case "press":
        el = seenEls.get(pack[2], curContainer);
        c = element.coordinates(el, pack[3], pack[4]);
        touch = createATouch(el, c.x, c.y, touchId);
        multiLast[touchId] = touch;
        touches.push(touch);
        emitMultiEvents("touchstart", touch, touches);
        break;

      case "release":
        touch = multiLast[touchId];
        // the index of the previous touch for the finger may change in
        // the touches array
        touchIndex = touches.indexOf(touch);
        touches.splice(touchIndex, 1);
        emitMultiEvents("touchend", touch, touches);
        break;

      case "move":
        el = seenEls.get(pack[2], curContainer);
        c = element.coordinates(el);
        touch = createATouch(multiLast[touchId].target, c.x, c.y, touchId);
        touchIndex = touches.indexOf(lastTouch);
        touches[touchIndex] = touch;
        multiLast[touchId] = touch;
        emitMultiEvents("touchmove", touch, touches);
        break;

      case "moveByOffset":
        el = multiLast[touchId].target;
        lastTouch = multiLast[touchId];
        touchIndex = touches.indexOf(lastTouch);
        let doc = el.ownerDocument;
        let win = doc.defaultView;
        // since x and y are relative to the last touch, therefore,
        // it's relative to the position of the last touch
        let clientX = lastTouch.clientX + pack[2];
        let clientY = lastTouch.clientY + pack[3];
        let pageX = clientX + win.pageXOffset;
        let pageY = clientY + win.pageYOffset;
        let screenX = clientX + win.mozInnerScreenX;
        let screenY = clientY + win.mozInnerScreenY;
        touch = doc.createTouch(
            win,
            el,
            touchId,
            pageX,
            pageY,
            screenX,
            screenY,
            clientX,
            clientY);
        touches[touchIndex] = touch;
        multiLast[touchId] = touch;
        emitMultiEvents("touchmove", touch, touches);
        break;

      case "wait":
        if (typeof pack[2] != "undefined") {
          waitTime = pack[2] * 1000;
          if (waitTime > maxTime) {
            maxTime = waitTime;
          }
        }
        break;
    }
  }

  if (maxTime != 0) {
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback(function() {
      setDispatch(batches, touches, batchIndex);
    }, maxTime, Ci.nsITimer.TYPE_ONE_SHOT);
  } else {
    setDispatch(batches, touches, batchIndex);
  }
}

/**
 * Start multi-action.
 *
 * @param {Number} maxLen
 *     Longest action chain for one finger.
 */
function multiAction(args, maxLen) {
  // unwrap the original nested array
  let commandArray = evaluate.fromJSON(
      args, seenEls, curContainer.frame, curContainer.shadowRoot);
  let concurrentEvent = [];
  let temp;
  for (let i = 0; i < maxLen; i++) {
    let row = [];
    for (let j = 0; j < commandArray.length; j++) {
      if (typeof commandArray[j][i] != "undefined") {
        // add finger id to the front of each action,
        // i.e. [finger_id, action, element]
        temp = commandArray[j][i];
        temp.unshift(j);
        row.push(temp);
      }
    }
    concurrentEvent.push(row);
  }

  // Now concurrent event is made of sets where each set contain a list
  // of actions that need to be fired.
  //
  // But note that each action belongs to a different finger
  // pendingTouches keeps track of current touches that's on the screen.
  let pendingTouches = [];
  setDispatch(concurrentEvent, pendingTouches);
}

/**
 * Cancel the polling and remove the event listener associated with a
 * current navigation request in case we're interupted by an onbeforeunload
 * handler and navigation doesn't complete.
 */
function cancelRequest() {
  loadListener.stop();
}

/**
 * This implements the latter part of a get request (for the case we need
 * to resume one when the frame script has been reloaded in the middle of a
 * navigate request). This is most of of the work of a navigate request,
 * but doesn't assume DOMContentLoaded is yet to fire.
 *
 * @param {number} command_id
 *     ID of the currently handled message between the driver and
 *     listener.
 * @param {number} pageTimeout
 *     Timeout in seconds the method has to wait for the page being
 *     finished loading.
 * @param {number} startTime
 *     Unix timestap when the navitation request got triggred.
 */
function waitForPageLoaded(msg) {
  let {command_id, pageTimeout, startTime} = msg.json;

  loadListener.waitForLoadAfterFramescriptReload(
      command_id, pageTimeout, startTime);
}

/**
 * Navigate to the given URL.  The operation will be performed on the
 * current browsing context, which means it handles the case where we
 * navigate within an iframe.  All other navigation is handled by the driver
 * (in chrome space).
 */
function get(msg) {
  let {command_id, pageTimeout, url, loadEventExpected = null} = msg.json;

  try {
    if (typeof url == "string") {
      try {
        if (loadEventExpected === null) {
          loadEventExpected = navigate.isLoadEventExpected(
              curContainer.frame.location, url);
        }
      } catch (e) {
        let err = new InvalidArgumentError("Malformed URL: " + e.message);
        sendError(err, command_id);
        return;
      }
    }

    // We need to move to the top frame before navigating
    sendSyncMessage("Marionette:switchedToFrame", {frameValue: null});
    curContainer.frame = content;

    loadListener.navigate(() => {
      curContainer.frame.location = url;
    }, command_id, pageTimeout, loadEventExpected);

  } catch (e) {
    sendError(e, command_id);
  }
}

/**
 * Cause the browser to traverse one step backward in the joint history
 * of the current browsing context.
 *
 * @param {number} command_id
 *     ID of the currently handled message between the driver and
 *     listener.
 * @param {number} pageTimeout
 *     Timeout in milliseconds the method has to wait for the page being
 *     finished loading.
 */
function goBack(msg) {
  let {command_id, pageTimeout} = msg.json;

  try {
    loadListener.navigate(() => {
      curContainer.frame.history.back();
    }, command_id, pageTimeout);

  } catch (e) {
    sendError(e, command_id);
  }
}

/**
 * Cause the browser to traverse one step forward in the joint history
 * of the current browsing context.
 *
 * @param {number} command_id
 *     ID of the currently handled message between the driver and
 *     listener.
 * @param {number} pageTimeout
 *     Timeout in milliseconds the method has to wait for the page being
 *     finished loading.
 */
function goForward(msg) {
  let {command_id, pageTimeout} = msg.json;

  try {
    loadListener.navigate(() => {
      curContainer.frame.history.forward();
    }, command_id, pageTimeout);

  } catch (e) {
    sendError(e, command_id);
  }
}

/**
 * Causes the browser to reload the page in in current top-level browsing
 * context.
 *
 * @param {number} command_id
 *     ID of the currently handled message between the driver and
 *     listener.
 * @param {number} pageTimeout
 *     Timeout in milliseconds the method has to wait for the page being
 *     finished loading.
 */
function refresh(msg) {
  let {command_id, pageTimeout} = msg.json;

  try {
    // We need to move to the top frame before navigating
    sendSyncMessage("Marionette:switchedToFrame", {frameValue: null});
    curContainer.frame = content;

    loadListener.navigate(() => {
      curContainer.frame.location.reload(true);
    }, command_id, pageTimeout);

  } catch (e) {
    sendError(e, command_id);
  }
}

/**
 * Get source of the current browsing context's DOM.
 */
function getPageSource() {
  return curContainer.frame.document.documentElement.outerHTML;
}

/**
 * Find an element in the current browsing context's document using the
 * given search strategy.
 */
function* findElementContent(strategy, selector, opts = {}) {
  if (!SUPPORTED_STRATEGIES.has(strategy)) {
    throw new InvalidSelectorError("Strategy not supported: " + strategy);
  }

  opts.all = false;
  if (opts.startNode) {
    opts.startNode = seenEls.get(opts.startNode, curContainer);
  }

  let el = yield element.find(curContainer, strategy, selector, opts);
  let elRef = seenEls.add(el);
  let webEl = element.makeWebElement(elRef);
  return webEl;
}

/**
 * Find elements in the current browsing context's document using the
 * given search strategy.
 */
function* findElementsContent(strategy, selector, opts = {}) {
  if (!SUPPORTED_STRATEGIES.has(strategy)) {
    throw new InvalidSelectorError("Strategy not supported: " + strategy);
  }

  opts.all = true;
  if (opts.startNode) {
    opts.startNode = seenEls.get(opts.startNode, curContainer);
  }

  let els = yield element.find(curContainer, strategy, selector, opts);
  let elRefs = seenEls.addAll(els);
  let webEls = elRefs.map(element.makeWebElement);
  return webEls;
}

/** Find and return the active element on the page. */
function getActiveElement() {
  let el = curContainer.frame.document.activeElement;
  return evaluate.toJSON(el, seenEls);
}

/**
 * Send click event to element.
 *
 * @param {number} command_id
 *     ID of the currently handled message between the driver and
 *     listener.
 * @param {WebElement} id
 *     Reference to the web element to click.
 * @param {number} pageTimeout
 *     Timeout in milliseconds the method has to wait for the page being
 *     finished loading.
 */
function clickElement(msg) {
  let {command_id, id, pageTimeout} = msg.json;

  try {
    let loadEventExpected = true;

    let target = getElementAttribute(id, "target");

    if (target === "_blank") {
      loadEventExpected = false;
    }

    loadListener.navigate(() => {
      return interaction.clickElement(
        seenEls.get(id, curContainer),
        capabilities.get("moz:accessibilityChecks"),
        capabilities.get("specificationLevel") >= 1
      );
    }, command_id, pageTimeout, loadEventExpected, true);

  } catch (e) {
    sendError(e, command_id);
  }
}

function getElementAttribute(id, name) {
  let el = seenEls.get(id, curContainer);
  if (element.isBooleanAttribute(el, name)) {
    if (el.hasAttribute(name)) {
      return "true";
    }
    return null;
  }
  return el.getAttribute(name);
}

function getElementProperty(id, name) {
  let el = seenEls.get(id, curContainer);
  return typeof el[name] != "undefined" ? el[name] : null;
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
  let el = seenEls.get(id, curContainer);
  return atom.getElementText(el, curContainer.frame);
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
  let el = seenEls.get(id, curContainer);
  return el.tagName.toLowerCase();
}

/**
 * Determine the element displayedness of the given web element.
 *
 * Also performs additional accessibility checks if enabled by session
 * capability.
 */
function isElementDisplayed(id) {
  let el = seenEls.get(id, curContainer);
  return interaction.isElementDisplayed(
      el, capabilities.get("moz:accessibilityChecks"));
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
  let el = seenEls.get(id, curContainer);
  let st = curContainer.frame.document.defaultView.getComputedStyle(el);
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
  let el = seenEls.get(id, curContainer);
  let clientRect = el.getBoundingClientRect();
  return {
    x: clientRect.x + curContainer.frame.pageXOffset,
    y: clientRect.y + curContainer.frame.pageYOffset,
    width: clientRect.width,
    height: clientRect.height,
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
  let el = seenEls.get(id, curContainer);
  return interaction.isElementEnabled(
      el, capabilities.get("moz:accessibilityChecks"));
}

/**
 * Determines if the referenced element is selected or not.
 *
 * This operation only makes sense on input elements of the Checkbox-
 * and Radio Button states, or option elements.
 */
function isElementSelected(id) {
  let el = seenEls.get(id, curContainer);
  return interaction.isElementSelected(
      el, capabilities.get("moz:accessibilityChecks"));
}

function* sendKeysToElement(id, val) {
  let el = seenEls.get(id, curContainer);
  if (el.type == "file") {
    yield interaction.uploadFile(el, val);
  } else if ((el.type == "date" || el.type == "time") &&
      Preferences.get("dom.forms.datetime")) {
    yield interaction.setFormControlValue(el, val);
  } else {
    yield interaction.sendKeysToElement(
        el, val, false, capabilities.get("moz:accessibilityChecks"));
  }
}

/**
 * Clear the text of an element.
 */
function clearElement(id) {
  try {
    let el = seenEls.get(id, curContainer);
    if (el.type == "file") {
      el.value = null;
    } else {
      atom.clearElement(el, curContainer.frame);
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
 *
 * @param {WebElement} id
 *     Reference to web element.
 */
function switchToShadowRoot(id) {
  if (!id) {
    // If no host element is passed, attempt to find a parent shadow
    // root or, if none found, unset the current shadow root
    if (curContainer.shadowRoot) {
      let parent;
      try {
        parent = curContainer.shadowRoot.host;
      } catch (e) {
        // There is a chance that host element is dead and we are trying to
        // access a dead object.
        curContainer.shadowRoot = null;
        return;
      }
      while (parent && !(parent instanceof curContainer.frame.ShadowRoot)) {
        parent = parent.parentNode;
      }
      curContainer.shadowRoot = parent;
    }
    return;
  }

  let foundShadowRoot;
  let hostEl = seenEls.get(id, curContainer);
  foundShadowRoot = hostEl.shadowRoot;
  if (!foundShadowRoot) {
    throw new NoSuchElementError("Unable to locate shadow root: " + id);
  }
  curContainer.shadowRoot = foundShadowRoot;
}

/**
 * Switch to the parent frame of the current frame. If the frame is the
 * top most is the current frame then no action will happen.
 */
function switchToParentFrame(msg) {
  curContainer.frame = curContainer.frame.parent;
  let parentElement = seenEls.add(curContainer.frame);

  sendSyncMessage(
      "Marionette:switchedToFrame", {frameValue: parentElement});

  sendOk(msg.json.command_id);
}

/**
 * Switch to frame given either the server-assigned element id,
 * its index in window.frames, or the iframe's name or id.
 */
function switchToFrame(msg) {
  let command_id = msg.json.command_id;
  let foundFrame = null;
  let frames = [];
  let parWindow = null;

  // Check of the curContainer.frame reference is dead
  try {
    frames = curContainer.frame.frames;
    // Until Bug 761935 lands, we won't have multiple nested OOP
    // iframes. We will only have one.  parWindow will refer to the iframe
    // above the nested OOP frame.
    parWindow = curContainer.frame.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
  } catch (e) {
    // We probably have a dead compartment so accessing it is going to
    // make Firefox very upset. Let's now try redirect everything to the
    // top frame even if the user has given us a frame since search doesnt
    // look up.
    msg.json.id = null;
    msg.json.element = null;
  }

  if ((msg.json.id === null || msg.json.id === undefined) &&
      (msg.json.element == null)) {
    // returning to root frame
    sendSyncMessage("Marionette:switchedToFrame", {frameValue: null});

    curContainer.frame = content;
    if (msg.json.focus == true) {
      curContainer.frame.focus();
    }

    sendOk(command_id);
    return;
  }

  let id = msg.json.element;
  if (seenEls.has(id)) {
    let wantedFrame;
    try {
      wantedFrame = seenEls.get(id, curContainer);
    } catch (e) {
      sendError(e, command_id);
    }

    if (frames.length > 0) {
      for (let i = 0; i < frames.length; i++) {
        // use XPCNativeWrapper to compare elements; see bug 834266
        let frameEl = frames[i].frameElement;
        let wrappedItem = new XPCNativeWrapper(frameEl);
        let wrappedWanted = new XPCNativeWrapper(wantedFrame);
        if (wrappedItem == wrappedWanted) {
          curContainer.frame = frameEl;
          foundFrame = i;
        }
      }
    }

    if (foundFrame === null) {
      // Either the frame has been removed or we have a OOP frame
      // so lets just get all the iframes and do a quick loop before
      // throwing in the towel
      const doc = curContainer.frame.document;
      let iframes = doc.getElementsByTagName("iframe");
      for (var i = 0; i < iframes.length; i++) {
        let frameEl = iframes[i];
        let wrappedEl = new XPCNativeWrapper(frameEl);
        let wrappedWanted = new XPCNativeWrapper(wantedFrame);
        if (wrappedEl == wrappedWanted) {
          curContainer.frame = iframes[i];
          foundFrame = i;
        }
      }
    }
  }

  if (foundFrame === null) {
    if (typeof(msg.json.id) === "number") {
      try {
        foundFrame = frames[msg.json.id].frameElement;
        if (foundFrame !== null) {
          curContainer.frame = foundFrame;
          foundFrame = seenEls.add(curContainer.frame);
        } else {
          // If foundFrame is null at this point then we have the top
          // level browsing context so should treat it accordingly.
          sendSyncMessage("Marionette:switchedToFrame", {frameValue: null});
          curContainer.frame = content;

          if (msg.json.focus == true) {
            curContainer.frame.focus();
          }

          sendOk(command_id);
          return;
        }
      } catch (e) {
        // Since window.frames does not return OOP frames it will throw
        // and we land up here. Let's not give up and check if there are
        // iframes and switch to the indexed frame there
        let doc = curContainer.frame.document;
        let iframes = doc.getElementsByTagName("iframe");
        if (msg.json.id >= 0 && msg.json.id < iframes.length) {
          curContainer.frame = iframes[msg.json.id];
          foundFrame = msg.json.id;
        }
      }
    }
  }

  if (foundFrame === null) {
    let failedFrame = msg.json.id || msg.json.element;
    let err = new NoSuchFrameError(`Unable to locate frame: ${failedFrame}`);
    sendError(err, command_id);
    return;
  }

  // send a synchronous message to let the server update the currently active
  // frame element (for getActiveFrame)
  let frameValue = evaluate.toJSON(
      curContainer.frame.wrappedJSObject, seenEls)[element.Key];
  sendSyncMessage("Marionette:switchedToFrame", {"frameValue": frameValue});

  if (curContainer.frame.contentWindow === null) {
    // The frame we want to switch to is a remote/OOP frame;
    // notify our parent to handle the switch
    curContainer.frame = content;
    let rv = {win: parWindow, frame: foundFrame};
    sendResponse(rv, command_id);

  } else {
    curContainer.frame = curContainer.frame.contentWindow;

    if (msg.json.focus) {
      curContainer.frame.focus();
    }

    sendOk(command_id);
  }
}

/**
 * Perform a screen capture in content context.
 *
 * Accepted values for |opts|:
 *
 *     @param {UUID=} id
 *         Optional web element reference of an element to take a screenshot
 *         of.
 *     @param {boolean=} full
 *         True to take a screenshot of the entire document element.  Is not
 *         considered if {@code id} is not defined.  Defaults to true.
 *     @param {Array.<UUID>=} highlights
 *         Draw a border around the elements found by their web element
 *         references.
 *     @param {boolean=} scroll
 *         When |id| is given, scroll it into view before taking the
 *         screenshot.  Defaults to true.
 *
 * @param {capture.Format} format
 *     Format to return the screenshot in.
 * @param {Object.<string, ?>} opts
 *     Options.
 *
 * @return {string}
 *     Base64 encoded string or a SHA-256 hash of the screenshot.
 */
function takeScreenshot(format, opts = {}) {
  let id = opts.id;
  let full = !!opts.full;
  let highlights = opts.highlights || [];
  let scroll = !!opts.scroll;

  let highlightEls = highlights.map(ref => seenEls.get(ref, curContainer));

  let canvas;

  // viewport
  if (!id && !full) {
    canvas = capture.viewport(curContainer.frame, highlightEls);

  // element or full document element
  } else {
    let el;
    if (id) {
      el = seenEls.get(id, curContainer);
      if (scroll) {
        element.scrollIntoView(el);
      }
    } else {
      el = curContainer.frame.document.documentElement;
    }

    canvas = capture.element(el, highlightEls);
  }

  switch (format) {
    case capture.Format.Base64:
      return capture.toBase64(canvas);

    case capture.Format.Hash:
      return capture.toHash(canvas);

    default:
      throw new TypeError("Unknown screenshot format: " + format);
  }
}

function flushRendering() {
  let content = curContainer.frame;
  let anyPendingPaintsGeneratedInDescendants = false;

  let windowUtils = content.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIDOMWindowUtils);

  function flushWindow(win) {
    let utils = win.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils);
    let afterPaintWasPending = utils.isMozAfterPaintPending;

    let root = win.document.documentElement;
    if (root) {
      try {
        // Flush pending restyles and reflows for this window
        root.getBoundingClientRect();
      } catch (e) {
        logger.warning(`flushWindow failed: ${e}`);
      }
    }

    if (!afterPaintWasPending && utils.isMozAfterPaintPending) {
      anyPendingPaintsGeneratedInDescendants = true;
    }

    for (let i = 0; i < win.frames.length; ++i) {
      flushWindow(win.frames[i]);
    }
  }
  flushWindow(content);

  if (anyPendingPaintsGeneratedInDescendants &&
      !windowUtils.isMozAfterPaintPending) {
    logger.error("Internal error: descendant frame generated a MozAfterPaint event, but the root document doesn't have one!");
  }

  logger.debug(`flushRendering ${windowUtils.isMozAfterPaintPending}`);
  return windowUtils.isMozAfterPaintPending;
}

function* reftestWait(url, remote) {
  let win = curContainer.frame;
  let document = curContainer.frame.document;

  let windowUtils = content.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils);


  let reftestWait = false;

  if (document.location.href !== url || document.readyState != "complete") {
    logger.debug(`Waiting for page load of ${url}`);
    yield new Promise(resolve => {
      let maybeResolve = (event) => {
        if (event.target === curContainer.frame.document &&
            event.target.location.href === url) {
          win = curContainer.frame;
          document = curContainer.frame.document;
          reftestWait = document.documentElement.classList.contains("reftest-wait");
          removeEventListener("load", maybeResolve, {once: true});
          win.setTimeout(resolve, 0);
        }
      };
      addEventListener("load", maybeResolve, true);
    });
  } else {
    // Ensure that the event loop has spun at least once since load,
    // so that setTimeout(fn, 0) in the load event has run
    reftestWait = document.documentElement.classList.contains("reftest-wait");
    yield new Promise(resolve => win.setTimeout(resolve, 0));
  }

  let root = document.documentElement;
  if (reftestWait) {
    // Check again in case reftest-wait was removed since the load event
    if (root.classList.contains("reftest-wait")) {
      logger.debug("Waiting for reftest-wait removal");
      yield new Promise(resolve => {
        let observer = new win.MutationObserver(() => {
          if (!root.classList.contains("reftest-wait")) {
            observer.disconnect();
            logger.debug("reftest-wait removed");
            win.setTimeout(resolve, 0);
          }
        });
        observer.observe(root, {attributes: true});
      });
    }

    logger.debug("Waiting for rendering");

    yield new Promise(resolve => {
      let maybeResolve = () => {
        if (flushRendering()) {
          win.addEventListener("MozAfterPaint", maybeResolve, {once: true});
        } else {
          win.setTimeout(resolve, 0);
        }
      };
      maybeResolve();
    });
  } else {
    flushRendering();
  }

  if (remote) {
    windowUtils.updateLayerTree();
  }
}

// Call register self when we get loaded
registerSelf();
