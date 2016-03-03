/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Provides functionality for creating and sending DOM events.

"use strict";

const {interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
const logger = Log.repository.getLogger("Marionette");

Cu.import("chrome://marionette/content/element.js");
Cu.import("chrome://marionette/content/error.js");

this.EXPORTED_SYMBOLS = ["event"];

// must be synchronised with nsIDOMWindowUtils
const COMPOSITION_ATTR_RAWINPUT = 0x02;
const COMPOSITION_ATTR_SELECTEDRAWTEXT = 0x03;
const COMPOSITION_ATTR_CONVERTEDTEXT = 0x04;
const COMPOSITION_ATTR_SELECTEDCONVERTEDTEXT = 0x05;

// TODO(ato): Document!
let seenEvent = false;

function getDOMWindowUtils(win) {
  if (!win) {
    win = window;
  }

  // this assumes we are operating in chrome space
  return win.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils);
}

this.event = {};

event.MouseEvents = {
  click: 0,
  dblclick: 1,
  mousedown: 2,
  mouseup: 3,
  mouseover: 4,
  mouseout: 5,
};

event.Modifiers = {
  shiftKey: 0,
  ctrlKey: 1,
  altKey: 2,
  metaKey: 3,
};

/**
 * Sends a mouse event to given target.
 *
 * @param {nsIDOMMouseEvent} mouseEvent
 *     Event to send.
 * @param {(Element|string)} target
 *     Target of event.  Can either be an Element or the ID of an element.
 * @param {Window=} window
 *     Window object.  Defaults to the current window.
 *
 * @throws {TypeError}
 *     If the event is unsupported.
 */
event.sendMouseEvent = function(mouseEvent, target, window = undefined) {
  if (event.MouseEvents.hasOwnProperty(mouseEvent.type)) {
    throw new TypeError("Unsupported event type: " + mouseEvent.type);
  }

  if (!(target instanceof Element)) {
    target = window.document.getElementById(target);
  }

  let ev = window.document.createEvent("MouseEvent");

  let type = mouseEvent.type;
  let view = window;

  let detail = mouseEvent.detail;
  if (!detail) {
    if (mouseEvent.type in ["click", "mousedown", "mouseup"]) {
      detail = 1;
    } else if (mouseEvent.type == "dblclick") {
      detail = 2;
    } else {
      detail = 0;
    }
  }

  let screenX = mouseEvent.screenX || 0;
  let screenY = mouseEvent.screenY || 0;
  let clientX = mouseEvent.clientX || 0;
  let clientY = mouseEvent.clientY || 0;
  let ctrlKey = mouseEvent.ctrlKey || false;
  let altKey = mouseEvent.altKey || false;
  let shiftKey = mouseEvent.shiftKey || false;
  let metaKey = mouseEvent.metaKey || false;
  let button = mouseEvent.button || 0;
  let relatedTarget = mouseEvent.relatedTarget || null;

  ev.initMouseEvent(
      mouseEvent.type,
      /* canBubble */ true,
      /* cancelable */ true,
      view,
      detail,
      screenX,
      screenY,
      clientX,
      clientY,
      ctrlKey,
      altKey,
      shiftKey,
      metaKey,
      button,
      relatedTarget);
};

/**
 * Send character to the currently focused element.
 *
 * This function handles casing of characters (sends the right charcode,
 * and sends a shift key for uppercase chars).  No other modifiers are
 * handled at this point.
 *
 * For now this method only works for English letters (lower and upper
 * case) and the digits 0-9.
 */
event.sendChar = function(char, window = undefined) {
  // DOM event charcodes match ASCII (JS charcodes) for a-zA-Z0-9
  let hasShift = (char == char.toUpperCase());
  event.synthesizeKey(char, {shiftKey: hasShift}, window);
};

/**
 * Send string to the focused element.
 *
 * For now this method only works for English letters (lower and upper
 * case) and the digits 0-9.
 */
event.sendString = function(string, window = undefined) {
  for (let i = 0; i < string.length; ++i) {
    event.sendChar(string.charAt(i), window);
  }
};

/**
 * Send the non-character key to the focused element.
 *
 * The name of the key should be the part that comes after "DOM_VK_"
 * in the nsIDOMKeyEvent constant name for this key.  No modifiers are
 * handled at this point.
 */
event.sendKey = function(key, window = undefined) {
  let keyName = "VK_" + key.toUpperCase();
  event.synthesizeKey(keyName, {shiftKey: false}, window);
};

