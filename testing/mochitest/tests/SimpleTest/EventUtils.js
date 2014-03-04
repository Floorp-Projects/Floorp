/**
 * EventUtils provides some utility methods for creating and sending DOM events.
 * Current methods:
 *  sendMouseEvent
 *  sendChar
 *  sendString
 *  sendKey
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

  if (!(aTarget instanceof aWindow.Element)) {
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
  if (aEvent.numLockKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_NUMLOCK;
  }
  if (aEvent.scrollLockKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_SCROLLLOCK;
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

    if (("type" in aEvent) && aEvent.type) {
      defaultPrevented = utils.sendPointerEvent(aEvent.type, left, top, button,
                                                clickCount, modifiers, false,
                                                pressure, inputSource,
                                                synthesized);
    }
    else {
      utils.sendPointerEvent("pointerdown", left, top, button, clickCount, modifiers, false, pressure, inputSource);
      utils.sendPointerEvent("pointerup", left, top, button, clickCount, modifiers, false, pressure, inputSource);
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
 *   deltaMode, lineOrPageDeltaX, lineOrPageDeltaY, isMomentum, isPixelOnlyDevice,
 *   isCustomizedByPrefs, expectedOverflowDeltaX, expectedOverflowDeltaY
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
  if (aEvent.isPixelOnlyDevice &&
      (aEvent.deltaMode == WheelEvent.DOM_DELTA_PIXEL)) {
    options |= utils.WHEEL_EVENT_CAUSED_BY_PIXEL_ONLY_DEVICE;
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
  var isPixelOnlyDevice =
    aEvent.isPixelOnlyDevice && aEvent.deltaMode == WheelEvent.DOM_DELTA_PIXEL;

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
 * isKeypressFiredKey() returns TRUE if the given key should cause keypress
 * event when widget handles the native key event.  Otherwise, FALSE.
 *
 * aDOMKeyCode should be one of consts of nsIDOMKeyEvent::DOM_VK_*, or a key
 * name begins with "VK_", or a character.
 */
function isKeypressFiredKey(aDOMKeyCode)
{
  if (typeof(aDOMKeyCode) == "string") {
    if (aDOMKeyCode.indexOf("VK_") == 0) {
      aDOMKeyCode = KeyEvent["DOM_" + aDOMKeyCode];
      if (!aDOMKeyCode) {
        throw "Unknown key: " + aDOMKeyCode;
      }
    } else {
      // If the key generates a character, it must cause a keypress event.
      return true;
    }
  }
  switch (aDOMKeyCode) {
    case KeyEvent.DOM_VK_SHIFT:
    case KeyEvent.DOM_VK_CONTROL:
    case KeyEvent.DOM_VK_ALT:
    case KeyEvent.DOM_VK_CAPS_LOCK:
    case KeyEvent.DOM_VK_NUM_LOCK:
    case KeyEvent.DOM_VK_SCROLL_LOCK:
    case KeyEvent.DOM_VK_META:
      return false;
    default:
      return true;
  }
}

/**
 * Synthesize a key event. It is targeted at whatever would be targeted by an
 * actual keypress by the user, typically the focused element.
 *
 * aKey should be either a character or a keycode starting with VK_ such as
 * VK_RETURN.
 *
 * aEvent is an object which may contain the properties:
 *   shiftKey, ctrlKey, altKey, metaKey, accessKey, type, location
 *
 * Sets one of KeyboardEvent.DOM_KEY_LOCATION_* to location.  Otherwise,
 * DOMWindowUtils will choose good location from the keycode.
 *
 * If the type is specified, a key event of that type is fired. Otherwise,
 * a keydown, a keypress and then a keyup event are fired in sequence.
 *
 * aWindow is optional, and defaults to the current window object.
 */
