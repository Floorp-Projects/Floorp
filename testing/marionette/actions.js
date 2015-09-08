/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["ActionChain"];

/**
 * Functionality for (single finger) action chains.
 */
this.ActionChain = function(utils, checkForInterrupted) {
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
  this.checkTimer = Components.classes["@mozilla.org/timer;1"]
      .createInstance(Components.interfaces.nsITimer);

  // callbacks for command completion
  this.onSuccess = null;
  this.onError = null;
  if (typeof checkForInterrupted == "function") {
    this.checkForInterrupted = checkForInterrupted;
  } else {
    this.checkForInterrupted = () => {};
  }

  // determines if we create touch events
  this.inputSource = null;

  // test utilities providing some event synthesis code
  this.utils = utils;
};

ActionChain.prototype.dispatchActions = function(
    args,
    touchId,
    container,
    elementManager,
    callbacks,
    touchProvider) {
  // Some touch events code in the listener needs to do ipc, so we can't
  // share this code across chrome/content.
  if (touchProvider) {
    this.touchProvider = touchProvider;
  }

  this.elementManager = elementManager;
  let commandArray = elementManager.convertWrappedArguments(args, container);
  this.onSuccess = callbacks.onSuccess;
  this.onError = callbacks.onError;
  this.container = container;

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
    metaKey: false
  };

  try {
    this.actions(commandArray, touchId, 0, keyModifiers);
  } catch (e) {
    this.onError(e);
    this.resetValues();
  }
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
ActionChain.prototype.emitMouseEvent = function(
    doc,
    type,
    elClientX,
    elClientY,
    button,
    clickCount,
    modifiers) {
  if (!this.checkForInterrupted()) {
    let loggingInfo = "emitting Mouse event of type " + type +
      " at coordinates (" + elClientX + ", " + elClientY +
      ") relative to the viewport\n" +
      " button: " + button + "\n" +
      " clickCount: " + clickCount + "\n";
    dump(Date.now() + " Marionette: " + loggingInfo);

    let win = doc.defaultView;
    let domUtils = win.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
        .getInterface(Components.interfaces.nsIDOMWindowUtils);

    let mods;
    if (typeof modifiers != "undefined") {
      mods = this.utils._parseModifiers(modifiers);
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
ActionChain.prototype.resetValues = function() {
  this.onSuccess = null;
  this.onError = null;
  this.container = null;
  this.elementManager = null;
  this.touchProvider = null;
  this.mouseEventsOnly = false;
};

/**
 * Function to emit touch events for each finger. e.g.
 * finger=[['press', id], ['wait', 5], ['release']] touchId represents
 * the finger id, i keeps track of the current action of the chain
 * keyModifiers is an object keeping track keyDown/keyUp pairs through
 * an action chain.
 */
ActionChain.prototype.actions = function(chain, touchId, i, keyModifiers) {
  if (i == chain.length) {
    this.onSuccess({value: touchId || null});
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

  switch(command) {
    case "keyDown":
      this.utils.sendKeyDown(pack[1], keyModifiers, this.container.frame);
      this.actions(chain, touchId, i, keyModifiers);
      break;

    case "keyUp":
      this.utils.sendKeyUp(pack[1], keyModifiers, this.container.frame);
      this.actions(chain, touchId, i, keyModifiers);
      break;

    case "click":
      el = this.elementManager.getKnownElement(pack[1], this.container);
      let button = pack[2];
      let clickCount = pack[3];
      c = this.coordinates(el, null, null);
      this.mouseTap(el.ownerDocument, c.x, c.y, button, clickCount, keyModifiers);
      if (button == 2) {
        this.emitMouseEvent(el.ownerDocument, "contextmenu", c.x, c.y,
            button, clickCount, keyModifiers);
      }
      this.actions(chain, touchId, i, keyModifiers);
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

      // look ahead to check if we're scrolling. Needed for APZ touch dispatching.
      if ((i != chain.length) && (chain[i][0].indexOf('move') !== -1)) {
        this.scrolling = true;
      }
      el = this.elementManager.getKnownElement(pack[1], this.container);
      c = this.coordinates(el, pack[2], pack[3]);
      touchId = this.generateEvents("press", c.x, c.y, null, el, keyModifiers);
      this.actions(chain, touchId, i, keyModifiers);
      break;

    case "release":
      this.generateEvents(
          "release",
          this.lastCoordinates[0],
          this.lastCoordinates[1],
          touchId,
          null,
          keyModifiers);
      this.actions(chain, null, i, keyModifiers);
      this.scrolling =  false;
      break;

    case "move":
      el = this.elementManager.getKnownElement(pack[1], this.container);
      c = this.coordinates(el);
      this.generateEvents("move", c.x, c.y, touchId, null, keyModifiers);
      this.actions(chain, touchId, i, keyModifiers);
      break;

    case "moveByOffset":
      this.generateEvents(
          "move",
          this.lastCoordinates[0] + pack[1],
          this.lastCoordinates[1] + pack[2],
          touchId,
          null,
          keyModifiers);
      this.actions(chain, touchId, i, keyModifiers);
      break;

    case "wait":
      if (pack[1] != null) {
        let time = pack[1] * 1000;

        // standard waiting time to fire contextmenu
        let standard = 750;
        try {
          standard = Services.prefs.getIntPref("ui.click_hold_context_menus.delay");
        } catch (e) {}

        if (time >= standard && this.isTap) {
          chain.splice(i, 0, ["longPress"], ["wait", (time - standard) / 1000]);
          time = standard;
        }
        this.checkTimer.initWithCallback(
            () => this.actions(chain, touchId, i, keyModifiers),
            time, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
      } else {
        this.actions(chain, touchId, i, keyModifiers);
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
      this.actions(chain, touchId, i, keyModifiers);
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
      this.actions(chain, touchId, i, keyModifiers);
      break;

  }
};

/**
 * This function generates a pair of coordinates relative to the viewport given a
 * target element and coordinates relative to that element's top-left corner.
 *
 * @param {DOMElement} target
 *     The target to calculate coordinates of.
 * @param {number} x
 *     X coordinate relative to target.  If unspecified, the centre of
 *     the target is used.
 * @param {number} y
 *     Y coordinate relative to target.  If unspecified, the centre of
 *     the target is used.
 */
ActionChain.prototype.coordinates = function(target, x, y) {
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
};

/**
 * Given an element and a pair of coordinates, returns an array of the
 * form [clientX, clientY, pageX, pageY, screenX, screenY].
 */
ActionChain.prototype.getCoordinateInfo = function(el, corx, cory) {
  let win = el.ownerDocument.defaultView;
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
ActionChain.prototype.generateEvents = function(
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
          target.ownerDocument.defaultView,
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

ActionChain.prototype.mouseTap = function(doc, x, y, button, count, mod) {
  this.emitMouseEvent(doc, "mousemove", x, y, button, count, mod);
  this.emitMouseEvent(doc, "mousedown", x, y, button, count, mod);
  this.emitMouseEvent(doc, "mouseup", x, y, button, count, mod);
};