// TODO(ato): Unexpose this when action.Chain#emitMouseEvent
// no longer emits its own events
event.parseModifiers_ = function(event) {
  let mval = 0;
  if (event.shiftKey) {
    mval |= Ci.nsIDOMNSEvent.SHIFT_MASK;
  }
  if (event.ctrlKey) {
    mval |= Ci.nsIDOMNSEvent.CONTROL_MASK;
  }
  if (event.altKey) {
    mval |= Ci.nsIDOMNSEvent.ALT_MASK;
  }
  if (event.metaKey) {
    mval |= Ci.nsIDOMNSEvent.META_MASK;
  }
  if (event.accelKey) {
    if (navigator.platform.indexOf("Mac") >= 0) {
      mval |= Ci.nsIDOMNSEvent.META_MASK;
    } else {
      mval |= Ci.nsIDOMNSEvent.CONTROL_MASK;
    }
  }
  return mval;
};

/**
 * Synthesise a mouse event on a target.
 *
 * The actual client point is determined by taking the aTarget's client
 * box and offseting it by offsetX and offsetY.  This allows mouse clicks
 * to be simulated by calling this method.
 *
 * If the type is specified, an mouse event of that type is
 * fired. Otherwise, a mousedown followed by a mouse up is performed.
 *
 * @param {Element} element
 *     Element to click.
 * @param {number} offsetX
 *     Horizontal offset to click from the target's bounding box.
 * @param {number} offsetY
 *     Vertical offset to click from the target's bounding box.
 * @param {Object.<string, ?>} opts
 *     Object which may contain the properties "shiftKey", "ctrlKey",
 *     "altKey", "metaKey", "accessKey", "clickCount", "button", and
 *     "type".
 * @param {Window=} window
 *     Window object.  Defaults to the current window.
 */
event.synthesizeMouse = function(
    element, offsetX, offsetY, opts, window = undefined) {
  let rect = element.getBoundingClientRect();
  event.synthesizeMouseAtPoint(
      rect.left + offsetX, rect.top + offsetY, opts, window);
};

/*
 * Synthesize a mouse event at a particular point in a window.
 *
 * If the type of the event is specified, a mouse event of that type is
 * fired. Otherwise, a mousedown followed by a mouse up is performed.
 *
 * @param {number} left
 *     CSS pixels from the left document margin.
 * @param {number} top
 *     CSS pixels from the top document margin.
 * @param {Object.<string, ?>} event
 *     Object which may contain the properties "shiftKey", "ctrlKey",
 *     "altKey", "metaKey", "accessKey", "clickCount", "button", and
 *     "type".
 * @param {Window=} window
 *     Window object.  Defaults to the current window.
 */
event.synthesizeMouseAtPoint = function(
    left, top, opts, window = undefined) {

  let domutils = getDOMWindowUtils(window);

  let button = event.button || 0;
  let clickCount = event.clickCount || 1;
  let modifiers = event.parseModifiers_(event);

  if (("type" in event) && event.type) {
    domutils.sendMouseEvent(
        event.type, left, top, button, clickCount, modifiers);
  } else {
    domutils.sendMouseEvent(
        "mousedown", left, top, button, clickCount, modifiers);
    domutils.sendMouseEvent(
        "mouseup", left, top, button, clickCount, modifiers);
  }
};

/**
 * Call event.synthesizeMouse with coordinates at the centre of the
 * target.
 */
event.synthesizeMouseAtCenter = function(element, event, window) {
  let rect = element.getBoundingClientRect();
  event.synthesizeMouse(
      element,
      rect.width / 2,
      rect.height / 2,
      event,
      window);
};

/**
 * Synthesise a mouse scroll event on a target.
 *
 * The actual client point is determined by taking the target's client
 * box and offseting it by |offsetX| and |offsetY|.
 *
 * If the |type| property is specified for the |event| argument, a mouse
 * scroll event of that type is fired.  Otherwise, DOMMouseScroll is used.
 *
 * If the |axis| is specified, it must be one of "horizontal" or
 * "vertical". If not specified, "vertical" is used.
 *
 * |delta| is the amount to scroll by (can be positive or negative).
 * It must be specified.
 *
 * |hasPixels| specifies whether kHasPixels should be set in the
 * |scrollFlags|.
 *
 * |isMomentum| specifies whether kIsMomentum should be set in the
 * |scrollFlags|.
 *
 * @param {Element} target
 * @param {number} offsetY
 * @param {number} offsetY
 * @param {Object.<string, ?>} event
 *     Object which may contain the properties shiftKey, ctrlKey, altKey,
 *     metaKey, accessKey, button, type, axis, delta, and hasPixels.
 * @param {Window=} window
 *     Window object.  Defaults to the current window.
 */
