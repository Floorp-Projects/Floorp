/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable no-restricted-globals */

"use strict";

const EXPORTED_SYMBOLS = ["legacyaction"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  Preferences: "resource://gre/modules/Preferences.jsm",

  accessibility: "chrome://remote/content/marionette/accessibility.js",
  element: "chrome://remote/content/marionette/element.js",
  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
  evaluate: "chrome://remote/content/marionette/evaluate.js",
  event: "chrome://remote/content/marionette/event.js",
  Log: "chrome://remote/content/shared/Log.jsm",
  WebReference: "chrome://remote/content/marionette/element.js",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.MARIONETTE)
);

const CONTEXT_MENU_DELAY_PREF = "ui.click_hold_context_menus.delay";
const DEFAULT_CONTEXT_MENU_DELAY = 750; // ms

/** @namespace */
const legacyaction = {};
const action = legacyaction;

/**
 * Functionality for (single finger) action chains.
 */
action.Chain = function() {
  // for assigning unique ids to all touches
  this.nextTouchId = 1000;
  // keep track of active Touches
  this.touchIds = {};
  // last touch for each fingerId
  this.lastCoordinates = null;
  this.isTap = false;
  this.scrolling = false;
  // whether to send mouse event
  this.mouseEventsOnly = false;
  this.checkTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

  // determines if we create touch events
  this.inputSource = null;
};

/**
 * Create a touch based event.
 *
 * @param {Element} elem
 *        The Element on which the touch event should be created.
 * @param {Number} x
 *        x coordinate relative to the viewport.
 * @param {Number} y
 *        y coordinate relative to the viewport.
 * @param {Number} touchId
 *        Touch event id used by legacyactions.
 */
action.Chain.prototype.createATouch = function(elem, x, y, touchId) {
  const doc = elem.ownerDocument;
  const win = doc.defaultView;
  const [
    clientX,
    clientY,
    pageX,
    pageY,
    screenX,
    screenY,
  ] = this.getCoordinateInfo(elem, x, y);
  const atouch = doc.createTouch(
    win,
    elem,
    touchId,
    pageX,
    pageY,
    screenX,
    screenY,
    clientX,
    clientY
  );
  return atouch;
};

action.Chain.prototype.dispatchActions = function(
  args,
  touchId,
  container,
  seenEls
) {
  this.seenEls = seenEls;
  this.container = container;
  let commandArray = lazy.evaluate.fromJSON({
    obj: args,
    seenEls,
    win: container.frame,
  });

  if (touchId == null) {
    touchId = this.nextTSouchId++;
  }

  if (!container.frame.document.createTouch) {
    this.mouseEventsOnly = true;
  }

  let keyModifiers = {
    shiftKey: false,
    ctrlKey: false,
    altKey: false,
    metaKey: false,
  };

  return new Promise(resolve => {
    this.actions(commandArray, touchId, 0, keyModifiers, resolve);
  }).catch(this.resetValues.bind(this));
};

/**
 * This function emit mouse event.
 *
 * @param {Document} doc
 *     Current document.
 * @param {string} type
 *     Type of event to dispatch.
 * @param {number} clickCount
 *     Number of clicks, button notes the mouse button.
 * @param {number} elClientX
 *     X coordinate of the mouse relative to the viewport.
 * @param {number} elClientY
 *     Y coordinate of the mouse relative to the viewport.
 * @param {Object} modifiers
 *     An object of modifier keys present.
 */
action.Chain.prototype.emitMouseEvent = function(
  doc,
  type,
  elClientX,
  elClientY,
  button,
  clickCount,
  modifiers
) {
  lazy.logger.debug(
    `Emitting ${type} mouse event ` +
      `at coordinates (${elClientX}, ${elClientY}) ` +
      `relative to the viewport, ` +
      `button: ${button}, ` +
      `clickCount: ${clickCount}`
  );

  let win = doc.defaultView;
  let domUtils = win.windowUtils;

  let mods;
  if (typeof modifiers != "undefined") {
    mods = lazy.event.parseModifiers_(modifiers, win);
  } else {
    mods = 0;
  }

  domUtils.sendMouseEvent(
    type,
    elClientX,
    elClientY,
    button || 0,
    clickCount || 1,
    mods,
    false,
    0,
    this.inputSource
  );
};

