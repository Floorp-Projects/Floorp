/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */
/* global XPCNativeWrapper */
/* eslint-disable no-restricted-globals */

"use strict";

const winUtil = content.windowUtils;

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  OS: "resource://gre/modules/osfile.jsm",

  accessibility: "chrome://marionette/content/accessibility.js",
  action: "chrome://marionette/content/action.js",
  atom: "chrome://marionette/content/atom.js",
  ContentEventObserverService: "chrome://marionette/content/dom.js",
  element: "chrome://marionette/content/element.js",
  error: "chrome://marionette/content/error.js",
  evaluate: "chrome://marionette/content/evaluate.js",
  event: "chrome://marionette/content/event.js",
  interaction: "chrome://marionette/content/interaction.js",
  legacyaction: "chrome://marionette/content/legacyaction.js",
  Log: "chrome://marionette/content/log.js",
  pprint: "chrome://marionette/content/format.js",
  proxy: "chrome://marionette/content/proxy.js",
  sandbox: "chrome://marionette/content/evaluate.js",
  Sandboxes: "chrome://marionette/content/evaluate.js",
  truncate: "chrome://marionette/content/format.js",
  WebElement: "chrome://marionette/content/element.js",
});

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.getWithPrefix(contentId));
XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

const contentFrameMessageManager = this;
const contentId = content.docShell.browsingContext.id;

const curContainer = {
  _frame: null,
  _parentFrame: null,
  shadowRoot: null,

  get frame() {
    return this._frame;
  },

  set frame(frame) {
    this._frame = frame;
    this._parentFrame = frame.parent;
    this.id = frame.browsingContext.id;
    this.shadowRoot = null;
  },

  get parentFrame() {
    return this._parentFrame;
  },
};

// Listen for click event to indicate one click has happened, so actions
// code can send dblclick event
addEventListener("click", event.DoubleClickTracker.setClick);
addEventListener("dblclick", event.DoubleClickTracker.resetClick);
addEventListener("unload", event.DoubleClickTracker.resetClick, true);

const seenEls = new element.Store();

let legacyactions = new legacyaction.Chain();

// last touch for each fingerId
let multiLast = {};

// sandbox storage and name of the current sandbox
const sandboxes = new Sandboxes(() => curContainer.frame);

const eventObservers = new ContentEventObserverService(
  content,
  sendAsyncMessage.bind(this)
);

// Eventually we will not have a closure for every single command,
// but use a generic dispatch for all listener commands.
//
// Worth nothing that this shares many characteristics with
// server.TCPConnection#execute.  Perhaps this could be generalised
// at the point.
function dispatch(fn) {
  if (typeof fn != "function") {
    throw new TypeError("Provided dispatch handler is not a function");
  }

  return msg => {
    const id = msg.json.commandID;

    let req = new Promise(resolve => {
      const args = evaluate.fromJSON(msg.json, seenEls, curContainer.frame);

      let rv;
      if (typeof args == "undefined" || args instanceof Array) {
        rv = fn.apply(null, args);
      } else {
        rv = fn(args);
      }
      resolve(rv);
    });

    req
      .then(
        rv => sendResponse(rv, id),
        err => sendError(err, id)
      )
      .catch(err => sendError(err, id));
  };
}

let clickElementFn = dispatch(clickElement);
let getActiveElementFn = dispatch(getActiveElement);
let getBrowsingContextIdFn = dispatch(getBrowsingContextId);
let getCurrentUrlFn = dispatch(getCurrentUrl);
let getElementAttributeFn = dispatch(getElementAttribute);
let getElementPropertyFn = dispatch(getElementProperty);
let getElementTextFn = dispatch(getElementText);
let getElementTagNameFn = dispatch(getElementTagName);
let getElementRectFn = dispatch(getElementRect);
let getPageSourceFn = dispatch(getPageSource);
let getScreenshotRectFn = dispatch(getScreenshotRect);
let isElementEnabledFn = dispatch(isElementEnabled);
let findElementContentFn = dispatch(findElementContent);
let findElementsContentFn = dispatch(findElementsContent);
let isElementSelectedFn = dispatch(isElementSelected);
let clearElementFn = dispatch(clearElement);
let isElementDisplayedFn = dispatch(isElementDisplayed);
let getElementValueOfCssPropertyFn = dispatch(getElementValueOfCssProperty);
let switchToShadowRootFn = dispatch(switchToShadowRoot);
let singleTapFn = dispatch(singleTap);
let performActionsFn = dispatch(performActions);
let releaseActionsFn = dispatch(releaseActions);
let actionChainFn = dispatch(actionChain);
let multiActionFn = dispatch(multiAction);
let executeFn = dispatch(execute);
let executeInSandboxFn = dispatch(executeInSandbox);
let sendKeysToElementFn = dispatch(sendKeysToElement);
let reftestWaitFn = dispatch(reftestWait);