event.synthesizeMouseScroll = function(
    target, offsetX, offsetY, ev, window = undefined) {

  let domutils = getDOMWindowUtils(window);

  // see nsMouseScrollFlags in nsGUIEvent.h
  const kIsVertical = 0x02;
  const kIsHorizontal = 0x04;
  const kHasPixels = 0x08;
  const kIsMomentum = 0x40;

  let button = ev.button || 0;
  let modifiers = event.parseModifiers_(ev);

  let rect = target.getBoundingClientRect();
  let left = rect.left;
  let top = rect.top;

  let type = (("type" in ev) && ev.type) || "DOMMouseScroll";
  let axis = ev.axis || "vertical";
  let scrollFlags = (axis == "horizontal") ? kIsHorizontal : kIsVertical;
  if (ev.hasPixels) {
    scrollFlags |= kHasPixels;
  }
  if (ev.isMomentum) {
    scrollFlags |= kIsMomentum;
  }

  domutils.sendMouseScrollEvent(
      type,
      left + offsetX,
      top + offsetY,
      button,
      scrollFlags,
      ev.delta,
      modifiers);
};

function computeKeyCodeFromChar_(char) {
  if (char.length != 1) {
    return 0;
  }

  if (char >= "a" && char <= "z") {
    return Ci.nsIDOMKeyEvent.DOM_VK_A + char.charCodeAt(0) - "a".charCodeAt(0);
  }
  if (char >= "A" && char <= "Z") {
    return Ci.nsIDOMKeyEvent.DOM_VK_A + char.charCodeAt(0) - "A".charCodeAt(0);
  }
  if (char >= "0" && char <= "9") {
    return Ci.nsIDOMKeyEvent.DOM_VK_0 + char.charCodeAt(0) - "0".charCodeAt(0);
  }

  // returns US keyboard layout's keycode
  switch (char) {
    case "~":
    case "`":
      return Ci.nsIDOMKeyEvent.DOM_VK_BACK_QUOTE;

    case "!":
      return Ci.nsIDOMKeyEvent.DOM_VK_1;

    case "@":
      return Ci.nsIDOMKeyEvent.DOM_VK_2;

    case "#":
      return Ci.nsIDOMKeyEvent.DOM_VK_3;

    case "$":
      return Ci.nsIDOMKeyEvent.DOM_VK_4;

    case "%":
      return Ci.nsIDOMKeyEvent.DOM_VK_5;

    case "^":
      return Ci.nsIDOMKeyEvent.DOM_VK_6;

    case "&":
      return Ci.nsIDOMKeyEvent.DOM_VK_7;

    case "*":
      return Ci.nsIDOMKeyEvent.DOM_VK_8;

    case "(":
      return Ci.nsIDOMKeyEvent.DOM_VK_9;

    case ")":
      return Ci.nsIDOMKeyEvent.DOM_VK_0;

    case "-":
    case "_":
      return Ci.nsIDOMKeyEvent.DOM_VK_SUBTRACT;

    case "+":
    case "=":
      return Ci.nsIDOMKeyEvent.DOM_VK_EQUALS;

    case "{":
    case "[":
      return Ci.nsIDOMKeyEvent.DOM_VK_OPEN_BRACKET;

    case "}":
    case "]":
      return Ci.nsIDOMKeyEvent.DOM_VK_CLOSE_BRACKET;

    case "|":
    case "\\":
      return Ci.nsIDOMKeyEvent.DOM_VK_BACK_SLASH;

    case ":":
    case ";":
      return Ci.nsIDOMKeyEvent.DOM_VK_SEMICOLON;

    case "'":
    case "\"":
      return Ci.nsIDOMKeyEvent.DOM_VK_QUOTE;

    case "<":
    case ",":
      return Ci.nsIDOMKeyEvent.DOM_VK_COMMA;

    case ">":
    case ".":
      return Ci.nsIDOMKeyEvent.DOM_VK_PERIOD;

    case "?":
    case "/":
      return Ci.nsIDOMKeyEvent.DOM_VK_SLASH;

    case "\n":
      return Ci.nsIDOMKeyEvent.DOM_VK_RETURN;

    default:
      return 0;
  }
}

