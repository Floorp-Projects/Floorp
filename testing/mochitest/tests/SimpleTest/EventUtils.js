/**
 * EventUtils provides some utility methods for creating and sending DOM events.
 * Current methods:
 *  sendMouseEvent
 *  sendDragEvent
 *  sendChar
 *  sendString
 *  sendKey
 *  sendWheelAndPaint
 *  synthesizeMouse
 *  synthesizeMouseAtCenter
 *  synthesizePointer
 *  synthesizeWheel
 *  synthesizeKey
 *  synthesizeNativeKey
 *  synthesizeMouseExpectEvent
 *  synthesizeKeyExpectEvent
 *
 *  When adding methods to this file, please add a performance test for it.
 */

// This file is used both in privileged and unprivileged contexts, so we have to
// be careful about our access to Components.interfaces. We also want to avoid
// naming collisions with anything that might be defined in the scope that imports
// this script.
window.__defineGetter__('_EU_Ci', function() {
  // Even if the real |Components| doesn't exist, we might shim in a simple JS
  // placebo for compat. An easy way to differentiate this from the real thing
  // is whether the property is read-only or not.
  var c = Object.getOwnPropertyDescriptor(window, 'Components');
  return c.value && !c.writable ? Components.interfaces : SpecialPowers.Ci;
});

window.__defineGetter__('_EU_Cc', function() {
  var c = Object.getOwnPropertyDescriptor(window, 'Components');
  return c.value && !c.writable ? Components.classes : SpecialPowers.Cc;
});

/**
 * Send a mouse event to the node aTarget (aTarget can be an id, or an
 * actual node) . The "event" passed in to aEvent is just a JavaScript
 * object with the properties set that the real mouse event object should
 * have. This includes the type of the mouse event.
 * E.g. to send an click event to the node with id 'node' you might do this:
 *
 * sendMouseEvent({type:'click'}, 'node');
 */
function getElement(id) {
  return ((typeof(id) == "string") ?
    document.getElementById(id) : id); 
};   

this.$ = this.getElement;

function sendMouseEvent(aEvent, aTarget, aWindow) {
  if (['click', 'contextmenu', 'dblclick', 'mousedown', 'mouseup', 'mouseover', 'mouseout'].indexOf(aEvent.type) == -1) {
    throw new Error("sendMouseEvent doesn't know about event type '" + aEvent.type + "'");
  }

  if (!aWindow) {
    aWindow = window;
  }

  if (typeof aTarget == "string") {
    aTarget = aWindow.document.getElementById(aTarget);
  }

  var event = aWindow.document.createEvent('MouseEvent');

  var typeArg          = aEvent.type;
  var canBubbleArg     = true;
  var cancelableArg    = true;
  var viewArg          = aWindow;
  var detailArg        = aEvent.detail        || (aEvent.type == 'click'     ||
                                                  aEvent.type == 'mousedown' ||
                                                  aEvent.type == 'mouseup' ? 1 :
                                                  aEvent.type == 'dblclick'? 2 : 0);
  var screenXArg       = aEvent.screenX       || 0;
  var screenYArg       = aEvent.screenY       || 0;
  var clientXArg       = aEvent.clientX       || 0;
  var clientYArg       = aEvent.clientY       || 0;
  var ctrlKeyArg       = aEvent.ctrlKey       || false;
  var altKeyArg        = aEvent.altKey        || false;
  var shiftKeyArg      = aEvent.shiftKey      || false;
  var metaKeyArg       = aEvent.metaKey       || false;
  var buttonArg        = aEvent.button        || (aEvent.type == 'contextmenu' ? 2 : 0);
  var relatedTargetArg = aEvent.relatedTarget || null;

  event.initMouseEvent(typeArg, canBubbleArg, cancelableArg, viewArg, detailArg,
                       screenXArg, screenYArg, clientXArg, clientYArg,
                       ctrlKeyArg, altKeyArg, shiftKeyArg, metaKeyArg,
                       buttonArg, relatedTargetArg);

  return SpecialPowers.dispatchEvent(aWindow, aTarget, event);
}

/**
 * Send a drag event to the node aTarget (aTarget can be an id, or an
 * actual node) . The "event" passed in to aEvent is just a JavaScript
 * object with the properties set that the real drag event object should
 * have. This includes the type of the drag event.
 */
function sendDragEvent(aEvent, aTarget, aWindow=window) {
  if (['drag', 'dragstart', 'dragend', 'dragover', 'dragenter', 'dragleave', 'drop'].indexOf(aEvent.type) == -1) {
    throw new Error("sendDragEvent doesn't know about event type '" + aEvent.type + "'");
  }

  if (typeof aTarget == "string") {
    aTarget = aWindow.document.getElementById(aTarget);
  }

  var event = aWindow.document.createEvent('DragEvent');

  var typeArg          = aEvent.type;
  var canBubbleArg     = true;
  var cancelableArg    = true;
  var viewArg          = aWindow;
  var detailArg        = aEvent.detail        || 0;
  var screenXArg       = aEvent.screenX       || 0;
  var screenYArg       = aEvent.screenY       || 0;
  var clientXArg       = aEvent.clientX       || 0;
  var clientYArg       = aEvent.clientY       || 0;
  var ctrlKeyArg       = aEvent.ctrlKey       || false;
  var altKeyArg        = aEvent.altKey        || false;
  var shiftKeyArg      = aEvent.shiftKey      || false;
  var metaKeyArg       = aEvent.metaKey       || false;
  var buttonArg        = aEvent.button        || 0;
  var relatedTargetArg = aEvent.relatedTarget || null;
  var dataTransfer     = aEvent.dataTransfer  || null;

  event.initDragEvent(typeArg, canBubbleArg, cancelableArg, viewArg, detailArg,
                      screenXArg, screenYArg, clientXArg, clientYArg,
                      ctrlKeyArg, altKeyArg, shiftKeyArg, metaKeyArg,
                      buttonArg, relatedTargetArg, dataTransfer);

  var utils = _getDOMWindowUtils(aWindow);
  return utils.dispatchDOMEventViaPresShell(aTarget, event, true);
}

/**
 * Send the char aChar to the focused element.  This method handles casing of
 * chars (sends the right charcode, and sends a shift key for uppercase chars).
 * No other modifiers are handled at this point.
 *
 * For now this method only works for ASCII characters and emulates the shift
 * key state on US keyboard layout.
 */
function sendChar(aChar, aWindow) {
  var hasShift;
  // Emulate US keyboard layout for the shiftKey state.
  switch (aChar) {
    case "!":
    case "@":
    case "#":
    case "$":
    case "%":
    case "^":
    case "&":
    case "*":
    case "(":
    case ")":
    case "_":
    case "+":
    case "{":
    case "}":
    case ":":
    case "\"":
    case "|":
    case "<":
    case ">":
    case "?":
      hasShift = true;
      break;
    default:
      hasShift = (aChar == aChar.toUpperCase());
      break;
  }
  synthesizeKey(aChar, { shiftKey: hasShift }, aWindow);
}

/**
 * Send the string aStr to the focused element.
 *
 * For now this method only works for ASCII characters and emulates the shift
 * key state on US keyboard layout.
 */
function sendString(aStr, aWindow) {
  for (var i = 0; i < aStr.length; ++i) {
    sendChar(aStr.charAt(i), aWindow);
  }
}

