/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/* eslint-disable no-restricted-globals */

const EXPORTED_SYMBOLS = ["event"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  keyData: "chrome://remote/content/shared/webdriver/KeyData.jsm",
});

/** Provides functionality for creating and sending DOM events. */
const event = {};

XPCOMUtils.defineLazyGetter(lazy, "dblclickTimer", () => {
  return Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
});

const _eventUtils = new WeakMap();

function _getEventUtils(win) {
  if (!_eventUtils.has(win)) {
    const eventUtilsObject = {
      window: win,
      parent: win,
      _EU_Ci: Ci,
      _EU_Cc: Cc,
    };
    Services.scriptloader.loadSubScript(
      "chrome://remote/content/external/EventUtils.js",
      eventUtilsObject
    );
    _eventUtils.set(win, eventUtilsObject);
  }
  return _eventUtils.get(win);
}

//  Max interval between two clicks that should result in a dblclick (in ms)
const DBLCLICK_INTERVAL = 640;

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

event.MouseButton = {
  isPrimary(button) {
    return button === 0;
  },
  isAuxiliary(button) {
    return button === 1;
  },
  isSecondary(button) {
    return button === 2;
  },
};

event.DoubleClickTracker = {
  firstClick: false,
  isClicked() {
    return event.DoubleClickTracker.firstClick;
  },
  setClick() {
    if (!event.DoubleClickTracker.firstClick) {
      event.DoubleClickTracker.firstClick = true;
      event.DoubleClickTracker.startTimer();
    }
  },
  resetClick() {
    event.DoubleClickTracker.firstClick = false;
    event.DoubleClickTracker.cancelTimer();
  },
  startTimer() {
    lazy.dblclickTimer.initWithCallback(
      event.DoubleClickTracker.resetClick,
      DBLCLICK_INTERVAL,
      Ci.nsITimer.TYPE_ONE_SHOT
    );
  },
  cancelTimer() {
    lazy.dblclickTimer.cancel();
  },
};

// Only used by legacyactions.js
event.parseModifiers_ = function(modifiers, win) {
  return _getEventUtils(win)._parseModifiers(modifiers);
};

/**
 * Synthesise a mouse event at a point.
 *
 * If the type is specified in opts, an mouse event of that type is
 * fired. Otherwise, a mousedown followed by a mouseup is performed.
 *
 * @param {number} left
 *     Offset from viewport left, in CSS pixels
 * @param {number} top
 *     Offset from viewport top, in CSS pixels
 * @param {Object} opts
 *     Object which may contain the properties "shiftKey", "ctrlKey",
 *     "altKey", "metaKey", "accessKey", "clickCount", "button", and
 *     "type".
 * @param {Window} win
 *     Window object.
 */
event.synthesizeMouseAtPoint = function(left, top, opts, win) {
  return _getEventUtils(win).synthesizeMouseAtPoint(left, top, opts, win);
};

/**
 * Synthesize a keydown event for a single key.
 *
 * @param {Object} key
 *     Key data as returned by keyData.getData
 * @param {Window} win
 *     Window object.
 */
event.sendKeyDown = function(key, win) {
  event.sendSingleKey(key, win, "keydown");
};

/**
 * Synthesize a keyup event for a single key.
 *
 * @param {Object} key
 *     Key data as returned by keyData.getData
 * @param {Window} win
 *     Window object.
 */
event.sendKeyUp = function(key, win) {
  event.sendSingleKey(key, win, "keyup");
};

/**
 * Synthesize a key event for a single key.
 *
 * @param {Object} key
 *     Key data as returned by keyData.getData
 * @param {Window} win
 *     Window object.
 * @param {string=} type
 *     Event to emit. By default the full keydown/keypressed/keyup event
 *     sequence is emitted.
 */
event.sendSingleKey = function(key, win, type = null) {
  let keyValue = key.key;
  if (!key.printable) {
    keyValue = `KEY_${keyValue}`;
  }
  const event = {
    code: key.code,
    location: key.location,
    altKey: key.altKey ?? false,
    shiftKey: key.shiftKey ?? false,
    ctrlKey: key.ctrlKey ?? false,
    metaKey: key.metaKey ?? false,
    repeat: key.repeat ?? false,
  };
  if (type) {
    event.type = type;
  }
  _getEventUtils(win).synthesizeKey(keyValue, event, win);
};

/**
 * Send a string as a series of keypresses.
 *
 * @param {string} keyString
 *     Sequence of characters to send as key presses
 * @param {Window} win
 *     Window object
 */
event.sendKeys = function(keyString, win) {
  const modifiers = {};
  for (let modifier in event.Modifiers) {
    modifiers[modifier] = false;
  }

  for (let i = 0; i < keyString.length; i++) {
    let keyValue = keyString.charAt(i);
    if (modifiers.shiftKey) {
      keyValue = lazy.keyData.getShiftedKey(keyValue);
    }
    const data = lazy.keyData.getData(keyValue);
    const key = { ...data, ...modifiers };
    if (data.modifier) {
      modifiers[data.modifier] = true;
    }
    event.sendSingleKey(key, win);
  }
};

event.sendEvent = function(eventType, el, modifiers = {}, opts = {}) {
  opts.canBubble = opts.canBubble || true;

  let doc = el.ownerDocument || el.document;
  let ev = doc.createEvent("Event");

  ev.shiftKey = modifiers.shift;
  ev.metaKey = modifiers.meta;
  ev.altKey = modifiers.alt;
  ev.ctrlKey = modifiers.ctrl;

  ev.initEvent(eventType, opts.canBubble, true);
  el.dispatchEvent(ev);
};

event.mouseover = function(el, modifiers = {}, opts = {}) {
  return event.sendEvent("mouseover", el, modifiers, opts);
};

event.mousemove = function(el, modifiers = {}, opts = {}) {
  return event.sendEvent("mousemove", el, modifiers, opts);
};

event.mousedown = function(el, modifiers = {}, opts = {}) {
  return event.sendEvent("mousedown", el, modifiers, opts);
};

event.mouseup = function(el, modifiers = {}, opts = {}) {
  return event.sendEvent("mouseup", el, modifiers, opts);
};

event.click = function(el, modifiers = {}, opts = {}) {
  return event.sendEvent("click", el, modifiers, opts);
};

event.change = function(el, modifiers = {}, opts = {}) {
  return event.sendEvent("change", el, modifiers, opts);
};

event.input = function(el, modifiers = {}, opts = {}) {
  return event.sendEvent("input", el, modifiers, opts);
};