/**
 * Returns true if the given key should cause keypress event when widget
 * handles the native key event.  Otherwise, false.
 *
 * The key code should be one of consts of nsIDOMKeyEvent.DOM_VK_*,
 * or a key name begins with "VK_", or a character.
 */
event.isKeypressFiredKey = function(key) {
  if (typeof key == "string") {
    if (key.indexOf("VK_") === 0) {
      key = Ci.nsIDOMKeyEvent["DOM_" + key];
      if (!key) {
        throw new TypeError("Unknown key: " + key);
      }

    // if key generates a character, it must cause a keypress event
    } else {
      return true;
    }
  }

  switch (key) {
    case Ci.nsIDOMKeyEvent.DOM_VK_SHIFT:
    case Ci.nsIDOMKeyEvent.DOM_VK_CONTROL:
    case Ci.nsIDOMKeyEvent.DOM_VK_ALT:
    case Ci.nsIDOMKeyEvent.DOM_VK_CAPS_LOCK:
    case Ci.nsIDOMKeyEvent.DOM_VK_NUM_LOCK:
    case Ci.nsIDOMKeyEvent.DOM_VK_SCROLL_LOCK:
    case Ci.nsIDOMKeyEvent.DOM_VK_META:
      return false;

    default:
      return true;
  }
};

/**
 * Synthesise a key event.
 *
 * It is targeted at whatever would be targeted by an actual keypress
 * by the user, typically the focused element.
 *
 * @param {string} key
 *     Key to synthesise.  Should either be a character or a key code
 *     starting with "VK_" such as VK_RETURN.
 * @param {Object.<string, ?>} event
 *     Object which may contain the properties shiftKey, ctrlKey, altKey,
 *     metaKey, accessKey, type.  If the type is specified, a key event
 *    of that type is fired.  Otherwise, a keydown, a keypress, and then a
 *     keyup event are fired in sequence.
 * @param {Window=} window
 *     Window object.  Defaults to the current window.
 *
 * @throws {TypeError}
 *     If unknown key.
 */
event.synthesizeKey = function(key, ev, window = undefined) {
  let domutils = getDOMWindowUtils(window);

  let keyCode = 0;
  let charCode = 0;
  if (key.indexOf("VK_") === 0) {
    keyCode = Ci.nsIDOMKeyEvent["DOM_" + key];
    if (!keyCode) {
      throw new TypeError("Unknown key: " + key);
    }
  } else {
    charCode = key.charCodeAt(0);
    keyCode = computeKeyCodeFromChar_(key.charAt(0));
  }

  let modifiers = event.parseModifiers_(ev);

  // send keydown + (optional) keypress + keyup events
  if (!("type" in ev) || !ev.type) {
    let keyDownDefaultHappened = domutils.sendKeyEvent(
        "keydown", keyCode, 0, modifiers);
    if (event.isKeypressFiredKey(keyCode)) {
      domutils.sendKeyEvent(
          "keypress",
          charCode ? 0 : keyCode,
          charCode,
          modifiers,
          !keyDownDefaultHappened);
    }
    domutils.sendKeyEvent("keyup", keyCode, 0, modifiers);

  // send standalone keypress event
  } else if (ev.type == "keypress") {
    domutils.sendKeyEvent(
        ev.type,
        charCode ? 0 : keyCode,
        charCode,
        modifiers);

  // send other standalone event than keypress
  } else {
    domutils.sendKeyEvent(ev.type, keyCode, 0, modifiers);
  }
};

/**
 * Indicate that an event with an original target and type is expected
 * to be fired, or not expected to be fired.
 */
function expectEvent_(expectedTarget, expectedEvent, testName) {
  if (!expectedTarget || !expectedEvent) {
    return null;
  }

  seenEvent = false;

  let type;
  if (expectedEvent.charAt(0) == "!") {
    type = expectedEvent.substring(1);
  } else {
    type = expectedEvent;
  }

  let handler = ev => {
    let pass = (!seenEvent && ev.originalTarget == expectedTarget && ev.type == type);
    is(pass, true, `${testName} ${type} event target ${seenEvent ? "twice" : ""}`);
    seenEvent = true;
  };

  expectedTarget.addEventListener(type, handler, false);
  return handler;
}