/**
 * Send the non-character key aKey to the focused node.
 * The name of the key should be the part that comes after "DOM_VK_" in the
 *   KeyEvent constant name for this key.
 * No modifiers are handled at this point.
 */
function sendKey(aKey, aWindow) {
  var keyName = "VK_" + aKey.toUpperCase();
  synthesizeKey(keyName, { shiftKey: false }, aWindow);
}

/**
 * Parse the key modifier flags from aEvent. Used to share code between
 * synthesizeMouse and synthesizeKey.
 */
function _parseModifiers(aEvent)
{
  const nsIDOMWindowUtils = _EU_Ci.nsIDOMWindowUtils;
  var mval = 0;
  if (aEvent.shiftKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_SHIFT;
  }
  if (aEvent.ctrlKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_CONTROL;
  }
  if (aEvent.altKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_ALT;
  }
  if (aEvent.metaKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_META;
  }
  if (aEvent.accelKey) {
    mval |= (navigator.platform.indexOf("Mac") >= 0) ?
      nsIDOMWindowUtils.MODIFIER_META : nsIDOMWindowUtils.MODIFIER_CONTROL;
  }
  if (aEvent.altGrKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_ALTGRAPH;
  }
  if (aEvent.capsLockKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_CAPSLOCK;
  }
  if (aEvent.fnKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_FN;
  }
  if (aEvent.fnLockKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_FNLOCK;
  }
  if (aEvent.numLockKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_NUMLOCK;
  }
  if (aEvent.scrollLockKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_SCROLLLOCK;
  }
  if (aEvent.symbolKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_SYMBOL;
  }
  if (aEvent.symbolLockKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_SYMBOLLOCK;
  }
  if (aEvent.osKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_OS;
  }

  return mval;
}

/**
 * Synthesize a mouse event on a target. The actual client point is determined
 * by taking the aTarget's client box and offseting it by aOffsetX and
 * aOffsetY. This allows mouse clicks to be simulated by calling this method.
 *
 * aEvent is an object which may contain the properties:
 *   shiftKey, ctrlKey, altKey, metaKey, accessKey, clickCount, button, type
 *
 * If the type is specified, an mouse event of that type is fired. Otherwise,
 * a mousedown followed by a mouse up is performed.
 *
 * aWindow is optional, and defaults to the current window object.
 *
 * Returns whether the event had preventDefault() called on it.
 */
function synthesizeMouse(aTarget, aOffsetX, aOffsetY, aEvent, aWindow)
{
  var rect = aTarget.getBoundingClientRect();
  return synthesizeMouseAtPoint(rect.left + aOffsetX, rect.top + aOffsetY,
       aEvent, aWindow);
}
function synthesizeTouch(aTarget, aOffsetX, aOffsetY, aEvent, aWindow)
{
  var rect = aTarget.getBoundingClientRect();
  synthesizeTouchAtPoint(rect.left + aOffsetX, rect.top + aOffsetY,
       aEvent, aWindow);
}
function synthesizePointer(aTarget, aOffsetX, aOffsetY, aEvent, aWindow)
{
  var rect = aTarget.getBoundingClientRect();
  return synthesizePointerAtPoint(rect.left + aOffsetX, rect.top + aOffsetY,
       aEvent, aWindow);
}

/*
 * Synthesize a mouse event at a particular point in aWindow.
 *
 * aEvent is an object which may contain the properties:
 *   shiftKey, ctrlKey, altKey, metaKey, accessKey, clickCount, button, type
 *
 * If the type is specified, an mouse event of that type is fired. Otherwise,
 * a mousedown followed by a mouse up is performed.
 *
 * aWindow is optional, and defaults to the current window object.
 */
function synthesizeMouseAtPoint(left, top, aEvent, aWindow)
{
  var utils = _getDOMWindowUtils(aWindow);
  var defaultPrevented = false;

  if (utils) {
    var button = aEvent.button || 0;
    var clickCount = aEvent.clickCount || 1;
    var modifiers = _parseModifiers(aEvent);
    var pressure = ("pressure" in aEvent) ? aEvent.pressure : 0;
    var inputSource = ("inputSource" in aEvent) ? aEvent.inputSource : 0;
    var synthesized = ("isSynthesized" in aEvent) ? aEvent.isSynthesized : true;

    if (("type" in aEvent) && aEvent.type) {
      defaultPrevented = utils.sendMouseEvent(aEvent.type, left, top, button,
                                              clickCount, modifiers, false,
                                              pressure, inputSource,
                                              synthesized);
    }
    else {
      utils.sendMouseEvent("mousedown", left, top, button, clickCount, modifiers, false, pressure, inputSource);
      utils.sendMouseEvent("mouseup", left, top, button, clickCount, modifiers, false, pressure, inputSource);
    }
  }

  return defaultPrevented;
}
function synthesizeTouchAtPoint(left, top, aEvent, aWindow)
{
  var utils = _getDOMWindowUtils(aWindow);

  if (utils) {
    var id = aEvent.id || 0;
    var rx = aEvent.rx || 1;
    var ry = aEvent.rx || 1;
    var angle = aEvent.angle || 0;
    var force = aEvent.force || 1;
    var modifiers = _parseModifiers(aEvent);

    if (("type" in aEvent) && aEvent.type) {
      utils.sendTouchEvent(aEvent.type, [id], [left], [top], [rx], [ry], [angle], [force], 1, modifiers);
    }
    else {
      utils.sendTouchEvent("touchstart", [id], [left], [top], [rx], [ry], [angle], [force], 1, modifiers);
      utils.sendTouchEvent("touchend", [id], [left], [top], [rx], [ry], [angle], [force], 1, modifiers);
    }
  }
}
function synthesizePointerAtPoint(left, top, aEvent, aWindow)
{
  var utils = _getDOMWindowUtils(aWindow);
  var defaultPrevented = false;

  if (utils) {
    var button = aEvent.button || 0;
    var clickCount = aEvent.clickCount || 1;
    var modifiers = _parseModifiers(aEvent);
    var pressure = ("pressure" in aEvent) ? aEvent.pressure : 0;
    var inputSource = ("inputSource" in aEvent) ? aEvent.inputSource : 0;
    var synthesized = ("isSynthesized" in aEvent) ? aEvent.isSynthesized : true;
    var isPrimary = ("isPrimary" in aEvent) ? aEvent.isPrimary : false;

    if (("type" in aEvent) && aEvent.type) {
      defaultPrevented = utils.sendPointerEventToWindow(aEvent.type, left, top, button,
                                                        clickCount, modifiers, false,
                                                        pressure, inputSource,
                                                        synthesized, 0, 0, 0, 0, isPrimary);
    }
    else {
      utils.sendPointerEventToWindow("pointerdown", left, top, button, clickCount, modifiers, false, pressure, inputSource);
      utils.sendPointerEventToWindow("pointerup", left, top, button, clickCount, modifiers, false, pressure, inputSource);
    }
  }

  return defaultPrevented;
}