function synthesizeKey(aKey, aEvent, aWindow)
{
  var utils = _getDOMWindowUtils(aWindow);
  if (utils) {
    var keyCode = 0, charCode = 0;
    if (aKey.indexOf("VK_") == 0) {
      keyCode = KeyEvent["DOM_" + aKey];
      if (!keyCode) {
        throw "Unknown key: " + aKey;
      }
    } else {
      charCode = aKey.charCodeAt(0);
      keyCode = _computeKeyCodeFromChar(aKey.charAt(0));
    }

    var modifiers = _parseModifiers(aEvent);
    var flags = 0;
    if (aEvent.location != undefined) {
      switch (aEvent.location) {
        case KeyboardEvent.DOM_KEY_LOCATION_STANDARD:
          flags |= utils.KEY_FLAG_LOCATION_STANDARD;
          break;
        case KeyboardEvent.DOM_KEY_LOCATION_LEFT:
          flags |= utils.KEY_FLAG_LOCATION_LEFT;
          break;
        case KeyboardEvent.DOM_KEY_LOCATION_RIGHT:
          flags |= utils.KEY_FLAG_LOCATION_RIGHT;
          break;
        case KeyboardEvent.DOM_KEY_LOCATION_NUMPAD:
          flags |= utils.KEY_FLAG_LOCATION_NUMPAD;
          break;
        case KeyboardEvent.DOM_KEY_LOCATION_MOBILE:
          flags |= utils.KEY_FLAG_LOCATION_MOBILE;
          break;
        case KeyboardEvent.DOM_KEY_LOCATION_JOYSTICK:
          flags |= utils.KEY_FLAG_LOCATION_JOYSTICK;
          break;
      }
    }

    if (!("type" in aEvent) || !aEvent.type) {
      // Send keydown + (optional) keypress + keyup events.
      var keyDownDefaultHappened =
        utils.sendKeyEvent("keydown", keyCode, 0, modifiers, flags);
      if (isKeypressFiredKey(keyCode) && keyDownDefaultHappened) {
        utils.sendKeyEvent("keypress", keyCode, charCode, modifiers, flags);
      }
      utils.sendKeyEvent("keyup", keyCode, 0, modifiers, flags);
    } else if (aEvent.type == "keypress") {
      // Send standalone keypress event.
      utils.sendKeyEvent(aEvent.type, keyCode, charCode, modifiers, flags);
    } else {
      // Send other standalone event than keypress.
      utils.sendKeyEvent(aEvent.type, keyCode, 0, modifiers, flags);
    }
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
 * This is implemented only on Windows and Mac.
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
 * @return                      True if this function succeed dispatching
 *                              native key event.  Otherwise, false.
 */

function synthesizeNativeKey(aKeyboardLayout, aNativeKeyCode, aModifiers,
                             aChars, aUnmodifiedChars)
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
  utils.sendNativeKeyEvent(nativeKeyboardLayout, aNativeKeyCode,
                           _parseNativeModifiers(aModifiers),
                           aChars, aUnmodifiedChars);
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

// Must be synchronized with nsICompositionStringSynthesizer.
const COMPOSITION_ATTR_RAWINPUT              = 0x02;
const COMPOSITION_ATTR_SELECTEDRAWTEXT       = 0x03;
const COMPOSITION_ATTR_CONVERTEDTEXT         = 0x04;
const COMPOSITION_ATTR_SELECTEDCONVERTEDTEXT = 0x05;

/**
 * Synthesize a composition event.
 *
 * @param aEvent               The composition event information.  This must
 *                             have |type| member.  The value must be
 *                             "compositionstart", "compositionend" or
 *                             "compositionupdate".
 *                             And also this may have |data| and |locale| which
 *                             would be used for the value of each property of
 *                             the composition event.  Note that the data would
 *                             be ignored if the event type were
 *                             "compositionstart".
 * @param aWindow              Optional (If null, current |window| will be used)
 */
function synthesizeComposition(aEvent, aWindow)
{
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return;
  }

  utils.sendCompositionEvent(aEvent.type, aEvent.data ? aEvent.data : "",
                             aEvent.locale ? aEvent.locale : "");
}
/**
 * Synthesize a text event.
 *
 * @param aEvent   The text event's information, this has |composition|
 *                 and |caret| members.  |composition| has |string| and
 *                 |clauses| members.  |clauses| must be array object.  Each
 *                 object has |length| and |attr|.  And |caret| has |start| and
 *                 |length|.  See the following tree image.
 *
 *                 aEvent
 *                   +-- composition
 *                   |     +-- string
 *                   |     +-- clauses[]
 *                   |           +-- length
 *                   |           +-- attr
 *                   +-- caret
 *                         +-- start
 *                         +-- length
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
 * @param aWindow  Optional (If null, current |window| will be used)
 */
function synthesizeText(aEvent, aWindow)
{
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return;
  }

  if (!aEvent.composition || !aEvent.composition.clauses ||
      !aEvent.composition.clauses[0]) {
    return;
  }

  var compositionString = utils.createCompositionStringSynthesizer();
  compositionString.setString(aEvent.composition.string);
  if (aEvent.composition.clauses[0].length) {
    for (var i = 0; i < aEvent.composition.clauses.length; i++) {
      switch (aEvent.composition.clauses[i].attr) {
        case compositionString.ATTR_RAWINPUT:
        case compositionString.ATTR_SELECTEDRAWTEXT:
        case compositionString.ATTR_CONVERTEDTEXT:
        case compositionString.ATTR_SELECTEDCONVERTEDTEXT:
          compositionString.appendClause(aEvent.composition.clauses[i].length,
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
    compositionString.setCaret(aEvent.caret.start, aEvent.caret.length);
  }

  compositionString.dispatchEvent();
}

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

  return utils.sendQueryContentEvent(utils.QUERY_SELECTED_TEXT, 0, 0, 0, 0);
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
  return utils.sendSelectionSetEvent(aOffset, aLength, aReverse);
}