function startListeners() {
  eventDispatcher.enable();

  addMessageListener("Marionette:actionChain", actionChainFn);
  addMessageListener("Marionette:clearElement", clearElementFn);
  addMessageListener("Marionette:clickElement", clickElementFn);
  addMessageListener("Marionette:Deregister", deregister);
  addMessageListener("Marionette:DOM:AddEventListener", domAddEventListener);
  addMessageListener(
    "Marionette:DOM:RemoveEventListener",
    domRemoveEventListener
  );
  addMessageListener("Marionette:execute", executeFn);
  addMessageListener("Marionette:executeInSandbox", executeInSandboxFn);
  addMessageListener("Marionette:findElementContent", findElementContentFn);
  addMessageListener("Marionette:findElementsContent", findElementsContentFn);
  addMessageListener("Marionette:getActiveElement", getActiveElementFn);
  addMessageListener("Marionette:getBrowsingContextId", getBrowsingContextIdFn);
  addMessageListener("Marionette:getCurrentUrl", getCurrentUrlFn);
  addMessageListener("Marionette:getElementAttribute", getElementAttributeFn);
  addMessageListener("Marionette:getElementProperty", getElementPropertyFn);
  addMessageListener("Marionette:getElementRect", getElementRectFn);
  addMessageListener("Marionette:getElementTagName", getElementTagNameFn);
  addMessageListener("Marionette:getElementText", getElementTextFn);
  addMessageListener(
    "Marionette:getElementValueOfCssProperty",
    getElementValueOfCssPropertyFn
  );
  addMessageListener("Marionette:getPageSource", getPageSourceFn);
  addMessageListener("Marionette:getScreenshotRect", getScreenshotRectFn);
  addMessageListener("Marionette:isElementDisplayed", isElementDisplayedFn);
  addMessageListener("Marionette:isElementEnabled", isElementEnabledFn);
  addMessageListener("Marionette:isElementSelected", isElementSelectedFn);
  addMessageListener("Marionette:multiAction", multiActionFn);
  addMessageListener("Marionette:performActions", performActionsFn);
  addMessageListener("Marionette:reftestWait", reftestWaitFn);
  addMessageListener("Marionette:releaseActions", releaseActionsFn);
  addMessageListener("Marionette:sendKeysToElement", sendKeysToElementFn);
  addMessageListener("Marionette:Session:Delete", deleteSession);
  addMessageListener("Marionette:singleTap", singleTapFn);
  addMessageListener("Marionette:switchToFrame", switchToFrame);
  addMessageListener("Marionette:switchToParentFrame", switchToParentFrame);
  addMessageListener("Marionette:switchToShadowRoot", switchToShadowRootFn);
}