// Call synthesizeMouse with coordinates at the center of aTarget.
function synthesizeMouseAtCenter(aTarget, aEvent, aWindow)
{
  var rect = aTarget.getBoundingClientRect();
  synthesizeMouse(aTarget, rect.width / 2, rect.height / 2, aEvent,
                  aWindow);
}
function synthesizeTouchAtCenter(aTarget, aEvent, aWindow)
{
  var rect = aTarget.getBoundingClientRect();
  synthesizeTouch(aTarget, rect.width / 2, rect.height / 2, aEvent,
                  aWindow);
}

/**
 * Synthesize a wheel event on a target. The actual client point is determined
 * by taking the aTarget's client box and offseting it by aOffsetX and
 * aOffsetY.
 *
 * aEvent is an object which may contain the properties:
 *   shiftKey, ctrlKey, altKey, metaKey, accessKey, deltaX, deltaY, deltaZ,
 *   deltaMode, lineOrPageDeltaX, lineOrPageDeltaY, isMomentum,
 *   isNoLineOrPageDelta, isCustomizedByPrefs, expectedOverflowDeltaX,
 *   expectedOverflowDeltaY
 *
 * deltaMode must be defined, others are ok even if undefined.
 *
 * expectedOverflowDeltaX and expectedOverflowDeltaY take integer value.  The
 * value is just checked as 0 or positive or negative.
 *
 * aWindow is optional, and defaults to the current window object.
 */
function synthesizeWheel(aTarget, aOffsetX, aOffsetY, aEvent, aWindow)
{
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return;
  }

  var modifiers = _parseModifiers(aEvent);
  var options = 0;
  if (aEvent.isNoLineOrPageDelta) {
    options |= utils.WHEEL_EVENT_CAUSED_BY_NO_LINE_OR_PAGE_DELTA_DEVICE;
  }
  if (aEvent.isMomentum) {
    options |= utils.WHEEL_EVENT_CAUSED_BY_MOMENTUM;
  }
  if (aEvent.isCustomizedByPrefs) {
    options |= utils.WHEEL_EVENT_CUSTOMIZED_BY_USER_PREFS;
  }
  if (typeof aEvent.expectedOverflowDeltaX !== "undefined") {
    if (aEvent.expectedOverflowDeltaX === 0) {
      options |= utils.WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_X_ZERO;
    } else if (aEvent.expectedOverflowDeltaX > 0) {
      options |= utils.WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_X_POSITIVE;
    } else {
      options |= utils.WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_X_NEGATIVE;
    }
  }
  if (typeof aEvent.expectedOverflowDeltaY !== "undefined") {
    if (aEvent.expectedOverflowDeltaY === 0) {
      options |= utils.WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_Y_ZERO;
    } else if (aEvent.expectedOverflowDeltaY > 0) {
      options |= utils.WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_Y_POSITIVE;
    } else {
      options |= utils.WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_Y_NEGATIVE;
    }
  }
  var isNoLineOrPageDelta = aEvent.isNoLineOrPageDelta;

  // Avoid the JS warnings "reference to undefined property"
  if (!aEvent.deltaX) {
    aEvent.deltaX = 0;
  }
  if (!aEvent.deltaY) {
    aEvent.deltaY = 0;
  }
  if (!aEvent.deltaZ) {
    aEvent.deltaZ = 0;
  }

  var lineOrPageDeltaX =
    aEvent.lineOrPageDeltaX != null ? aEvent.lineOrPageDeltaX :
                  aEvent.deltaX > 0 ? Math.floor(aEvent.deltaX) :
                                      Math.ceil(aEvent.deltaX);
  var lineOrPageDeltaY =
    aEvent.lineOrPageDeltaY != null ? aEvent.lineOrPageDeltaY :
                  aEvent.deltaY > 0 ? Math.floor(aEvent.deltaY) :
                                      Math.ceil(aEvent.deltaY);

  var rect = aTarget.getBoundingClientRect();
  utils.sendWheelEvent(rect.left + aOffsetX, rect.top + aOffsetY,
                       aEvent.deltaX, aEvent.deltaY, aEvent.deltaZ,
                       aEvent.deltaMode, modifiers,
                       lineOrPageDeltaX, lineOrPageDeltaY, options);
}

/**
 * This is a wrapper around synthesizeWheel that waits for the wheel event
 * to be dispatched and for the subsequent layout/paints to be flushed.
 *
 * This requires including paint_listener.js. Tests must call
 * DOMWindowUtils.restoreNormalRefresh() before finishing, if they use this
 * function.
 */
function sendWheelAndPaint(aTarget, aOffsetX, aOffsetY, aEvent, aCallback, aWindow) {
  aWindow = aWindow || window;

  var utils = _getDOMWindowUtils(aWindow);
  if (!utils)
    return;

  if (utils.isMozAfterPaintPending) {
    // If a paint is pending, then APZ may be waiting for a scroll acknowledgement
    // from the content thread. If we send a wheel event now, it could be ignored
    // by APZ (or its scroll offset could be overridden). To avoid problems we
    // just wait for the paint to complete.
    aWindow.waitForAllPaintsFlushed(function() {
      sendWheelAndPaint(aTarget, aOffsetX, aOffsetY, aEvent, aCallback, aWindow);
    });
    return;
  }

  var onwheel = function() {
    window.removeEventListener("wheel", onwheel);

    // Wait one frame since the wheel event has not caused a refresh observer
    // to be added yet.
    setTimeout(function() {
      utils.advanceTimeAndRefresh(1000);
      aWindow.waitForAllPaintsFlushed(function() {
        utils.restoreNormalRefresh();
        aCallback();
      });
    }, 0);
  };

  aWindow.addEventListener("wheel", onwheel);
  synthesizeWheel(aTarget, aOffsetX, aOffsetY, aEvent, aWindow);
}

function _computeKeyCodeFromChar(aChar)
{
  if (aChar.length != 1) {
    return 0;
  }
  const nsIDOMKeyEvent = _EU_Ci.nsIDOMKeyEvent;
  if (aChar >= 'a' && aChar <= 'z') {
    return nsIDOMKeyEvent.DOM_VK_A + aChar.charCodeAt(0) - 'a'.charCodeAt(0);
  }
  if (aChar >= 'A' && aChar <= 'Z') {
    return nsIDOMKeyEvent.DOM_VK_A + aChar.charCodeAt(0) - 'A'.charCodeAt(0);
  }
  if (aChar >= '0' && aChar <= '9') {
    return nsIDOMKeyEvent.DOM_VK_0 + aChar.charCodeAt(0) - '0'.charCodeAt(0);
  }
  // returns US keyboard layout's keycode
  switch (aChar) {
    case '~':
    case '`':
      return nsIDOMKeyEvent.DOM_VK_BACK_QUOTE;
    case '!':
      return nsIDOMKeyEvent.DOM_VK_1;
    case '@':
      return nsIDOMKeyEvent.DOM_VK_2;
    case '#':
      return nsIDOMKeyEvent.DOM_VK_3;
    case '$':
      return nsIDOMKeyEvent.DOM_VK_4;
    case '%':
      return nsIDOMKeyEvent.DOM_VK_5;
    case '^':
      return nsIDOMKeyEvent.DOM_VK_6;
    case '&':
      return nsIDOMKeyEvent.DOM_VK_7;
    case '*':
      return nsIDOMKeyEvent.DOM_VK_8;
    case '(':
      return nsIDOMKeyEvent.DOM_VK_9;
    case ')':
      return nsIDOMKeyEvent.DOM_VK_0;
    case '-':
    case '_':
      return nsIDOMKeyEvent.DOM_VK_SUBTRACT;
    case '+':
    case '=':
      return nsIDOMKeyEvent.DOM_VK_EQUALS;
    case '{':
    case '[':
      return nsIDOMKeyEvent.DOM_VK_OPEN_BRACKET;
    case '}':
    case ']':
      return nsIDOMKeyEvent.DOM_VK_CLOSE_BRACKET;
    case '|':
    case '\\':
      return nsIDOMKeyEvent.DOM_VK_BACK_SLASH;
    case ':':
    case ';':
      return nsIDOMKeyEvent.DOM_VK_SEMICOLON;
    case '\'':
    case '"':
      return nsIDOMKeyEvent.DOM_VK_QUOTE;
    case '<':
    case ',':
      return nsIDOMKeyEvent.DOM_VK_COMMA;
    case '>':
    case '.':
      return nsIDOMKeyEvent.DOM_VK_PERIOD;
    case '?':
    case '/':
      return nsIDOMKeyEvent.DOM_VK_SLASH;
    case '\n':
      return nsIDOMKeyEvent.DOM_VK_RETURN;
    case ' ':
      return nsIDOMKeyEvent.DOM_VK_SPACE;
    default:
      return 0;
  }
}