/**
 * Check if the event was fired or not. The provided event handler will
 * be removed.
 */
function checkExpectedEvent_(
    expectedTarget, expectedEvent, eventHandler, testName) {

  if (eventHandler) {
    let expectEvent = (expectedEvent.charAt(0) != "!");
    let type = expectEvent;
    if (!type) {
      type = expectedEvent.substring(1);
    }
    expectedTarget.removeEventListener(type, eventHandler, false);

    let desc = `${type} event`;
    if (!expectEvent) {
      desc += " not";
    }
    is(seenEvent, expectEvent, `${testName} ${desc} fired`);
  }

  seenEvent = false;
}

/**
 * Similar to event.synthesizeMouse except that a test is performed to
 * see if an event is fired at the right target as a result.
 *
 * To test that an event is not fired, use an expected type preceded by
 * an exclamation mark, such as "!select". This might be used to test that
 * a click on a disabled element doesn't fire certain events for instance.
 *
 * @param {Element} target
 *     Synthesise the mouse event on this target.
 * @param {number} offsetX
 *     Horizontal offset from the target's bounding box.
 * @param {number} offsetY
 *     Vertical offset from the target's bounding box.
 * @param {Object.<string, ?>} ev
 *     Object which may contain the properties shiftKey, ctrlKey, altKey,
 *     metaKey, accessKey, type.
 * @param {Element} expectedTarget
 *     Expected originalTarget of the event.
 * @param {DOMEvent} expectedEvent
 *     Expected type of the event, such as "select".
 * @param {string} testName
 *     Test name when outputing results.
 * @param {Window=} window
 *     Window object.  Defaults to the current window.
 */
event.synthesizeMouseExpectEvent = function(
    target, offsetX, offsetY, ev, expectedTarget, expectedEvent,
    testName, window = undefined) {

  let eventHandler = expectEvent_(
      expectedTarget,
      expectedEvent,
      testName);
  event.synthesizeMouse(target, offsetX, offsetY, ev, window);
  checkExpectedEvent_(
      expectedTarget,
      expectedEvent,
      eventHandler,
      testName);
};

/**
 * Similar to synthesizeKey except that a test is performed to see if
 * an event is fired at the right target as a result.
 *
 * @param {string} key
 *     Key to synthesise.
 * @param {Object.<string, ?>} ev
 *     Object which may contain the properties shiftKey, ctrlKey, altKey,
 *     metaKey, accessKey, type.
 * @param {Element} expectedTarget
 *     Expected originalTarget of the event.
 * @param {DOMEvent} expectedEvent
 *     Expected type of the event, such as "select".
 * @param {string} testName
 *     Test name when outputing results
 * @param {Window=} window
 *     Window object.  Defaults to the current window.
 *
 * To test that an event is not fired, use an expected type preceded by an
 * exclamation mark, such as "!select".
 *
 * aWindow is optional, and defaults to the current window object.
 */
event.synthesizeKeyExpectEvent = function(
    key, ev, expectedTarget, expectedEvent, testName,
    window = undefined) {

  let eventHandler = expectEvent_(
      expectedTarget,
      expectedEvent,
      testName);
  event.synthesizeKey(key, ev, window);
  checkExpectedEvent_(
      expectedTarget,
      expectedEvent,
      eventHandler,
      testName);
};

/**
 * Synthesize a composition event.
 *
 * @param {DOMEvent} ev
 *     The composition event information.  This must have |type|
 *     member.  The value must be "compositionstart", "compositionend" or
 *     "compositionupdate".  And also this may have |data| and |locale|
 *     which would be used for the value of each property of the
 *     composition event.  Note that the data would be ignored if the
 *     event type were "compositionstart".
 * @param {Window=} window
 *     Window object.  Defaults to the current window.
 */
event.synthesizeComposition = function(ev, window = undefined) {
  let domutils = getDOMWindowUtils(window);
  domutils.sendCompositionEvent(ev.type, ev.data || "", ev.locale || "");
};

