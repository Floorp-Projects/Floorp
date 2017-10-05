/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");

Cu.import("chrome://marionette/content/element.js");
Cu.import("chrome://marionette/content/evaluate.js");
Cu.import("chrome://marionette/content/event.js");

const CONTEXT_MENU_DELAY_PREF = "ui.click_hold_context_menus.delay";
const DEFAULT_CONTEXT_MENU_DELAY = 750;  // ms

this.EXPORTED_SYMBOLS = ["legacyaction"];

const logger = Log.repository.getLogger("Marionette");

/** @namespace */
this.legacyaction = this.action = {};

/**
 * Functionality for (single finger) action chains.
 */
action.Chain = function (checkForInterrupted) {
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

  if (typeof checkForInterrupted == "function") {
    this.checkForInterrupted = checkForInterrupted;
  } else {
    this.checkForInterrupted = () => {};
  }

  // determines if we create touch events
  this.inputSource = null;
};

action.Chain.prototype.dispatchActions = function (
    args,
    touchId,
    container,
    seenEls,
    touchProvider) {
  // Some touch events code in the listener needs to do ipc, so we can't
  // share this code across chrome/content.
  if (touchProvider) {
    this.touchProvider = touchProvider;
  }

  this.seenEls = seenEls;
  this.container = container;
  let commandArray = evaluate.fromJSON(
      args, seenEls, container.frame, container.shadowRoot);

  if (touchId == null) {
    touchId = this.nextTouchId++;
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
action.Chain.prototype.emitMouseEvent = function (
    doc,
    type,
    elClientX,
    elClientY,
    button,
    clickCount,
    modifiers) {
  if (!this.checkForInterrupted()) {
    logger.debug(`Emitting ${type} mouse event ` +
        `at coordinates (${elClientX}, ${elClientY}) ` +
        `relative to the viewport, ` +
        `button: ${button}, ` +
        `clickCount: ${clickCount}`);

    let win = doc.defaultView;
    let domUtils = win.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindowUtils);

    let mods;
    if (typeof modifiers != "undefined") {
      mods = event.parseModifiers_(modifiers);
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
        this.inputSource);
  }
};

/**
 * Reset any persisted values after a command completes.
 */
action.Chain.prototype.resetValues = function() {
  this.container = null;
  this.seenEls = null;
  this.touchProvider = null;
  this.mouseEventsOnly = false;
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
action.Chain.prototype.actions = function (chain, touchId, i, keyModifiers, cb) {
  if (i == chain.length) {
    cb(touchId || null);
    this.resetValues();
    return;
  }

  let pack = chain[i];
  let command = pack[0];
  let el;
  let c;
  i++;

  if (["press", "wait", "keyDown", "keyUp", "click"].indexOf(command) == -1) {
    // if mouseEventsOnly, then touchIds isn't used
    if (!(touchId in this.touchIds) && !this.mouseEventsOnly) {
      this.resetValues();
      throw new WebDriverError("Element has not been pressed");
    }
  }

  switch (command) {
    case "keyDown":
      event.sendKeyDown(pack[1], keyModifiers, this.container.frame);
      this.actions(chain, touchId, i, keyModifiers, cb);
      break;

    case "keyUp":
      event.sendKeyUp(pack[1], keyModifiers, this.container.frame);
      this.actions(chain, touchId, i, keyModifiers, cb);
      break;

    case "click":
      el = this.seenEls.get(pack[1]);
      let button = pack[2];
      let clickCount = pack[3];
      c = element.coordinates(el);
      this.mouseTap(el.ownerDocument, c.x, c.y, button, clickCount, keyModifiers);
      if (button == 2) {
        this.emitMouseEvent(el.ownerDocument, "contextmenu", c.x, c.y,
            button, clickCount, keyModifiers);
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
            keyModifiers);
        this.resetValues();
        throw new WebDriverError(
            "Invalid Command: press cannot follow an active touch event");
      }

      // look ahead to check if we're scrolling,
      // needed for APZ touch dispatching
      if ((i != chain.length) && (chain[i][0].indexOf('move') !== -1)) {
        this.scrolling = true;
      }
      el = this.seenEls.get(pack[1]);
      c = element.coordinates(el, pack[2], pack[3]);
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
          keyModifiers);
      this.actions(chain, null, i, keyModifiers, cb);
      this.scrolling =  false;
      break;

    case "move":
      el = this.seenEls.get(pack[1]);
      c = element.coordinates(el);
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
          keyModifiers);
      this.actions(chain, touchId, i, keyModifiers, cb);
      break;

    case "wait":
      if (pack[1] != null) {
        let time = pack[1] * 1000;

        // standard waiting time to fire contextmenu
        let standard = Preferences.get(
            CONTEXT_MENU_DELAY_PREF,
            DEFAULT_CONTEXT_MENU_DELAY);

        if (time >= standard && this.isTap) {
          chain.splice(i, 0, ["longPress"], ["wait", (time - standard) / 1000]);
          time = standard;
        }
        this.checkTimer.initWithCallback(
            () => this.actions(chain, touchId, i, keyModifiers, cb),
            time, Ci.nsITimer.TYPE_ONE_SHOT);
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
          keyModifiers);
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
          keyModifiers);
      this.actions(chain, touchId, i, keyModifiers, cb);
      break;
  }
};