/**
 * Synthesize a key event. It is targeted at whatever would be targeted by an
 * actual keypress by the user, typically the focused element.
 *
 * aKey should be:
 *  - key value (recommended).  If you specify a non-printable key name,
 *    append "KEY_" prefix.  Otherwise, specifying a printable key, the
 *    key value should be specified.
 *  - keyCode name starting with "VK_" (e.g., VK_RETURN).  This is available
 *    only for compatibility with legacy API.  Don't use this with new tests.
 *
 * aEvent is an object which may contain the properties:
 *  - code: If you emulates a physical keyboard's key event, this should be
 *          specified.
 *  - repeat: If you emulates auto-repeat, you should set the count of repeat.
 *            This method will automatically synthesize keydown (and keypress).
 *  - location: If you want to specify this, you can specify this explicitly.
 *              However, if you don't specify this value, it will be computed
 *              from code value.
 *  - type: Basically, you shouldn't specify this.  Then, this function will
 *          synthesize keydown (, keypress) and keyup.
 *          If keydown is specified, this only fires keydown (and keypress if
 *          it should be fired).
 *          If keyup is specified, this only fires keyup.
 *  - altKey, altGraphKey, ctrlKey, capsLockKey, fnKey, fnLockKey, numLockKey,
 *    metaKey, osKey, scrollLockKey, shiftKey, symbolKey, symbolLockKey:
 *        Basically, you shouldn't use these attributes.  nsITextInputProcessor
 *        manages modifier key state when you synthesize modifier key events.
 *        However, if some of these attributes are true, this function activates
 *        the modifiers only during dispatching the key events.
 *        Note that if some of these values are false, they are ignored (i.e.,
 *        not inactivated with this function).
 *  - keyCode: Must be 0 - 255 (0xFF). If this is specified explicitly,
 *             .keyCode value is initialized with this value.
 *
 * aWindow is optional, and defaults to the current window object.
 */
function synthesizeKey(aKey, aEvent, aWindow)
{
  var TIP = _getTIP(aWindow);
  if (!TIP) {
    return;
  }
  var modifiers = _emulateToActivateModifiers(TIP, aEvent);
  var keyEventDict = _createKeyboardEventDictionary(aKey, aEvent);
  var keyEvent = new KeyboardEvent("", keyEventDict.dictionary);
  var dispatchKeydown =
    !("type" in aEvent) || aEvent.type === "keydown" || !aEvent.type;
  var dispatchKeyup =
    !("type" in aEvent) || aEvent.type === "keyup"   || !aEvent.type;

  try {
    if (dispatchKeydown) {
      TIP.keydown(keyEvent, keyEventDict.flags);
      if ("repeat" in aEvent && aEvent.repeat > 1) {
        keyEventDict.dictionary.repeat = true;
        var repeatedKeyEvent = new KeyboardEvent("", keyEventDict.dictionary);
        for (var i = 1; i < aEvent.repeat; i++) {
          TIP.keydown(repeatedKeyEvent, keyEventDict.flags);
        }
      }
    }
    if (dispatchKeyup) {
      TIP.keyup(keyEvent, keyEventDict.flags);
    }
  } finally {
    _emulateToInactivateModifiers(TIP, modifiers);
  }
}

function _parseNativeModifiers(aModifiers)
{
  var modifiers;
  if (aModifiers.capsLockKey) {
    modifiers |= 0x00000001;
  }
  if (aModifiers.numLockKey) {
    modifiers |= 0x00000002;
  }
  if (aModifiers.shiftKey) {
    modifiers |= 0x00000100;
  }
  if (aModifiers.shiftRightKey) {
    modifiers |= 0x00000200;
  }
  if (aModifiers.ctrlKey) {
    modifiers |= 0x00000400;
  }
  if (aModifiers.ctrlRightKey) {
    modifiers |= 0x00000800;
  }
  if (aModifiers.altKey) {
    modifiers |= 0x00001000;
  }
  if (aModifiers.altRightKey) {
    modifiers |= 0x00002000;
  }
  if (aModifiers.metaKey) {
    modifiers |= 0x00004000;
  }
  if (aModifiers.metaRightKey) {
    modifiers |= 0x00008000;
  }
  if (aModifiers.helpKey) {
    modifiers |= 0x00010000;
  }
  if (aModifiers.fnKey) {
    modifiers |= 0x00100000;
  }
  if (aModifiers.numericKeyPadKey) {
    modifiers |= 0x01000000;
  }

  if (aModifiers.accelKey) {
    modifiers |=
      (navigator.platform.indexOf("Mac") == 0) ? 0x00004000 : 0x00000400;
  }
  if (aModifiers.accelRightKey) {
    modifiers |=
      (navigator.platform.indexOf("Mac") == 0) ? 0x00008000 : 0x00000800;
  }
  if (aModifiers.altGrKey) {
    modifiers |=
      (navigator.platform.indexOf("Win") == 0) ? 0x00002800 : 0x00001000;
  }
  return modifiers;
}

// Mac: Any unused number is okay for adding new keyboard layout.
//      When you add new keyboard layout here, you need to modify
//      TISInputSourceWrapper::InitByLayoutID().
// Win: These constants can be found by inspecting registry keys under
//      HKEY_LOCAL_MACHINE\System\CurrentControlSet\Control\Keyboard Layouts

const KEYBOARD_LAYOUT_ARABIC =
  { name: "Arabic",             Mac: 6,    Win: 0x00000401 };
const KEYBOARD_LAYOUT_BRAZILIAN_ABNT =
  { name: "Brazilian ABNT",     Mac: null, Win: 0x00000416 };
const KEYBOARD_LAYOUT_DVORAK_QWERTY =
  { name: "Dvorak-QWERTY",      Mac: 4,    Win: null       };
const KEYBOARD_LAYOUT_EN_US =
  { name: "US",                 Mac: 0,    Win: 0x00000409 };