/**
 * Synthesize a text event.
 *
 * The text event's information, this has |composition| and |caret|
 * members.  |composition| has |string| and |clauses| members. |clauses|
 * must be array object.  Each object has |length| and |attr|.
 * And |caret| has |start| and |length|.  See the following tree image.
 *
 *     ev
 *      +-- composition
 *      |     +-- string
 *      |     +-- clauses[]
 *      |           +-- length
 *      |           +-- attr
 *      +-- caret
 *            +-- start
 *            +-- length
 *
 * Set the composition string to |composition.string|.  Set its clauses
 * information to the |clauses| array.
 *
 * When it's composing, set the each clauses' length
 * to the |composition.clauses[n].length|.  The sum
 * of the all length values must be same as the length of
 * |composition.string|. Set nsIDOMWindowUtils.COMPOSITION_ATTR_* to the
 * |composition.clauses[n].attr|.
 *
 * When it's not composing, set 0 to the |composition.clauses[0].length|
 * and |composition.clauses[0].attr|.
 *
 * Set caret position to the |caret.start|. Its offset from the start of
 * the composition string.  Set caret length to |caret.length|.  If it's
 * larger than 0, it should be wide caret.  However, current nsEditor
 * doesn't support wide caret, therefore, you should always set 0 now.
 *
 * @param {Object.<string, ?>} ev
 *     The text event's information,
 * @param {Window=} window
 *     Window object.  Defaults to the current window.
 */
event.synthesizeText = function(ev, window = undefined) {
  let domutils = getDOMWindowUtils(window);

  if (!ev.composition ||
      !ev.composition.clauses ||
      !ev.composition.clauses[0]) {
    return;
  }

  let firstClauseLength = ev.composition.clauses[0].length;
  let firstClauseAttr   = ev.composition.clauses[0].attr;
  let secondClauseLength = 0;
  let secondClauseAttr = 0;
  let thirdClauseLength = 0;
  let thirdClauseAttr = 0;
  if (ev.composition.clauses[1]) {
    secondClauseLength = ev.composition.clauses[1].length;
    secondClauseAttr   = ev.composition.clauses[1].attr;
    if (event.composition.clauses[2]) {
      thirdClauseLength = ev.composition.clauses[2].length;
      thirdClauseAttr   = ev.composition.clauses[2].attr;
    }
  }

  let caretStart = -1;
  let caretLength = 0;
  if (event.caret) {
    caretStart = ev.caret.start;
    caretLength = ev.caret.length;
  }

  domutils.sendTextEvent(
      ev.composition.string,
      firstClauseLength,
      firstClauseAttr,
      secondClauseLength,
      secondClauseAttr,
      thirdClauseLength,
      thirdClauseAttr,
      caretStart,
      caretLength);
};

/**
 * Synthesize a query selected text event.
 *
 * @param {Window=}
 *     Window object.  Defaults to the current window.
 *
 * @return {(nsIQueryContentEventResult|null)}
 *     Event's result, or null if it failed.
 */
event.synthesizeQuerySelectedText = function(window = undefined) {
  let domutils = getDOMWindowUtils(window);
  return domutils.sendQueryContentEvent(
      domutils.QUERY_SELECTED_TEXT, 0, 0, 0, 0);
};

/**
 * Synthesize a selection set event.
 *
 * @param {number} offset
 *     Character offset.  0 means the first character in the selection
 *     root.
 * @param {number} length
 *     Length of the text.  If the length is too long, the extra length
 *     is ignored.
 * @param {boolean} reverse
 *     If true, the selection is from |aOffset + aLength| to |aOffset|.
 *     Otherwise, from |aOffset| to |aOffset + aLength|.
 * @param {Window=} window
 *     Window object.  Defaults to the current window.
 *
 * @return         True, if succeeded.  Otherwise false.
 */
event.synthesizeSelectionSet = function(
    offset, length, reverse, window = undefined) {
  let domutils = getDOMWindowUtils(window);
  return domutils.sendSelectionSetEvent(offset, length, reverse);
};

const KEYCODES_LOOKUP = {
  "VK_SHIFT": "shiftKey",
  "VK_CONTROL": "ctrlKey",
  "VK_ALT": "altKey",
  "VK_META": "metaKey",
};