/**
 * Given an element and a pair of coordinates, returns an array of the
 * form [clientX, clientY, pageX, pageY, screenX, screenY].
 */
action.Chain.prototype.getCoordinateInfo = function (el, corx, cory) {
  let win = el.ownerGlobal;
  return [
    corx, // clientX
    cory, // clientY
    corx + win.pageXOffset, // pageX
    cory + win.pageYOffset, // pageY
    corx + win.mozInnerScreenX, // screenX
    cory + win.mozInnerScreenY // screenY
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
action.Chain.prototype.generateEvents = function (
    type, x, y, touchId, target, keyModifiers) {
  this.lastCoordinates = [x, y];
  let doc = this.container.frame.document;

  switch (type) {
    case "tap":
      if (this.mouseEventsOnly) {
        this.mouseTap(
            touch.target.ownerDocument,
            touch.clientX,
            touch.clientY,
            null,
            null,
            keyModifiers);
      } else {
        touchId = this.nextTouchId++;
        let touch = this.touchProvider.createATouch(target, x, y, touchId);
        this.touchProvider.emitTouchEvent("touchstart", touch);
        this.touchProvider.emitTouchEvent("touchend", touch);
        this.mouseTap(
            touch.target.ownerDocument,
            touch.clientX,
            touch.clientY,
            null,
            null,
            keyModifiers);
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
        let touch = this.touchProvider.createATouch(target, x, y, touchId);
        this.touchProvider.emitTouchEvent("touchstart", touch);
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

        touch = this.touchProvider.createATouch(touch.target, x, y, touchId);
        this.touchProvider.emitTouchEvent("touchend", touch);

        if (this.isTap) {
          this.mouseTap(
              touch.target.ownerDocument,
              touch.clientX,
              touch.clientY,
              null,
              null,
              keyModifiers);
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
        this.touchProvider.emitTouchEvent("touchcancel", this.touchIds[touchId]);
        delete this.touchIds[touchId];
      }
      this.lastCoordinates = null;
      break;

    case "move":
      this.isTap = false;
      if (this.mouseEventsOnly) {
        this.emitMouseEvent(doc, "mousemove", x, y, null, null, keyModifiers);
      } else {
        let touch = this.touchProvider.createATouch(
            this.touchIds[touchId].target, x, y, touchId);
        this.touchIds[touchId] = touch;
        this.touchProvider.emitTouchEvent("touchmove", touch);
      }
      break;

    case "contextmenu":
      this.isTap = false;
      let event = this.container.frame.document.createEvent("MouseEvents");
      if (this.mouseEventsOnly) {
        target = doc.elementFromPoint(this.lastCoordinates[0], this.lastCoordinates[1]);
      } else {
        target = this.touchIds[touchId].target;
      }

      let [clientX, clientY, pageX, pageY, screenX, screenY] =
          this.getCoordinateInfo(target, x, y);

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
          null);
      target.dispatchEvent(event);
      break;

    default:
      throw new WebDriverError("Unknown event type: " + type);
  }
  this.checkForInterrupted();
};

action.Chain.prototype.mouseTap = function (doc, x, y, button, count, mod) {
  this.emitMouseEvent(doc, "mousemove", x, y, button, count, mod);
  this.emitMouseEvent(doc, "mousedown", x, y, button, count, mod);
  this.emitMouseEvent(doc, "mouseup", x, y, button, count, mod);
};