const KEYBOARD_LAYOUT_FRENCH =
  { name: "French",             Mac: 7,    Win: 0x0000040C };
const KEYBOARD_LAYOUT_GREEK =
  { name: "Greek",              Mac: 1,    Win: 0x00000408 };
const KEYBOARD_LAYOUT_GERMAN =
  { name: "German",             Mac: 2,    Win: 0x00000407 };
const KEYBOARD_LAYOUT_HEBREW =
  { name: "Hebrew",             Mac: 8,    Win: 0x0000040D };
const KEYBOARD_LAYOUT_JAPANESE =
  { name: "Japanese",           Mac: null, Win: 0x00000411 };
const KEYBOARD_LAYOUT_LITHUANIAN =
  { name: "Lithuanian",         Mac: 9,    Win: 0x00010427 };
const KEYBOARD_LAYOUT_NORWEGIAN =
  { name: "Norwegian",          Mac: 10,   Win: 0x00000414 };
const KEYBOARD_LAYOUT_SPANISH =
  { name: "Spanish",            Mac: 11,   Win: 0x0000040A };
const KEYBOARD_LAYOUT_SWEDISH =
  { name: "Swedish",            Mac: 3,    Win: 0x0000041D };
const KEYBOARD_LAYOUT_THAI =
  { name: "Thai",               Mac: 5,    Win: 0x0002041E };

/**
 * synthesizeNativeKey() dispatches native key event on active window.
 * This is implemented only on Windows and Mac. Note that this function
 * dispatches the key event asynchronously and returns immediately. If a
 * callback function is provided, the callback will be called upon
 * completion of the key dispatch.
 *
 * @param aKeyboardLayout       One of KEYBOARD_LAYOUT_* defined above.
 * @param aNativeKeyCode        A native keycode value defined in
 *                              NativeKeyCodes.js.
 * @param aModifiers            Modifier keys.  If no modifire key is pressed,
 *                              this must be {}.  Otherwise, one or more items
 *                              referred in _parseNativeModifiers() must be
 *                              true.
 * @param aChars                Specify characters which should be generated
 *                              by the key event.
 * @param aUnmodifiedChars      Specify characters of unmodified (except Shift)
 *                              aChar value.
 * @param aCallback             If provided, this callback will be invoked
 *                              once the native keys have been processed
 *                              by Gecko. Will never be called if this
 *                              function returns false.
 * @return                      True if this function succeed dispatching
 *                              native key event.  Otherwise, false.
 */

function synthesizeNativeKey(aKeyboardLayout, aNativeKeyCode, aModifiers,
                             aChars, aUnmodifiedChars, aCallback)
{
  var utils = _getDOMWindowUtils(window);
  if (!utils) {
    return false;
  }
  var nativeKeyboardLayout = null;
  if (navigator.platform.indexOf("Mac") == 0) {
    nativeKeyboardLayout = aKeyboardLayout.Mac;
  } else if (navigator.platform.indexOf("Win") == 0) {
    nativeKeyboardLayout = aKeyboardLayout.Win;
  }
  if (nativeKeyboardLayout === null) {
    return false;
  }

  var observer = {
    observe: function(aSubject, aTopic, aData) {
      if (aCallback && aTopic == "keyevent") {
        aCallback(aData);
      }
    }
  };
  utils.sendNativeKeyEvent(nativeKeyboardLayout, aNativeKeyCode,
                           _parseNativeModifiers(aModifiers),
                           aChars, aUnmodifiedChars, observer);
  return true;
}

var _gSeenEvent = false;

/**
 * Indicate that an event with an original target of aExpectedTarget and
 * a type of aExpectedEvent is expected to be fired, or not expected to
 * be fired.
 */
function _expectEvent(aExpectedTarget, aExpectedEvent, aTestName)
{
  if (!aExpectedTarget || !aExpectedEvent)
    return null;

  _gSeenEvent = false;

  var type = (aExpectedEvent.charAt(0) == "!") ?
             aExpectedEvent.substring(1) : aExpectedEvent;
  var eventHandler = function(event) {
    var epassed = (!_gSeenEvent && event.originalTarget == aExpectedTarget &&
                   event.type == type);
    is(epassed, true, aTestName + " " + type + " event target " + (_gSeenEvent ? "twice" : ""));
    _gSeenEvent = true;
  };

  aExpectedTarget.addEventListener(type, eventHandler, false);
  return eventHandler;
}

/**
 * Check if the event was fired or not. The event handler aEventHandler
 * will be removed.
 */
function _checkExpectedEvent(aExpectedTarget, aExpectedEvent, aEventHandler, aTestName)
{
  if (aEventHandler) {
    var expectEvent = (aExpectedEvent.charAt(0) != "!");
    var type = expectEvent ? aExpectedEvent : aExpectedEvent.substring(1);
    aExpectedTarget.removeEventListener(type, aEventHandler, false);
    var desc = type + " event";
    if (!expectEvent)
      desc += " not";
    is(_gSeenEvent, expectEvent, aTestName + " " + desc + " fired");
  }

  _gSeenEvent = false;
}

/**
 * Similar to synthesizeMouse except that a test is performed to see if an
 * event is fired at the right target as a result.
 *
 * aExpectedTarget - the expected originalTarget of the event.
 * aExpectedEvent - the expected type of the event, such as 'select'.
 * aTestName - the test name when outputing results
 *
 * To test that an event is not fired, use an expected type preceded by an
 * exclamation mark, such as '!select'. This might be used to test that a
 * click on a disabled element doesn't fire certain events for instance.
 *
 * aWindow is optional, and defaults to the current window object.
 */
function synthesizeMouseExpectEvent(aTarget, aOffsetX, aOffsetY, aEvent,
                                    aExpectedTarget, aExpectedEvent, aTestName,
                                    aWindow)
{
  var eventHandler = _expectEvent(aExpectedTarget, aExpectedEvent, aTestName);
  synthesizeMouse(aTarget, aOffsetX, aOffsetY, aEvent, aWindow);
  _checkExpectedEvent(aExpectedTarget, aExpectedEvent, eventHandler, aTestName);
}

/**
 * Similar to synthesizeKey except that a test is performed to see if an
 * event is fired at the right target as a result.
 *
 * aExpectedTarget - the expected originalTarget of the event.
 * aExpectedEvent - the expected type of the event, such as 'select'.
 * aTestName - the test name when outputing results
 *
 * To test that an event is not fired, use an expected type preceded by an
 * exclamation mark, such as '!select'.
 *
 * aWindow is optional, and defaults to the current window object.
 */
function synthesizeKeyExpectEvent(key, aEvent, aExpectedTarget, aExpectedEvent,
                                  aTestName, aWindow)
{
  var eventHandler = _expectEvent(aExpectedTarget, aExpectedEvent, aTestName);
  synthesizeKey(key, aEvent, aWindow);
  _checkExpectedEvent(aExpectedTarget, aExpectedEvent, eventHandler, aTestName);
}

function disableNonTestMouseEvents(aDisable)
{
  var domutils = _getDOMWindowUtils();
  domutils.disableNonTestMouseEvents(aDisable);
}