const VIRTUAL_KEYCODE_LOOKUP = {
  "\uE001": "VK_CANCEL",
  "\uE002": "VK_HELP",
  "\uE003": "VK_BACK_SPACE",
  "\uE004": "VK_TAB",
  "\uE005": "VK_CLEAR",
  "\uE006": "VK_RETURN",
  "\uE007": "VK_RETURN",
  "\uE008": "VK_SHIFT",
  "\uE009": "VK_CONTROL",
  "\uE00A": "VK_ALT",
  "\uE03D": "VK_META",
  "\uE00B": "VK_PAUSE",
  "\uE00C": "VK_ESCAPE",
  "\uE00D": "VK_SPACE",  // printable
  "\uE00E": "VK_PAGE_UP",
  "\uE00F": "VK_PAGE_DOWN",
  "\uE010": "VK_END",
  "\uE011": "VK_HOME",
  "\uE012": "VK_LEFT",
  "\uE013": "VK_UP",
  "\uE014": "VK_RIGHT",
  "\uE015": "VK_DOWN",
  "\uE016": "VK_INSERT",
  "\uE017": "VK_DELETE",
  "\uE018": "VK_SEMICOLON",
  "\uE019": "VK_EQUALS",
  "\uE01A": "VK_NUMPAD0",
  "\uE01B": "VK_NUMPAD1",
  "\uE01C": "VK_NUMPAD2",
  "\uE01D": "VK_NUMPAD3",
  "\uE01E": "VK_NUMPAD4",
  "\uE01F": "VK_NUMPAD5",
  "\uE020": "VK_NUMPAD6",
  "\uE021": "VK_NUMPAD7",
  "\uE022": "VK_NUMPAD8",
  "\uE023": "VK_NUMPAD9",
  "\uE024": "VK_MULTIPLY",
  "\uE025": "VK_ADD",
  "\uE026": "VK_SEPARATOR",
  "\uE027": "VK_SUBTRACT",
  "\uE028": "VK_DECIMAL",
  "\uE029": "VK_DIVIDE",
  "\uE031": "VK_F1",
  "\uE032": "VK_F2",
  "\uE033": "VK_F3",
  "\uE034": "VK_F4",
  "\uE035": "VK_F5",
  "\uE036": "VK_F6",
  "\uE037": "VK_F7",
  "\uE038": "VK_F8",
  "\uE039": "VK_F9",
  "\uE03A": "VK_F10",
  "\uE03B": "VK_F11",
  "\uE03C": "VK_F12",
};

function getKeyCode(c) {
  if (c in VIRTUAL_KEYCODE_LOOKUP) {
    return VIRTUAL_KEYCODE_LOOKUP[c];
  }
  return c;
}

event.sendKeyDown = function(keyToSend, modifiers, document) {
  modifiers.type = "keydown";
  event.sendSingleKey(keyToSend, modifiers, document);
  if (["VK_SHIFT", "VK_CONTROL", "VK_ALT", "VK_META"].indexOf(getKeyCode(keyToSend)) < 0) {
    modifiers.type = "keypress";
    event.sendSingleKey(keyToSend, modifiers, document);
  }
  delete modifiers.type;
};

event.sendKeyUp = function(keyToSend, modifiers, window = undefined) {
  modifiers.type = "keyup";
  event.sendSingleKey(keyToSend, modifiers, window);
  delete modifiers.type;
};

event.sendSingleKey = function(keyToSend, modifiers, window = undefined) {
  let keyCode = getKeyCode(keyToSend);
  if (keyCode in KEYCODES_LOOKUP) {
    let modName = KEYCODES_LOOKUP[keyCode];
    modifiers[modName] = !modifiers[modName];
  } else if (modifiers.shiftKey) {
    keyCode = keyCode.toUpperCase();
  }
  event.synthesizeKey(keyCode, modifiers, window);
};

/**
 * Focus element and, if a textual input field and no previous selection
 * state exists, move the caret to the end of the input field.
 *
 * @param {Element} element
 *     Element to focus.
 */
function focusElement(element) {
  let t = element.type;
  if (t && (t == "text" || t == "textarea")) {
    if (element.selectionEnd == 0) {
      let len = element.value.length;
      element.setSelectionRange(len, len);
    }
  }
  element.focus();
}

/**
 * @param {Array.<string>} keySequence
 * @param {Element} element
 * @param {Object.<string, boolean>=} opts
 * @param {Window=} window
 */
event.sendKeysToElement = function(
    keySequence, el, opts = {}, window = undefined) {

  if (opts.ignoreVisibility || element.isVisible(el)) {
    focusElement(el);

    // make Object.<modifier, false> map
    let modifiers = Object.create(event.Modifiers);
    for (let modifier in event.Modifiers) {
      modifiers[modifier] = false;
    }

    let value = keySequence.join("");
    for (let i = 0; i < value.length; i++) {
      let c = value.charAt(i);
      event.sendSingleKey(c, modifiers, window);
    }

  } else {
    throw new ElementNotVisibleError("Element is not visible");
  }
};