function deregister() {
  eventDispatcher.disable();

  removeMessageListener("Marionette:actionChain", actionChainFn);
  removeMessageListener("Marionette:clearElement", clearElementFn);
  removeMessageListener("Marionette:clickElement", clickElementFn);
  removeMessageListener("Marionette:Deregister", deregister);
  removeMessageListener("Marionette:execute", executeFn);
  removeMessageListener("Marionette:executeInSandbox", executeInSandboxFn);
  removeMessageListener("Marionette:findElementContent", findElementContentFn);
  removeMessageListener(
    "Marionette:findElementsContent",
    findElementsContentFn
  );
  removeMessageListener("Marionette:getActiveElement", getActiveElementFn);
  removeMessageListener(
    "Marionette:getBrowsingContextId",
    getBrowsingContextIdFn
  );
  removeMessageListener("Marionette:getCurrentUrl", getCurrentUrlFn);
  removeMessageListener(
    "Marionette:getElementAttribute",
    getElementAttributeFn
  );
  removeMessageListener("Marionette:getElementProperty", getElementPropertyFn);
  removeMessageListener("Marionette:getElementRect", getElementRectFn);
  removeMessageListener("Marionette:getElementTagName", getElementTagNameFn);
  removeMessageListener("Marionette:getElementText", getElementTextFn);
  removeMessageListener(
    "Marionette:getElementValueOfCssProperty",
    getElementValueOfCssPropertyFn
  );
  removeMessageListener("Marionette:getPageSource", getPageSourceFn);
  removeMessageListener("Marionette:getScreenshotRect", getScreenshotRectFn);
  removeMessageListener("Marionette:isElementDisplayed", isElementDisplayedFn);
  removeMessageListener("Marionette:isElementEnabled", isElementEnabledFn);
  removeMessageListener("Marionette:isElementSelected", isElementSelectedFn);
  removeMessageListener("Marionette:multiAction", multiActionFn);
  removeMessageListener("Marionette:performActions", performActionsFn);
  removeMessageListener("Marionette:releaseActions", releaseActionsFn);
  removeMessageListener("Marionette:sendKeysToElement", sendKeysToElementFn);
  removeMessageListener("Marionette:Session:Delete", deleteSession);
  removeMessageListener("Marionette:singleTap", singleTapFn);
  removeMessageListener("Marionette:switchToFrame", switchToFrame);
  removeMessageListener("Marionette:switchToParentFrame", switchToParentFrame);
  removeMessageListener("Marionette:switchToShadowRoot", switchToShadowRootFn);
}