function _getDOMWindowUtils(aWindow)
{
  if (!aWindow) {
    aWindow = window;
  }

  // we need parent.SpecialPowers for:
  //  layout/base/tests/test_reftests_with_caret.html
  //  chrome: toolkit/content/tests/chrome/test_findbar.xul
  //  chrome: toolkit/content/tests/chrome/test_popup_anchor.xul
  if ("SpecialPowers" in window && window.SpecialPowers != undefined) {
    return SpecialPowers.getDOMWindowUtils(aWindow);
  }
  if ("SpecialPowers" in parent && parent.SpecialPowers != undefined) {
    return parent.SpecialPowers.getDOMWindowUtils(aWindow);
  }

  //TODO: this is assuming we are in chrome space
  return aWindow.QueryInterface(_EU_Ci.nsIInterfaceRequestor).
                               getInterface(_EU_Ci.nsIDOMWindowUtils);
}

const COMPOSITION_ATTR_RAW_CLAUSE =
  _EU_Ci.nsITextInputProcessor.ATTR_RAW_CLAUSE;
const COMPOSITION_ATTR_SELECTED_RAW_CLAUSE =
  _EU_Ci.nsITextInputProcessor.ATTR_SELECTED_RAW_CLAUSE;
const COMPOSITION_ATTR_CONVERTED_CLAUSE =
  _EU_Ci.nsITextInputProcessor.ATTR_CONVERTED_CLAUSE;
const COMPOSITION_ATTR_SELECTED_CLAUSE =
  _EU_Ci.nsITextInputProcessor.ATTR_SELECTED_CLAUSE;

var TIPMap = new WeakMap();

function _getTIP(aWindow, aCallback)
{
  if (!aWindow) {
    aWindow = window;
  }
  var tip;
  if (TIPMap.has(aWindow)) {
    tip = TIPMap.get(aWindow);
  } else {
    tip =
      _EU_Cc["@mozilla.org/text-input-processor;1"].
        createInstance(_EU_Ci.nsITextInputProcessor);
    TIPMap.set(aWindow, tip);
  }
  if (!tip.beginInputTransactionForTests(aWindow, aCallback)) {
    tip = null;
    TIPMap.delete(aWindow);
  }
  return tip;
}

function _guessKeyNameFromKeyCode(aKeyCode)
{
  switch (aKeyCode) {
    case KeyboardEvent.DOM_VK_CANCEL:
      return "Cancel";
    case KeyboardEvent.DOM_VK_HELP:
      return "Help";
    case KeyboardEvent.DOM_VK_BACK_SPACE:
      return "Backspace";
    case KeyboardEvent.DOM_VK_TAB:
      return "Tab";
    case KeyboardEvent.DOM_VK_CLEAR:
      return "Clear";
    case KeyboardEvent.DOM_VK_RETURN:
      return "Enter";
    case KeyboardEvent.DOM_VK_SHIFT:
      return "Shift";
    case KeyboardEvent.DOM_VK_CONTROL:
      return "Control";
    case KeyboardEvent.DOM_VK_ALT:
      return "Alt";
    case KeyboardEvent.DOM_VK_PAUSE:
      return "Pause";
    case KeyboardEvent.DOM_VK_EISU:
      return "Eisu";
    case KeyboardEvent.DOM_VK_ESCAPE:
      return "Escape";
    case KeyboardEvent.DOM_VK_CONVERT:
      return "Convert";
    case KeyboardEvent.DOM_VK_NONCONVERT:
      return "NonConvert";
    case KeyboardEvent.DOM_VK_ACCEPT:
      return "Accept";
    case KeyboardEvent.DOM_VK_MODECHANGE:
      return "ModeChange";
    case KeyboardEvent.DOM_VK_PAGE_UP:
      return "PageUp";
    case KeyboardEvent.DOM_VK_PAGE_DOWN:
      return "PageDown";
    case KeyboardEvent.DOM_VK_END:
      return "End";
    case KeyboardEvent.DOM_VK_HOME:
      return "Home";
    case KeyboardEvent.DOM_VK_LEFT:
      return "ArrowLeft";
    case KeyboardEvent.DOM_VK_UP:
      return "ArrowUp";
    case KeyboardEvent.DOM_VK_RIGHT:
      return "ArrowRight";
    case KeyboardEvent.DOM_VK_DOWN:
      return "ArrowDown";
    case KeyboardEvent.DOM_VK_SELECT:
      return "Select";
    case KeyboardEvent.DOM_VK_PRINT:
      return "Print";
    case KeyboardEvent.DOM_VK_EXECUTE:
      return "Execute";
    case KeyboardEvent.DOM_VK_PRINTSCREEN:
      return "PrintScreen";
    case KeyboardEvent.DOM_VK_INSERT:
      return "Insert";
    case KeyboardEvent.DOM_VK_DELETE:
      return "Delete";
    case KeyboardEvent.DOM_VK_WIN:
      return "OS";
    case KeyboardEvent.DOM_VK_CONTEXT_MENU:
      return "ContextMenu";
    case KeyboardEvent.DOM_VK_SLEEP:
      return "Standby";
    case KeyboardEvent.DOM_VK_F1:
      return "F1";
    case KeyboardEvent.DOM_VK_F2:
      return "F2";
    case KeyboardEvent.DOM_VK_F3:
      return "F3";
    case KeyboardEvent.DOM_VK_F4:
      return "F4";
    case KeyboardEvent.DOM_VK_F5:
      return "F5";
    case KeyboardEvent.DOM_VK_F6:
      return "F6";
    case KeyboardEvent.DOM_VK_F7:
      return "F7";
    case KeyboardEvent.DOM_VK_F8:
      return "F8";
    case KeyboardEvent.DOM_VK_F9:
      return "F9";
    case KeyboardEvent.DOM_VK_F10:
      return "F10";
    case KeyboardEvent.DOM_VK_F11:
      return "F11";
    case KeyboardEvent.DOM_VK_F12:
      return "F12";
    case KeyboardEvent.DOM_VK_F13:
      return "F13";
    case KeyboardEvent.DOM_VK_F14:
      return "F14";
    case KeyboardEvent.DOM_VK_F15:
      return "F15";
    case KeyboardEvent.DOM_VK_F16:
      return "F16";
    case KeyboardEvent.DOM_VK_F17:
      return "F17";
    case KeyboardEvent.DOM_VK_F18:
      return "F18";
    case KeyboardEvent.DOM_VK_F19:
      return "F19";
    case KeyboardEvent.DOM_VK_F20:
      return "F20";
    case KeyboardEvent.DOM_VK_F21:
      return "F21";
    case KeyboardEvent.DOM_VK_F22:
      return "F22";
    case KeyboardEvent.DOM_VK_F23:
      return "F23";
    case KeyboardEvent.DOM_VK_F24:
      return "F24";
    case KeyboardEvent.DOM_VK_NUM_LOCK:
      return "NumLock";
    case KeyboardEvent.DOM_VK_SCROLL_LOCK:
      return "ScrollLock";
    case KeyboardEvent.DOM_VK_VOLUME_MUTE:
      return "VolumeMute";
    case KeyboardEvent.DOM_VK_VOLUME_DOWN:
      return "VolumeDown";
    case KeyboardEvent.DOM_VK_VOLUME_UP:
      return "VolumeUp";
    case KeyboardEvent.DOM_VK_META:
      return "Meta";
    case KeyboardEvent.DOM_VK_ALTGR:
      return "AltGraph";
    case KeyboardEvent.DOM_VK_ATTN:
      return "Attn";
    case KeyboardEvent.DOM_VK_CRSEL:
      return "CrSel";
    case KeyboardEvent.DOM_VK_EXSEL:
      return "ExSel";
    case KeyboardEvent.DOM_VK_EREOF:
      return "EraseEof";
    case KeyboardEvent.DOM_VK_PLAY:
      return "Play";
    default:
      return "Unidentified";
  }
}