action.Chain.prototype.emitTouchEvent = function(doc, type, touch) {
  lazy.logger.info(
    `Emitting Touch event of type ${type} ` +
      `to element with id: ${touch.target.id} ` +
      `and tag name: ${touch.target.tagName} ` +
      `at coordinates (${touch.clientX}), ` +
      `${touch.clientY}) relative to the viewport`
  );

  const win = doc.defaultView;
  if (win.docShell.asyncPanZoomEnabled && this.scrolling) {
    lazy.logger.debug(
      `Cannot emit touch event with asyncPanZoomEnabled and legacyactions.scrolling`
    );
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
};

/**
 * Reset any persisted values after a command completes.
 */
action.Chain.prototype.resetValues = function() {
  this.container = null;
  this.seenEls = null;
  this.mouseEventsOnly = false;
};

/**
 * Function that performs a single tap.
 */
action.Chain.prototype.singleTap = async function(
  el,
  corx,
  cory,
  capabilities
) {
  const doc = el.ownerDocument;
  // after this block, the element will be scrolled into view
  let visible = lazy.element.isVisible(el, corx, cory);
  if (!visible) {
    throw new lazy.error.ElementNotInteractableError(
      "Element is not currently visible and may not be manipulated"
    );
  }

  let a11y = lazy.accessibility.get(capabilities["moz:accessibilityChecks"]);
  let acc = await a11y.getAccessible(el, true);
  a11y.assertVisible(acc, el, visible);
  a11y.assertActionable(acc, el);
  if (!doc.createTouch) {
    this.mouseEventsOnly = true;
  }
  let c = lazy.element.coordinates(el, corx, cory);
  if (!this.mouseEventsOnly) {
    let touchId = this.nextTouchId++;
    let touch = this.createATouch(el, c.x, c.y, touchId);
    this.emitTouchEvent(doc, "touchstart", touch);
    this.emitTouchEvent(doc, "touchend", touch);
  }
  this.mouseTap(doc, c.x, c.y);
};

/**
 * Emit events for each action in the provided chain.
 *
 * To emit touch events for each finger, one might send a [["press", id],
 * ["wait", 5], ["release"]] chain.
 *
 * @param {Array.<Array<?>>} chain
 *     A multi-dimensional array of actions.
 * @param {Object.<string, number>} touchId
 *     Represents the finger ID.
 * @param {number} i
 *     Keeps track of the current action of the chain.
 * @param {Object.<string, boolean>} keyModifiers
 *     Keeps track of keyDown/keyUp pairs through an action chain.
 * @param {function(?)} cb
 *     Called on success.
 *
 * @return {Object.<string, number>}
 *     Last finger ID, or an empty object.
 */
action.Chain.prototype.actions = function(chain, touchId, i, keyModifiers, cb) {
  if (i == chain.length) {
    cb(touchId || null);
    this.resetValues();
    return;
  }

  let pack = chain[i];
  let command = pack[0];
  let webEl;
  let el;
  let c;
  i++;

  if (!["press", "wait", "keyDown", "keyUp", "click"].includes(command)) {
    // if mouseEventsOnly, then touchIds isn't used
    if (!(touchId in this.touchIds) && !this.mouseEventsOnly) {
      this.resetValues();
      throw new lazy.error.WebDriverError("Element has not been pressed");
    }
  }

  switch (command) {
    case "keyDown":
      lazy.event.sendKeyDown(pack[1], keyModifiers, this.container.frame);
      this.actions(chain, touchId, i, keyModifiers, cb);
      break;

    case "keyUp":
      lazy.event.sendKeyUp(pack[1], keyModifiers, this.container.frame);
      this.actions(chain, touchId, i, keyModifiers, cb);
      break;

    case "click":
      webEl = lazy.WebReference.fromUUID(pack[1], "content");
      el = this.seenEls.get(webEl);
      let button = pack[2];
      let clickCount = pack[3];
      c = lazy.element.coordinates(el);
      this.mouseTap(
        el.ownerDocument,
        c.x,
        c.y,
        button,
        clickCount,
        keyModifiers
      );
      if (button == 2) {
        this.emitMouseEvent(
          el.ownerDocument,
          "contextmenu",
          c.x,
          c.y,
          button,
          clickCount,
          keyModifiers
        );
      }
      this.actions(chain, touchId, i, keyModifiers, cb);
      break;

    case "press":
      if (this.lastCoordinates) {
        this.generateEvents(
          "cancel",
          this.lastCoordinates[0],
          this.lastCoordinates[1],
          touchId,
          null,
          keyModifiers
        );
        this.resetValues();
        throw new lazy.error.WebDriverError(
          "Invalid Command: press cannot follow an active touch event"
        );
      }

      // look ahead to check if we're scrolling,
      // needed for APZ touch dispatching
      if (i != chain.length && chain[i][0].includes("move")) {
        this.scrolling = true;
      }
      webEl = lazy.WebReference.fromUUID(pack[1], "content");
      el = this.seenEls.get(webEl);
      c = lazy.element.coordinates(el, pack[2], pack[3]);
      touchId = this.generateEvents("press", c.x, c.y, null, el, keyModifiers);
      this.actions(chain, touchId, i, keyModifiers, cb);
      break;

    case "release":
      this.generateEvents(
        "release",
        this.lastCoordinates[0],
        this.lastCoordinates[1],
        touchId,
        null,
        keyModifiers
      );
      this.actions(chain, null, i, keyModifiers, cb);
      this.scrolling = false;
      break;

    case "move":
      webEl = lazy.WebReference.fromUUID(pack[1], "content");
      el = this.seenEls.get(webEl);
      c = lazy.element.coordinates(el);
      this.generateEvents("move", c.x, c.y, touchId, null, keyModifiers);
      this.actions(chain, touchId, i, keyModifiers, cb);
      break;

    case "moveByOffset":
      this.generateEvents(
        "move",
        this.lastCoordinates[0] + pack[1],
        this.lastCoordinates[1] + pack[2],
        touchId,
        null,
        keyModifiers
      );
      this.actions(chain, touchId, i, keyModifiers, cb);
      break;

    case "wait":
      if (pack[1] != null) {
        let time = pack[1] * 1000;

        // standard waiting time to fire contextmenu
        let standard = lazy.Preferences.get(
          CONTEXT_MENU_DELAY_PREF,
          DEFAULT_CONTEXT_MENU_DELAY
        );

        if (time >= standard && this.isTap) {
          chain.splice(i, 0, ["longPress"], ["wait", (time - standard) / 1000]);
          time = standard;
        }
        this.checkTimer.initWithCallback(
          () => this.actions(chain, touchId, i, keyModifiers, cb),
          time,
          Ci.nsITimer.TYPE_ONE_SHOT
        );
      } else {
        this.actions(chain, touchId, i, keyModifiers, cb);
      }
      break;

    case "cancel":
      this.generateEvents(
        "cancel",
        this.lastCoordinates[0],
        this.lastCoordinates[1],
        touchId,
        null,
        keyModifiers
      );
      this.actions(chain, touchId, i, keyModifiers, cb);
      this.scrolling = false;
      break;

    case "longPress":
      this.generateEvents(
        "contextmenu",
        this.lastCoordinates[0],
        this.lastCoordinates[1],
        touchId,
        null,
        keyModifiers
      );
      this.actions(chain, touchId, i, keyModifiers, cb);
      break;
  }
};

/**
 * Given an element and a pair of coordinates, returns an array of the
 * form [clientX, clientY, pageX, pageY, screenX, screenY].
 */
action.Chain.prototype.getCoordinateInfo = function(el, corx, cory) {
  let win = el.ownerGlobal;
  return [
    corx, // clientX
    cory, // clientY
    corx + win.pageXOffset, // pageX
    cory + win.pageYOffset, // pageY
    corx + win.mozInnerScreenX, // screenX
    cory + win.mozInnerScreenY, // screenY
  ];
};

/**
 * @param {number} x
 *     X coordinate of the location to generate the event that is relative
 *     to the viewport.
 * @param {number} y
 *     Y coordinate of the location to generate the event that is relative
 *     to the viewport.
 */
action.Chain.prototype.generateEvents = function(
  type,
  x,
  y,
  touchId,
  target,
  keyModifiers
) {
  this.lastCoordinates = [x, y];
  let doc = this.container.frame.document;

  switch (type) {
    case "tap":
      if (this.mouseEventsOnly) {
        let touch = this.createATouch(target, x, y, touchId);
        this.mouseTap(
          touch.target.ownerDocument,
          touch.clientX,
          touch.clientY,
          null,
          null,
          keyModifiers
        );
      } else {
        touchId = this.nextTouchId++;
        let touch = this.createATouch(target, x, y, touchId);
        this.emitTouchEvent(doc, "touchstart", touch);
        this.emitTouchEvent(doc, "touchend", touch);
        this.mouseTap(
          touch.target.ownerDocument,
          touch.clientX,
          touch.clientY,
          null,
          null,
          keyModifiers
        );
      }
      this.lastCoordinates = null;
      break;

    case "press":
      this.isTap = true;
      if (this.mouseEventsOnly) {
        this.emitMouseEvent(doc, "mousemove", x, y, null, null, keyModifiers);
        this.emitMouseEvent(doc, "mousedown", x, y, null, null, keyModifiers);
      } else {
        touchId = this.nextTouchId++;
        let touch = this.createATouch(target, x, y, touchId);
        this.emitTouchEvent(doc, "touchstart", touch);
        this.touchIds[touchId] = touch;
        return touchId;
      }
      break;

    case "release":
      if (this.mouseEventsOnly) {
        let [x, y] = this.lastCoordinates;
        this.emitMouseEvent(doc, "mouseup", x, y, null, null, keyModifiers);
      } else {
        let touch = this.touchIds[touchId];
        let [x, y] = this.lastCoordinates;

        touch = this.createATouch(touch.target, x, y, touchId);
        this.emitTouchEvent(doc, "touchend", touch);

        if (this.isTap) {
          this.mouseTap(
            touch.target.ownerDocument,
            touch.clientX,
            touch.clientY,
            null,
            null,
            keyModifiers
          );
        }
        delete this.touchIds[touchId];
      }

      this.isTap = false;
      this.lastCoordinates = null;
      break;

    case "cancel":
      this.isTap = false;
      if (this.mouseEventsOnly) {
        let [x, y] = this.lastCoordinates;
        this.emitMouseEvent(doc, "mouseup", x, y, null, null, keyModifiers);
      } else {
        this.emitTouchEvent(doc, "touchcancel", this.touchIds[touchId]);
        delete this.touchIds[touchId];
      }
      this.lastCoordinates = null;
      break;

    case "move":
      this.isTap = false;
      if (this.mouseEventsOnly) {
        this.emitMouseEvent(doc, "mousemove", x, y, null, null, keyModifiers);
      } else {
        let touch = this.createATouch(
          this.touchIds[touchId].target,
          x,
          y,
          touchId
        );
        this.touchIds[touchId] = touch;
        this.emitTouchEvent(doc, "touchmove", touch);
      }
      break;

    case "contextmenu":
      this.isTap = false;
      let event = this.container.frame.document.createEvent("MouseEvents");
      if (this.mouseEventsOnly) {
        target = doc.elementFromPoint(
          this.lastCoordinates[0],
          this.lastCoordinates[1]
        );
      } else {
        target = this.touchIds[touchId].target;
      }

      let [clientX, clientY, , , screenX, screenY] = this.getCoordinateInfo(
        target,
        x,
        y
      );

      event.initMouseEvent(
        "contextmenu",
        true,
        true,
        target.ownerGlobal,
        1,
        screenX,
        screenY,
        clientX,
        clientY,
        false,
        false,
        false,
        false,
        0,
        null
      );
      target.dispatchEvent(event);
      break;

    default:
      throw new lazy.error.WebDriverError("Unknown event type: " + type);
  }
  return null;
};

action.Chain.prototype.mouseTap = function(doc, x, y, button, count, mod) {
  this.emitMouseEvent(doc, "mousemove", x, y, button, count, mod);
  this.emitMouseEvent(doc, "mousedown", x, y, button, count, mod);
  this.emitMouseEvent(doc, "mouseup", x, y, button, count, mod);
};