function deleteSession() {
  seenEls.clear();

  // reset container frame to the top-most frame
  curContainer.frame = content;
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
 * @param {*} [Object] data
 *     JSON serialisable object to accompany the message.  Defaults to
 *     an empty dictionary.
 */
let sendToServer = (uuid, data = undefined) => {
  let channel = new proxy.AsyncMessageChannel(sendAsyncMessage.bind(this));
  channel.reply(uuid, data);
};

/**
 * Send asynchronous reply with value to chrome.
 *
 * @param {Object} obj
 *     JSON serialisable object of arbitrary type and complexity.
 * @param {UUID} uuid
 *     Unique identifier of the request.
 */
function sendResponse(obj, uuid) {
  let payload = evaluate.toJSON(obj, seenEls);
  sendToServer(uuid, payload);
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

async function execute(script, args, opts) {
  let sb = sandbox.createMutable(curContainer.frame);
  return evaluate.sandbox(sb, script, args, opts);
}

async function executeInSandbox(script, args, opts) {
  let sb = sandboxes.get(opts.sandboxName, opts.newSandbox);
  return evaluate.sandbox(sb, script, args, opts);
}

function emitTouchEvent(type, touch) {
  logger.info(
    `Emitting Touch event of type ${type} ` +
      `to element with id: ${touch.target.id} ` +
      `and tag name: ${touch.target.tagName} ` +
      `at coordinates (${touch.clientX}), ` +
      `${touch.clientY}) relative to the viewport`
  );

  const win = curContainer.frame;
  if (win.docShell.asyncPanZoomEnabled && legacyactions.scrolling) {
    let ev = {
      index: 0,
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

  // we get here if we're not in asyncPacZoomEnabled land, or if we're
  // the main process
  win.windowUtils.sendTouchEvent(
    type,
    [touch.identifier],
    [touch.clientX],
    [touch.clientY],
    [touch.radiusX],
    [touch.radiusY],
    [touch.rotationAngle],
    [touch.force],
    0
  );
}

/**
 * Function that perform a single tap
 */
async function singleTap(el, corx, cory, capabilities) {
  // after this block, the element will be scrolled into view
  let visible = element.isVisible(el, corx, cory);
  if (!visible) {
    throw new error.ElementNotInteractableError(
      "Element is not currently visible and may not be manipulated"
    );
  }

  let a11y = accessibility.get(capabilities["moz:accessibilityChecks"]);
  let acc = await a11y.getAccessible(el, true);
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
}

/**
 * Function to create a touch based on the element
 * corx and cory are relative to the viewport, id is the touchId
 */
function createATouch(el, corx, cory, touchId) {
  let doc = el.ownerDocument;
  let win = doc.defaultView;
  let [
    clientX,
    clientY,
    pageX,
    pageY,
    screenX,
    screenY,
  ] = legacyactions.getCoordinateInfo(el, corx, cory);
  let atouch = doc.createTouch(
    win,
    el,
    touchId,
    pageX,
    pageY,
    screenX,
    screenY,
    clientX,
    clientY
  );
  return atouch;
}

/**
 * Perform a series of grouped actions at the specified points in time.
 *
 * @param {Object} msg
 *     Object with an |actions| attribute that is an Array of objects
 *     each of which represents an action sequence.
 * @param {Object} capabilities
 *     Object with a list of WebDriver session capabilities.
 */
async function performActions(msg, capabilities) {
  let chain = action.Chain.fromJSON(msg.actions);
  await action.dispatch(
    chain,
    curContainer.frame,
    !capabilities["moz:useNonSpecCompliantPointerOrigin"]
  );
}

/**
 * The release actions command is used to release all the keys and pointer
 * buttons that are currently depressed. This causes events to be fired
 * as if the state was released by an explicit series of actions. It also
 * clears all the internal state of the virtual devices.
 */
async function releaseActions() {
  await action.dispatchTickActions(
    action.inputsToCancel.reverse(),
    0,
    curContainer.frame
  );
  action.inputsToCancel.length = 0;
  action.inputStateMap.clear();

  event.DoubleClickTracker.resetClick();
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
    touchProvider
  );
}

function emitMultiEvents(type, touch, touches) {
  let target = touch.target;
  let doc = target.ownerDocument;
  let win = doc.defaultView;
  // touches that are in the same document
  let documentTouches = doc.createTouchList(
    touches.filter(function(t) {
      return t.target.ownerDocument === doc && type != "touchcancel";
    })
  );
  // touches on the same target
  let targetTouches = doc.createTouchList(
    touches.filter(function(t) {
      return (
        t.target === target && (type != "touchcancel" || type != "touchend")
      );
    })
  );
  // Create changed touches
  let changedTouches = doc.createTouchList(touch);
  // Create the event object
  let event = doc.createEvent("TouchEvent");
  event.initTouchEvent(
    type,
    true,
    true,
    win,
    0,
    false,
    false,
    false,
    false,
    documentTouches,
    targetTouches,
    changedTouches
  );
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
        el = seenEls.get(pack[2], curContainer.frame);
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
        el = seenEls.get(pack[2], curContainer.frame);
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
          clientY
        );
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
    timer.initWithCallback(
      function() {
        setDispatch(batches, touches, batchIndex);
      },
      maxTime,
      Ci.nsITimer.TYPE_ONE_SHOT
    );
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
  let commandArray = evaluate.fromJSON(args, seenEls, curContainer.frame);
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
 * Get source of the current browsing context's DOM.
 */
function getPageSource() {
  return curContainer.frame.document.documentElement.outerHTML;
}

/**
 * Find an element in the current browsing context's document using the
 * given search strategy.
 */
async function findElementContent(strategy, selector, opts = {}) {
  opts.all = false;
  let el = await element.find(curContainer, strategy, selector, opts);
  return seenEls.add(el);
}

/**
 * Find elements in the current browsing context's document using the
 * given search strategy.
 */
async function findElementsContent(strategy, selector, opts = {}) {
  opts.all = true;
  let els = await element.find(curContainer, strategy, selector, opts);
  let webEls = seenEls.addAll(els);
  return webEls;
}

/**
 * Return the active element in the document.
 *
 * @return {WebElement}
 *     Active element of the current browsing context's document
 *     element, if the document element is non-null.
 *
 * @throws {NoSuchElementError}
 *     If the document does not have an active element, i.e. if
 *     its document element has been deleted.
 */
function getActiveElement() {
  let el = curContainer.frame.document.activeElement;
  if (!el) {
    throw new error.NoSuchElementError();
  }
  return evaluate.toJSON(el, seenEls);
}

/**
 * Return the current browsing context id.
 *
 * @param {boolean=} topContext
 *     If set to true use the window's top-level browsing context,
 *     otherwise the one from the currently selected frame. Defaults to false.
 *
 * @return {number}
 *     Id of the browsing context.
 */
function getBrowsingContextId(topContext = false) {
  const bc = curContainer.frame.docShell.browsingContext;

  return topContext ? bc.top.id : bc.id;
}

/**
 * Return the current visible URL.
 *
 * @return {string}
 *     Current visible URL.
 */
function getCurrentUrl() {
  return content.location.href;
}

/**
 * Send click event to element.
 *
 * @param {WebElement} el
 *     Element to click.
 * @param {Object} capabilities
 *     Object with a list of WebDriver session capabilities.
 */
function clickElement(el, capabilities) {
  return interaction.clickElement(
    el,
    capabilities["moz:accessibilityChecks"],
    capabilities["moz:webdriverClick"]
  );
}

function getElementAttribute(el, name) {
  if (element.isBooleanAttribute(el, name)) {
    if (el.hasAttribute(name)) {
      return "true";
    }
    return null;
  }
  return el.getAttribute(name);
}

function getElementProperty(el, name) {
  return typeof el[name] != "undefined" ? el[name] : null;
}

/**
 * Get the text of this element.  This includes text from child
 * elements.
 */
function getElementText(el) {
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
function getElementTagName(el) {
  return el.tagName.toLowerCase();
}

/**
 * Determine the element displayedness of the given web element.
 *
 * Also performs additional accessibility checks if enabled by session
 * capability.
 */
function isElementDisplayed(el, capabilities) {
  return interaction.isElementDisplayed(
    el,
    capabilities["moz:accessibilityChecks"]
  );
}

/**
 * Retrieves the computed value of the given CSS property of the given
 * web element.
 */
function getElementValueOfCssProperty(el, prop) {
  let st = curContainer.frame.document.defaultView.getComputedStyle(el);
  return st.getPropertyValue(prop);
}

/**
 * Get the position and dimensions of the element.
 *
 * @return {Object.<string, number>}
 *     The x, y, width, and height properties of the element.
 */
function getElementRect(el) {
  let clientRect = el.getBoundingClientRect();
  return {
    x: clientRect.x + curContainer.frame.pageXOffset,
    y: clientRect.y + curContainer.frame.pageYOffset,
    width: clientRect.width,
    height: clientRect.height,
  };
}

function isElementEnabled(el, capabilities) {
  return interaction.isElementEnabled(
    el,
    capabilities["moz:accessibilityChecks"]
  );
}

/**
 * Determines if the referenced element is selected or not.
 *
 * This operation only makes sense on input elements of the Checkbox-
 * and Radio Button states, or option elements.
 */
function isElementSelected(el, capabilities) {
  return interaction.isElementSelected(
    el,
    capabilities["moz:accessibilityChecks"]
  );
}

async function sendKeysToElement(el, val, capabilities) {
  let opts = {
    strictFileInteractability: capabilities.strictFileInteractability,
    accessibilityChecks: capabilities["moz:accessibilityChecks"],
    webdriverClick: capabilities["moz:webdriverClick"],
  };
  await interaction.sendKeysToElement(el, val, opts);
}

/** Clear the text of an element. */
function clearElement(el) {
  interaction.clearElement(el);
}

/** Switch the current context to the specified host's Shadow DOM. */
function switchToShadowRoot(el) {
  if (!element.isElement(el)) {
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

  let foundShadowRoot = el.shadowRoot;
  if (!foundShadowRoot) {
    throw new error.NoSuchElementError(
      pprint`Unable to locate shadow root: ${el}`
    );
  }
  curContainer.shadowRoot = foundShadowRoot;
}

/**
 * Switch to the parent frame of the current frame. If the frame is the
 * top most is the current frame then no action will happen.
 */
function switchToParentFrame(msg) {
  curContainer.frame = curContainer.parentFrame;

  sendSyncMessage("Marionette:switchedToFrame", {
    browsingContextId: curContainer.id,
  });

  sendOk(msg.json.commandID);
}

/**
 * Switch to the specified frame.
 *
 * @param {boolean=} focus
 *     Focus the frame if set to true. Defaults to false.
 * @param {(string|Object)=} element
 *     A web element reference of the frame or its element id.
 * @param {number=} id
 *     The index of the frame to switch to.
 *     If both element and id are not defined, switch to top-level frame.
 */
function switchToFrame({ json }) {
  let { commandID, element, focus, id } = json;

  let foundFrame;
  let wantedFrame = null;

  // check if curContainer.frame reference is dead
  let frames = [];
  try {
    frames = curContainer.frame.frames;
  } catch (e) {
    // dead comparment, redirect to top frame
    id = null;
    element = null;
  }

  // switch to top-level frame
  if (id == null && !element) {
    curContainer.frame = content;
    sendSyncMessage("Marionette:switchedToFrame", {
      browsingContextId: curContainer.id,
    });

    if (focus) {
      curContainer.frame.focus();
    }

    sendOk(commandID);
    return;
  }

  let webEl;
  if (typeof element != "undefined") {
    webEl = WebElement.fromUUID(element, "content");
  }

  if (webEl) {
    if (!seenEls.has(webEl)) {
      let err = new error.NoSuchElementError(
        `Unable to locate element: ${webEl}`
      );
      sendError(err, commandID);
      return;
    }

    try {
      wantedFrame = seenEls.get(webEl, curContainer.frame);
    } catch (e) {
      sendError(e, commandID);
      return;
    }

    if (frames.length > 0) {
      // use XPCNativeWrapper to compare elements; see bug 834266
      let wrappedWanted = new XPCNativeWrapper(wantedFrame);
      foundFrame = Array.prototype.find.call(frames, frame => {
        return new XPCNativeWrapper(frame.frameElement) === wrappedWanted;
      });
    }

    if (!foundFrame) {
      // Either the frame has been removed or we have a OOP frame
      // so lets just get all the iframes and do a quick loop before
      // throwing in the towel
      let iframes = curContainer.frame.document.getElementsByTagName("iframe");
      let wrappedWanted = new XPCNativeWrapper(wantedFrame);
      foundFrame = Array.prototype.find.call(iframes, frame => {
        return new XPCNativeWrapper(frame) === wrappedWanted;
      });
    }
  }

  if (!foundFrame) {
    if (typeof id === "number") {
      try {
        let frameEl;
        if (id >= 0 && id < frames.length) {
          frameEl = frames[id].frameElement;
          if (frameEl !== null) {
            foundFrame = frameEl.contentWindow;
          } else {
            // If foundFrame is null at this point then we have the top
            // level browsing context so should treat it accordingly.
            curContainer.frame = content;
            sendSyncMessage("Marionette:switchedToFrame", {
              browsingContextId: curContainer.id,
            });

            if (focus) {
              curContainer.frame.focus();
            }

            sendOk(commandID);
            return;
          }
        }
      } catch (e) {
        // Since window.frames does not return OOP frames it will throw
        // and we land up here. Let's not give up and check if there are
        // iframes and switch to the indexed frame there
        let iframes = foundFrame.document.getElementsByTagName("iframe");
        if (id >= 0 && id < iframes.length) {
          foundFrame = iframes[id];
        }
      }
    }
  }

  if (!foundFrame) {
    let failedFrame = id || element;
    let err = new error.NoSuchFrameError(
      `Unable to locate frame: ${failedFrame}`
    );
    sendError(err, commandID);
    return;
  }

  curContainer.frame = foundFrame;

  sendSyncMessage("Marionette:switchedToFrame", {
    browsingContextId: curContainer.id,
  });

  if (focus) {
    curContainer.frame.focus();
  }

  sendOk(commandID);
}

/**
 * Returns the rect of the element to screenshot.
 *
 * Because the screen capture takes place in the parent process the dimensions
 * for the screenshot have to be determined in the appropriate child process.
 *
 * Also it takes care of scrolling an element into view if requested.
 *
 * @param {Object.<string, ?>} opts
 *     Options.
 *
 * Accepted values for |opts|:
 *
 * @param {WebElement} webEl
 *     Optional element to take a screenshot of.
 * @param {boolean=} full
 *     True to take a screenshot of the entire document element. Is only
 *     considered if <var>id</var> is not defined. Defaults to true.
 * @param {boolean=} scroll
 *     When <var>id</var> is given, scroll it into view before taking the
 *     screenshot.  Defaults to true.
 * @param {capture.Format} format
 *     Format to return the screenshot in.
 * @param {Object.<string, ?>} opts
 *     Options.
 *
 * @return {DOMRect}
 *     The area to take a snapshot from
 */
function getScreenshotRect({ el, full = true, scroll = true } = {}) {
  let win = el ? curContainer.frame : content;

  let rect;

  if (el) {
    if (scroll) {
      element.scrollIntoView(el);
    }
    rect = getElementRect(el);
  } else if (full) {
    const docEl = win.document.documentElement;
    rect = new DOMRect(0, 0, docEl.scrollWidth, docEl.scrollHeight);
  } else {
    // viewport
    rect = new DOMRect(
      win.pageXOffset,
      win.pageYOffset,
      win.innerWidth,
      win.innerHeight
    );
  }

  return rect;
}

function flushRendering() {
  let content = curContainer.frame;
  let anyPendingPaintsGeneratedInDescendants = false;

  let windowUtils = content.windowUtils;

  function flushWindow(win) {
    let utils = win.windowUtils;
    let afterPaintWasPending = utils.isMozAfterPaintPending;

    let root = win.document.documentElement;
    if (root) {
      try {
        // Flush pending restyles and reflows for this window (layout)
        root.getBoundingClientRect();
      } catch (e) {
        logger.error("flushWindow failed", e);
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

  if (
    anyPendingPaintsGeneratedInDescendants &&
    !windowUtils.isMozAfterPaintPending
  ) {
    logger.error(
      "Descendant frame generated a MozAfterPaint event, " +
        "but the root document doesn't have one!"
    );
  }
}

async function reftestWait(url, remote) {
  let win = curContainer.frame;
  let document = curContainer.frame.document;
  let reftestWait;

  if (document.location.href !== url || document.readyState != "complete") {
    reftestWait = await documentLoad(win, url);
    win = curContainer.frame;
    document = curContainer.frame.document;
  } else {
    reftestWait = document.documentElement.classList.contains("reftest-wait");
  }

  logger.debug("Waiting for event loop to spin");
  await new Promise(resolve => win.setTimeout(resolve, 0));

  await paintComplete(win, remote);

  let root = document.documentElement;
  if (reftestWait) {
    let event = new Event("TestRendered", { bubbles: true });
    root.dispatchEvent(event);
    logger.info("Emitted TestRendered event");
    await reftestWaitRemoved(win, root);
    await paintComplete(win, remote);
  }
  if (
    win.innerWidth < document.documentElement.scrollWidth ||
    win.innerHeight < document.documentElement.scrollHeight
  ) {
    logger.warn(
      `${url} overflows viewport (width: ${document.documentElement.scrollWidth}, height: ${document.documentElement.scrollHeight})`
    );
  }
}

function documentLoad(win, url) {
  logger.debug(truncate`Waiting for page load of ${url}`);
  return new Promise(resolve => {
    let maybeResolve = event => {
      if (
        event.target === curContainer.frame.document &&
        event.target.location.href === url
      ) {
        let reftestWait = win.document.documentElement.classList.contains(
          "reftest-wait"
        );
        removeEventListener("load", maybeResolve, { once: true });
        resolve(reftestWait);
      }
    };
    addEventListener("load", maybeResolve, true);
  });
}

function paintComplete(win, remote) {
  logger.debug("Waiting for rendering");
  let windowUtils = content.windowUtils;
  return new Promise(resolve => {
    let maybeResolve = () => {
      flushRendering();
      if (remote) {
        // Flush display (paint)
        logger.debug("Force update of layer tree");
        windowUtils.updateLayerTree();
      }

      if (windowUtils.isMozAfterPaintPending) {
        logger.debug("isMozAfterPaintPending: true");
        win.addEventListener("MozAfterPaint", maybeResolve, { once: true });
      } else {
        // resolve at the start of the next frame in case of leftover paints
        logger.debug("isMozAfterPaintPending: false");
        win.requestAnimationFrame(() => {
          win.requestAnimationFrame(resolve);
        });
      }
    };
    maybeResolve();
  });
}

function reftestWaitRemoved(win, root) {
  logger.debug("Waiting for reftest-wait removal");
  return new Promise(resolve => {
    let observer = new win.MutationObserver(() => {
      if (!root.classList.contains("reftest-wait")) {
        observer.disconnect();
        logger.debug("reftest-wait removed");
        win.setTimeout(resolve, 0);
      }
    });
    if (root.classList.contains("reftest-wait")) {
      observer.observe(root, { attributes: true });
    } else {
      win.setTimeout(resolve, 0);
    }
  });
}

function domAddEventListener(msg) {
  eventObservers.add(msg.json.type);
}

function domRemoveEventListener(msg) {
  eventObservers.remove(msg.json.type);
}

const eventDispatcher = {
  enabled: false,

  enable() {
    if (this.enabled) {
      return;
    }

    addEventListener("unload", this, false);

    addEventListener("beforeunload", this, true);
    addEventListener("pagehide", this, true);
    addEventListener("popstate", this, true);

    addEventListener("DOMContentLoaded", this, true);
    addEventListener("hashchange", this, true);
    addEventListener("pageshow", this, true);

    Services.obs.addObserver(this, "webnavigation-destroy");

    this.enabled = true;
  },

  disable() {
    if (!this.enabled) {
      return;
    }

    removeEventListener("unload", this, false);

    removeEventListener("beforeunload", this, true);
    removeEventListener("pagehide", this, true);
    removeEventListener("popstate", this, true);

    removeEventListener("DOMContentLoaded", this, true);
    removeEventListener("hashchange", this, true);
    removeEventListener("pageshow", this, true);

    // In case the observer was added before the frame script has been moved
    // to a different process, it will no longer be available. Exceptions can
    // be ignored.
    try {
      Services.obs.removeObserver(this, "webnavigation-destroy");
    } catch (e) {}

    this.enabled = false;
  },

  handleEvent(event) {
    const { target, type } = event;

    // An unload event indicates that the framescript died because of a process
    // change, or that the tab / window has been closed.
    if (type === "unload" && target === contentFrameMessageManager) {
      logger.trace(`Frame script unloaded`);
      sendAsyncMessage("Marionette:Unloaded", {
        browsingContext: content.docShell.browsingContext,
      });
      return;
    }

    // Only care about events from the currently selected browsing context,
    // whereby some of those do not bubble up to the window.
    if (![curContainer.frame, curContainer.frame.document].includes(target)) {
      return;
    }

    if (type === "pagehide") {
      // The content window has been replaced. Immediately register the page
      // load events again so that we don't miss possible load events
      addEventListener("DOMContentLoaded", this, true);
      addEventListener("pageshow", this, true);
    }

    sendAsyncMessage("Marionette:NavigationEvent", {
      browsingContext: content.docShell.browsingContext,
      documentURI: target.documentURI,
      readyState: target.readyState,
      type,
    });
  },

  observe(subject, topic) {
    subject.QueryInterface(Ci.nsIDocShell);

    const browsingContext = subject.browsingContext;
    const isFrame = browsingContext !== subject.browsingContext.top;

    // The currently selected iframe has been closed
    if (isFrame && browsingContext.id === curContainer.id) {
      logger.trace(`Frame with id ${browsingContext.id} got removed`);
      sendAsyncMessage("Marionette:FrameRemoved", {
        browsingContextId: browsingContext.id,
      });
    }
  },
};

/**
 * Called when listener is first started up.  The listener sends its
 * unique window ID and its current URI to the actor.  If the actor returns
 * an ID, we start the listeners. Otherwise, nothing happens.
 */
function registerSelf() {
  logger.trace("Frame script loaded");

  curContainer.frame = content;

  sandboxes.clear();
  legacyactions.mouseEventsOnly = false;
  action.inputStateMap = new Map();
  action.inputsToCancel = [];

  let reply = sendSyncMessage("Marionette:Register", {
    frameId: contentId,
  });

  if (reply.length == 0) {
    logger.error("No reply from Marionette:Register");
    return;
  }

  if (reply[0].frameId === contentId) {
    startListeners();
    sendAsyncMessage("Marionette:ListenersAttached", {
      frameId: contentId,
    });
  }
}

// Call register self when we get loaded
registerSelf();