function _createKeyboardEventDictionary(aKey, aKeyEvent)
{
  var result = { dictionary: null, flags: 0 };

  var keyCodeIsDefined = "keyCode" in aKeyEvent;
  var keyCode =
    (keyCodeIsDefined && aKeyEvent.keyCode >= 0 && aKeyEvent.keyCode <= 255) ?
      aKeyEvent.keyCode : 0;
  var keyName = "Unidentified";
  if (aKey.indexOf("KEY_") == 0) {
    keyName = aKey.substr("KEY_".length);
    result.flags |= _EU_Ci.nsITextInputProcessor.KEY_NON_PRINTABLE_KEY;
  } else if (aKey.indexOf("VK_") == 0) {
    keyCode = KeyEvent["DOM_" + aKey];
    if (!keyCode) {
      throw "Unknown key: " + aKey;
    }
    keyName = _guessKeyNameFromKeyCode(keyCode);
    result.flags |= _EU_Ci.nsITextInputProcessor.KEY_NON_PRINTABLE_KEY;
  } else if (aKey != "") {
    keyName = aKey;
    if (!keyCodeIsDefined) {
      keyCode = _computeKeyCodeFromChar(aKey.charAt(0));
    }
    if (!keyCode) {
      result.flags |= _EU_Ci.nsITextInputProcessor.KEY_KEEP_KEYCODE_ZERO;
    }
    result.flags |= _EU_Ci.nsITextInputProcessor.KEY_FORCE_PRINTABLE_KEY;
  }
  var locationIsDefined = "location" in aKeyEvent;
  if (locationIsDefined && aKeyEvent.location === 0) {
    result.flags |= _EU_Ci.nsITextInputProcessor.KEY_KEEP_KEY_LOCATION_STANDARD;
  }
  result.dictionary = {
    key: keyName,
    code: "code" in aKeyEvent ? aKeyEvent.code : "",
    location: locationIsDefined ? aKeyEvent.location : 0,
    repeat: "repeat" in aKeyEvent ? aKeyEvent.repeat === true : false,
    keyCode: keyCode,
  };
  return result;
}

function _emulateToActivateModifiers(aTIP, aKeyEvent)
{
  if (!aKeyEvent) {
    return null;
  }
  var modifiers = {
    normal: [
      { key: "Alt",        attr: "altKey" },
      { key: "AltGraph",   attr: "altGraphKey" },
      { key: "Control",    attr: "ctrlKey" },
      { key: "Fn",         attr: "fnKey" },
      { key: "Meta",       attr: "metaKey" },
      { key: "OS",         attr: "osKey" },
      { key: "Shift",      attr: "shiftKey" },
      { key: "Symbol",     attr: "symbolKey" },
      { key: (navigator.platform.indexOf("Mac") >= 0) ? "Meta" : "Control",
                           attr: "accelKey" },
    ],
    lockable: [
      { key: "CapsLock",   attr: "capsLockKey" },
      { key: "FnLock",     attr: "fnLockKey" },
      { key: "NumLock",    attr: "numLockKey" },
      { key: "ScrollLock", attr: "scrollLockKey" },
      { key: "SymbolLock", attr: "symbolLockKey" },
    ]
  }

  for (var i = 0; i < modifiers.normal.length; i++) {
    if (!aKeyEvent[modifiers.normal[i].attr]) {
      continue;
    }
    if (aTIP.getModifierState(modifiers.normal[i].key)) {
      continue; // already activated.
    }
    var event = new KeyboardEvent("", { key: modifiers.normal[i].key });
    aTIP.keydown(event,
      aTIP.KEY_NON_PRINTABLE_KEY | aTIP.KEY_DONT_DISPATCH_MODIFIER_KEY_EVENT);
    modifiers.normal[i].activated = true;
  }
  for (var i = 0; i < modifiers.lockable.length; i++) {
    if (!aKeyEvent[modifiers.lockable[i].attr]) {
      continue;
    }
    if (aTIP.getModifierState(modifiers.lockable[i].key)) {
      continue; // already activated.
    }
    var event = new KeyboardEvent("", { key: modifiers.lockable[i].key });
    aTIP.keydown(event,
      aTIP.KEY_NON_PRINTABLE_KEY | aTIP.KEY_DONT_DISPATCH_MODIFIER_KEY_EVENT);
    aTIP.keyup(event,
      aTIP.KEY_NON_PRINTABLE_KEY | aTIP.KEY_DONT_DISPATCH_MODIFIER_KEY_EVENT);
    modifiers.lockable[i].activated = true;
  }
  return modifiers;
}

function _emulateToInactivateModifiers(aTIP, aModifiers)
{
  if (!aModifiers) {
    return;
  }
  for (var i = 0; i < aModifiers.normal.length; i++) {
    if (!aModifiers.normal[i].activated) {
      continue;
    }
    var event = new KeyboardEvent("", { key: aModifiers.normal[i].key });
    aTIP.keyup(event,
      aTIP.KEY_NON_PRINTABLE_KEY | aTIP.KEY_DONT_DISPATCH_MODIFIER_KEY_EVENT);
  }
  for (var i = 0; i < aModifiers.lockable.length; i++) {
    if (!aModifiers.lockable[i].activated) {
      continue;
    }
    if (!aTIP.getModifierState(aModifiers.lockable[i].key)) {
      continue; // who already inactivated this?
    }
    var event = new KeyboardEvent("", { key: aModifiers.lockable[i].key });
    aTIP.keydown(event,
      aTIP.KEY_NON_PRINTABLE_KEY | aTIP.KEY_DONT_DISPATCH_MODIFIER_KEY_EVENT);
    aTIP.keyup(event,
      aTIP.KEY_NON_PRINTABLE_KEY | aTIP.KEY_DONT_DISPATCH_MODIFIER_KEY_EVENT);
  }
}

/**
 * Synthesize a composition event.
 *
 * @param aEvent               The composition event information.  This must
 *                             have |type| member.  The value must be
 *                             "compositionstart", "compositionend",
 *                             "compositioncommitasis" or "compositioncommit".
 *                             And also this may have |data| and |locale| which
 *                             would be used for the value of each property of
 *                             the composition event.  Note that the |data| is
 *                             ignored if the event type is "compositionstart"
 *                             or "compositioncommitasis".
 *                             If |key| is specified, the key event may be
 *                             dispatched.  This can emulates changing
 *                             composition state caused by key operation.
 *                             Its key value should start with "KEY_" if the
 *                             value is non-printable key name defined in D3E.
 * @param aWindow              Optional (If null, current |window| will be used)
 * @param aCallback            Optional (If non-null, use the callback for
 *                             receiving notifications to IME)
 */
function synthesizeComposition(aEvent, aWindow, aCallback)
{
  var TIP = _getTIP(aWindow, aCallback);
  if (!TIP) {
    return false;
  }
  var modifiers = _emulateToActivateModifiers(TIP, aEvent.key);
  var ret = false;
  var keyEventDict =
    "key" in aEvent ?
      _createKeyboardEventDictionary(aEvent.key.key, aEvent.key) :
      { dictionary: null, flags: 0 };
  var keyEvent = 
    "key" in aEvent ?
      new KeyboardEvent(aEvent.type === "keydown" ? "keydown" : "",
                        keyEventDict.dictionary) :
      null;
  try {
    switch (aEvent.type) {
      case "compositionstart":
        ret = TIP.startComposition(keyEvent, keyEventDict.flags);
        break;
      case "compositioncommitasis":
        ret = TIP.commitComposition(keyEvent, keyEventDict.flags);
        break;
      case "compositioncommit":
        ret = TIP.commitCompositionWith(aEvent.data, keyEvent,
                                        keyEventDict.flags);
        break;
    }
  } finally {
    _emulateToInactivateModifiers(TIP, modifiers);
  }
}
/**
 * Synthesize a compositionchange event which causes a DOM text event and
 * compositionupdate event if it's necessary.
 *
 * @param aEvent   The compositionchange event's information, this has
 *                 |composition| and |caret| members.  |composition| has
 *                 |string| and |clauses| members.  |clauses| must be array
 *                 object.  Each object has |length| and |attr|.  And |caret|
 *                 has |start| and |length|.  See the following tree image.
 *
 *                 aEvent
 *                   +-- composition
 *                   |     +-- string
 *                   |     +-- clauses[]
 *                   |           +-- length
 *                   |           +-- attr
 *                   +-- caret
 *                   |     +-- start
 *                   |     +-- length
 *                   +-- key
 *
 *                 Set the composition string to |composition.string|.  Set its
 *                 clauses information to the |clauses| array.
 *
 *                 When it's composing, set the each clauses' length to the
 *                 |composition.clauses[n].length|.  The sum of the all length
 *                 values must be same as the length of |composition.string|.
 *                 Set nsICompositionStringSynthesizer.ATTR_* to the
 *                 |composition.clauses[n].attr|.
 *
 *                 When it's not composing, set 0 to the
 *                 |composition.clauses[0].length| and
 *                 |composition.clauses[0].attr|.
 *
 *                 Set caret position to the |caret.start|. It's offset from
 *                 the start of the composition string.  Set caret length to
 *                 |caret.length|.  If it's larger than 0, it should be wide
 *                 caret.  However, current nsEditor doesn't support wide
 *                 caret, therefore, you should always set 0 now.
 *
 *                 If |key| is specified, the key event may be dispatched.
 *                 This can emulates changing composition state caused by key
 *                 operation.  Its key value should start with "KEY_" if the
 *                 value is non-printable key name defined in D3E.
 *
 * @param aWindow  Optional (If null, current |window| will be used)
 * @param aCallback     Optional (If non-null, use the callback for receiving
 *                      notifications to IME)
 */
function synthesizeCompositionChange(aEvent, aWindow, aCallback)
{
  var TIP = _getTIP(aWindow, aCallback);
  if (!TIP) {
    return;
  }

  if (!aEvent.composition || !aEvent.composition.clauses ||
      !aEvent.composition.clauses[0]) {
    return;
  }

  TIP.setPendingCompositionString(aEvent.composition.string);
  if (aEvent.composition.clauses[0].length) {
    for (var i = 0; i < aEvent.composition.clauses.length; i++) {
      switch (aEvent.composition.clauses[i].attr) {
        case TIP.ATTR_RAW_CLAUSE:
        case TIP.ATTR_SELECTED_RAW_CLAUSE:
        case TIP.ATTR_CONVERTED_CLAUSE:
        case TIP.ATTR_SELECTED_CLAUSE:
          TIP.appendClauseToPendingComposition(
                aEvent.composition.clauses[i].length,
                aEvent.composition.clauses[i].attr);
          break;
        case 0:
          // Ignore dummy clause for the argument.
          break;
        default:
          throw new Error("invalid clause attribute specified");
          break;
      }
    }
  }

  if (aEvent.caret) {
    TIP.setCaretInPendingComposition(aEvent.caret.start);
  }

  var modifiers = _emulateToActivateModifiers(TIP, aEvent.key);
  try {
    var keyEventDict =
      "key" in aEvent ?
        _createKeyboardEventDictionary(aEvent.key.key, aEvent.key) :
        { dictionary: null, flags: 0 };
    var keyEvent = 
      "key" in aEvent ?
        new KeyboardEvent(aEvent.type === "keydown" ? "keydown" : "",
                          keyEventDict.dictionary) :
        null;
    TIP.flushPendingComposition(keyEvent, keyEventDict.flags);
  } finally {
    _emulateToInactivateModifiers(TIP, modifiers);
  }
}

// Must be synchronized with nsIDOMWindowUtils.
const QUERY_CONTENT_FLAG_USE_NATIVE_LINE_BREAK          = 0x0000;
const QUERY_CONTENT_FLAG_USE_XP_LINE_BREAK              = 0x0001;

const SELECTION_SET_FLAG_USE_NATIVE_LINE_BREAK          = 0x0000;
const SELECTION_SET_FLAG_USE_XP_LINE_BREAK              = 0x0001;
const SELECTION_SET_FLAG_REVERSE                        = 0x0002;

/**
 * Synthesize a query selected text event.
 *
 * @param aWindow  Optional (If null, current |window| will be used)
 * @return         An nsIQueryContentEventResult object.  If this failed,
 *                 the result might be null.
 */
function synthesizeQuerySelectedText(aWindow)
{
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return null;
  }

  return utils.sendQueryContentEvent(utils.QUERY_SELECTED_TEXT, 0, 0, 0, 0,
                                     QUERY_CONTENT_FLAG_USE_NATIVE_LINE_BREAK);
}

/**
 * Synthesize a query caret rect event.
 *
 * @param aOffset  The caret offset.  0 means left side of the first character
 *                 in the selection root.
 * @param aWindow  Optional (If null, current |window| will be used)
 * @return         An nsIQueryContentEventResult object.  If this failed,
 *                 the result might be null.
 */
function synthesizeQueryCaretRect(aOffset, aWindow)
{
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return null;
  }
  return utils.sendQueryContentEvent(utils.QUERY_CARET_RECT,
                                     aOffset, 0, 0, 0,
                                     QUERY_CONTENT_FLAG_USE_NATIVE_LINE_BREAK);
}

/**
 * Synthesize a selection set event.
 *
 * @param aOffset  The character offset.  0 means the first character in the
 *                 selection root.
 * @param aLength  The length of the text.  If the length is too long,
 *                 the extra length is ignored.
 * @param aReverse If true, the selection is from |aOffset + aLength| to
 *                 |aOffset|.  Otherwise, from |aOffset| to |aOffset + aLength|.
 * @param aWindow  Optional (If null, current |window| will be used)
 * @return         True, if succeeded.  Otherwise false.
 */
function synthesizeSelectionSet(aOffset, aLength, aReverse, aWindow)
{
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return false;
  }
  var flags = aReverse ? SELECTION_SET_FLAG_REVERSE : 0;
  return utils.sendSelectionSetEvent(aOffset, aLength, flags);
}
