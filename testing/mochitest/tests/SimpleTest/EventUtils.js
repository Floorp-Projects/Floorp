/**
 * EventUtils provides some utility methods for creating and sending DOM events.
 * Current methods:
 *  sendMouseEvent
 *  sendChar
 *  sendString
 *  sendKey
 */

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
  if (['click', 'mousedown', 'mouseup', 'mouseover', 'mouseout'].indexOf(aEvent.type) == -1) {
    throw new Error("sendMouseEvent doesn't know about event type '"+aEvent.type+"'");
  }

  if (!aWindow) {
    aWindow = window;
  }

  if (!(aTarget instanceof Element)) {
    aTarget = aWindow.document.getElementById(aTarget);
  }

  // For events to trigger the UA's default actions they need to be "trusted"
  netscape.security.PrivilegeManager.enablePrivilege('UniversalBrowserWrite');

  var event = aWindow.document.createEvent('MouseEvent');

  var typeArg          = aEvent.type;
  var canBubbleArg     = true;
  var cancelableArg    = true;
  var viewArg          = aWindow;
  var detailArg        = aEvent.detail        || (aEvent.type == 'click'     ||
                                                  aEvent.type == 'mousedown' ||
                                                  aEvent.type == 'mouseup' ? 1 : 0);
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

  event.initMouseEvent(typeArg, canBubbleArg, cancelableArg, viewArg, detailArg,
                       screenXArg, screenYArg, clientXArg, clientYArg,
                       ctrlKeyArg, altKeyArg, shiftKeyArg, metaKeyArg,
                       buttonArg, relatedTargetArg);

  aTarget.dispatchEvent(event);
}

/**
 * Send the char aChar to the node with id aTarget.  If aTarget is not
 * provided, use "target".  This method handles casing of chars (sends the
 * right charcode, and sends a shift key for uppercase chars).  No other
 * modifiers are handled at this point.
 *
 * For now this method only works for English letters (lower and upper case)
 * and the digits 0-9.
 *
 * Returns true if the keypress event was accepted (no calls to preventDefault
 * or anything like that), false otherwise.
 */
function sendChar(aChar, aTarget) {
  // DOM event charcodes match ASCII (JS charcodes) for a-zA-Z0-9.
  var hasShift = (aChar == aChar.toUpperCase());
  var charCode = aChar.charCodeAt(0);
  var keyCode = charCode;
  if (!hasShift) {
    // For lowercase letters, the keyCode is actually 32 less than the charCode
    keyCode -= 0x20;
  }

  return __doEventDispatch(aTarget, charCode, keyCode, hasShift);
}

/**
 * Send the string aStr to the node with id aTarget.  If aTarget is not
 * provided, use "target".
 *
 * For now this method only works for English letters (lower and upper case)
 * and the digits 0-9.
 */
function sendString(aStr, aTarget) {
  for (var i = 0; i < aStr.length; ++i) {
    sendChar(aStr.charAt(i), aTarget);
  }
}

/**
 * Send the non-character key aKey to the node with id aTarget. If aTarget is
 * not provided, use "target".  The name of the key should be a lowercase
 * version of the part that comes after "DOM_VK_" in the KeyEvent constant
 * name for this key.  No modifiers are handled at this point.
 *
 * Returns true if the keypress event was accepted (no calls to preventDefault
 * or anything like that), false otherwise.
 */
function sendKey(aKey, aTarget) {
  keyName = "DOM_VK_" + aKey.toUpperCase();

  if (!KeyEvent[keyName]) {
    throw "Unknown key: " + keyName;
  }

  return __doEventDispatch(aTarget, 0, KeyEvent[keyName], false);
}

/**
 * Actually perform event dispatch given a charCode, keyCode, and boolean for
 * whether "shift" was pressed.  Send the event to the node with id aTarget.  If
 * aTarget is not provided, use "target".
 *
 * Returns true if the keypress event was accepted (no calls to preventDefault
 * or anything like that), false otherwise.
 */
function __doEventDispatch(aTarget, aCharCode, aKeyCode, aHasShift) {
  if (aTarget === undefined) {
    aTarget = "target";
  }

  // Make our events trusted
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  var event = document.createEvent("KeyEvents");
  event.initKeyEvent("keydown", true, true, document.defaultView,
                     false, false, aHasShift, false,
                     aKeyCode, 0);
  var accepted = $(aTarget).dispatchEvent(event);

  // Preventing the default keydown action also prevents the default
  // keypress action.
  event = document.createEvent("KeyEvents");
  if (aCharCode) {
    event.initKeyEvent("keypress", true, true, document.defaultView,
                       false, false, aHasShift, false,
                       0, aCharCode);
  } else {
    event.initKeyEvent("keypress", true, true, document.defaultView,
                       false, false, aHasShift, false,
                       aKeyCode, 0);
  }
  if (!accepted) {
    event.preventDefault();
  }
  accepted = $(aTarget).dispatchEvent(event);

  // Always send keyup
  var event = document.createEvent("KeyEvents");
  event.initKeyEvent("keyup", true, true, document.defaultView,
                     false, false, aHasShift, false,
                     aKeyCode, 0);
  $(aTarget).dispatchEvent(event);
  return accepted;
}

/**
 * Parse the key modifier flags from aEvent. Used to share code between
 * synthesizeMouse and synthesizeKey.
 */
function _parseModifiers(aEvent)
{
  const masks = Components.interfaces.nsIDOMNSEvent;
  var mval = 0;
  if (aEvent.shiftKey)
    mval |= masks.SHIFT_MASK;
  if (aEvent.ctrlKey)
    mval |= masks.CONTROL_MASK;
  if (aEvent.altKey)
    mval |= masks.ALT_MASK;
  if (aEvent.metaKey)
    mval |= masks.META_MASK;
  if (aEvent.accelKey)
    mval |= (navigator.platform.indexOf("Mac") >= 0) ? masks.META_MASK :
                                                       masks.CONTROL_MASK;

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
 */
function synthesizeMouse(aTarget, aOffsetX, aOffsetY, aEvent, aWindow)
{
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

  if (!aWindow)
    aWindow = window;

  var utils = aWindow.QueryInterface(Components.interfaces.nsIInterfaceRequestor).
                      getInterface(Components.interfaces.nsIDOMWindowUtils);
  if (utils) {
    var button = aEvent.button || 0;
    var clickCount = aEvent.clickCount || 1;
    var modifiers = _parseModifiers(aEvent);

    var rect = aTarget.getBoundingClientRect();

    var left = rect.left + aOffsetX;
    var top = rect.top + aOffsetY;

    if (aEvent.type) {
      utils.sendMouseEvent(aEvent.type, left, top, button, clickCount, modifiers);
    }
    else {
      utils.sendMouseEvent("mousedown", left, top, button, clickCount, modifiers);
      utils.sendMouseEvent("mouseup", left, top, button, clickCount, modifiers);
    }
  }
}

// Call synthesizeMouse with coordinates at the center of aTarget.
function synthesizeMouseAtCenter(aTarget, aEvent, aWindow)
{
  var rect = aTarget.getBoundingClientRect();
  synthesizeMouse(aTarget, rect.width / 2, rect.height / 2, aEvent,
                  aWindow);
}

/**
 * Synthesize a mouse scroll event on a target. The actual client point is determined
 * by taking the aTarget's client box and offseting it by aOffsetX and
 * aOffsetY.
 *
 * aEvent is an object which may contain the properties:
 *   shiftKey, ctrlKey, altKey, metaKey, accessKey, button, type, axis, delta, hasPixels
 *
 * If the type is specified, a mouse scroll event of that type is fired. Otherwise,
 * "DOMMouseScroll" is used.
 *
 * If the axis is specified, it must be one of "horizontal" or "vertical". If not specified,
 * "vertical" is used.
 *
 * 'delta' is the amount to scroll by (can be positive or negative). It must
 * be specified.
 *
 * 'hasPixels' specifies whether kHasPixels should be set in the scrollFlags.
 *
 * 'isMomentum' specifies whether kIsMomentum should be set in the scrollFlags.
 *
 * aWindow is optional, and defaults to the current window object.
 */
function synthesizeMouseScroll(aTarget, aOffsetX, aOffsetY, aEvent, aWindow)
{
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

  if (!aWindow)
    aWindow = window;

  var utils = aWindow.QueryInterface(Components.interfaces.nsIInterfaceRequestor).
                      getInterface(Components.interfaces.nsIDOMWindowUtils);
  if (utils) {
    // See nsMouseScrollFlags in nsGUIEvent.h
    const kIsVertical = 0x02;
    const kIsHorizontal = 0x04;
    const kHasPixels = 0x08;
    const kIsMomentum = 0x40;

    var button = aEvent.button || 0;
    var modifiers = _parseModifiers(aEvent);

    var rect = aTarget.getBoundingClientRect();

    var left = rect.left;
    var top = rect.top;

    var type = aEvent.type || "DOMMouseScroll";
    var axis = aEvent.axis || "vertical";
    var scrollFlags = (axis == "horizontal") ? kIsHorizontal : kIsVertical;
    if (aEvent.hasPixels) {
      scrollFlags |= kHasPixels;
    }
    if (aEvent.isMomentum) {
      scrollFlags |= kIsMomentum;
    }
    utils.sendMouseScrollEvent(type, left + aOffsetX, top + aOffsetY, button,
                               scrollFlags, aEvent.delta, modifiers);
  }
}

function _computeKeyCodeFromChar(aChar)
{
  if (aChar.length != 1) {
    return 0;
  }
  const nsIDOMKeyEvent = Components.interfaces.nsIDOMKeyEvent;
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
    default:
      return 0;
  }
}

/**
 * Synthesize a key event. It is targeted at whatever would be targeted by an
 * actual keypress by the user, typically the focused element.
 *
 * aKey should be either a character or a keycode starting with VK_ such as
 * VK_ENTER.
 *
 * aEvent is an object which may contain the properties:
 *   shiftKey, ctrlKey, altKey, metaKey, accessKey, type
 *
 * If the type is specified, a key event of that type is fired. Otherwise,
 * a keydown, a keypress and then a keyup event are fired in sequence.
 *
 * aWindow is optional, and defaults to the current window object.
 */
function synthesizeKey(aKey, aEvent, aWindow)
{
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

  if (!aWindow)
    aWindow = window;

  var utils = aWindow.QueryInterface(Components.interfaces.nsIInterfaceRequestor).
                      getInterface(Components.interfaces.nsIDOMWindowUtils);
  if (utils) {
    var keyCode = 0, charCode = 0;
    if (aKey.indexOf("VK_") == 0)
      keyCode = KeyEvent["DOM_" + aKey];
    else {
      charCode = aKey.charCodeAt(0);
      keyCode = _computeKeyCodeFromChar(aKey.charAt(0));
    }

    var modifiers = _parseModifiers(aEvent);

    if (aEvent.type == "keypress") {
      utils.sendKeyEvent(aEvent.type, charCode ? 0 : keyCode,
                         charCode, modifiers);
    } else if (aEvent.type) {
      utils.sendKeyEvent(aEvent.type, keyCode, 0, modifiers);
    } else {
      var keyDownDefaultHappened =
          utils.sendKeyEvent("keydown", keyCode, 0, modifiers);
      utils.sendKeyEvent("keypress", charCode ? 0 : keyCode, charCode,
                         modifiers, !keyDownDefaultHappened);
      utils.sendKeyEvent("keyup", keyCode, 0, modifiers);
    }
  }
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

/**
 * Emulate a dragstart event.
 *  element - element to fire the dragstart event on
 *  expectedDragData - the data you expect the data transfer to contain afterwards
 *                      This data is in the format:
 *                         [ [ {type: value, data: value, test: function}, ... ], ... ]
 *                     can be null
 *  aWindow - optional; defaults to the current window object.
 *  x - optional; initial x coordinate
 *  y - optional; initial y coordinate
 * Returns null if data matches.
 * Returns the event.dataTransfer if data does not match
 *
 * eqTest is an optional function if comparison can't be done with x == y;
 *   function (actualData, expectedData) {return boolean}
 *   @param actualData from dataTransfer
 *   @param expectedData from expectedDragData
 * see bug 462172 for example of use
 *
 */
function synthesizeDragStart(element, expectedDragData, aWindow, x, y)
{
  if (!aWindow)
    aWindow = window;
  x = x || 2;
  y = y || 2;
  const step = 9;

  var result = "trapDrag was not called";
  var trapDrag = function(event) {
    try {
      var dataTransfer = event.dataTransfer;
      result = null;
      if (!dataTransfer)
        throw "no dataTransfer";
      if (expectedDragData == null ||
          dataTransfer.mozItemCount != expectedDragData.length)
        throw dataTransfer;
      for (var i = 0; i < dataTransfer.mozItemCount; i++) {
        var dtTypes = dataTransfer.mozTypesAt(i);
        if (dtTypes.length != expectedDragData[i].length)
          throw dataTransfer;
        for (var j = 0; j < dtTypes.length; j++) {
          if (dtTypes[j] != expectedDragData[i][j].type)
            throw dataTransfer;
          var dtData = dataTransfer.mozGetDataAt(dtTypes[j],i);
          if (expectedDragData[i][j].eqTest) {
            if (!expectedDragData[i][j].eqTest(dtData, expectedDragData[i][j].data))
              throw dataTransfer;
          }
          else if (expectedDragData[i][j].data != dtData)
            throw dataTransfer;
        }
      }
    } catch(ex) {
      result = ex;
    }
    event.preventDefault();
    event.stopPropagation();
  }
  aWindow.addEventListener("dragstart", trapDrag, false);
  synthesizeMouse(element, x, y, { type: "mousedown" }, aWindow);
  x += step; y += step;
  synthesizeMouse(element, x, y, { type: "mousemove" }, aWindow);
  x += step; y += step;
  synthesizeMouse(element, x, y, { type: "mousemove" }, aWindow);
  aWindow.removeEventListener("dragstart", trapDrag, false);
  synthesizeMouse(element, x, y, { type: "mouseup" }, aWindow);
  return result;
}

/**
 * Emulate a drop by emulating a dragstart and firing events dragenter, dragover, and drop.
 *  srcElement - the element to use to start the drag, usually the same as destElement
 *               but if destElement isn't suitable to start a drag on pass a suitable
 *               element for srcElement
 *  destElement - the element to fire the dragover, dragleave and drop events
 *  dragData - the data to supply for the data transfer
 *                     This data is in the format:
 *                       [ [ {type: value, data: value}, ...], ... ]
 *  dropEffect - the drop effect to set during the dragstart event, or 'move' if null
 *  aWindow - optional; defaults to the current window object.
 *
 * Returns the drop effect that was desired.
 */
function synthesizeDrop(srcElement, destElement, dragData, dropEffect, aWindow)
{
  if (!aWindow)
    aWindow = window;

  // For events to trigger the UA's default actions they need to be "trusted".
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  var gWindowUtils  = window.QueryInterface(Components.interfaces.nsIInterfaceRequestor).
                             getInterface(Components.interfaces.nsIDOMWindowUtils);
  var ds = Components.classes["@mozilla.org/widget/dragservice;1"].
           getService(Components.interfaces.nsIDragService);

  var dataTransfer;
  var trapDrag = function(event) {
    dataTransfer = event.dataTransfer;
    for (var i = 0; i < dragData.length; i++) {
      var item = dragData[i];
      for (var j = 0; j < item.length; j++) {
        dataTransfer.mozSetDataAt(item[j].type, item[j].data, i);
      }
    }
    dataTransfer.dropEffect = dropEffect || "move";
    event.preventDefault();
    event.stopPropagation();
  }

  ds.startDragSession();

  try {
    // need to use real mouse action
    aWindow.addEventListener("dragstart", trapDrag, true);
    synthesizeMouseAtCenter(srcElement, { type: "mousedown" }, aWindow);
    synthesizeMouse(srcElement, 11, 11, { type: "mousemove" }, aWindow);
    synthesizeMouse(srcElement, 20, 20, { type: "mousemove" }, aWindow);
    aWindow.removeEventListener("dragstart", trapDrag, true);

    event = aWindow.document.createEvent("DragEvents");
    event.initDragEvent("dragenter", true, true, aWindow, 0, 0, 0, 0, 0, false, false, false, false, 0, null, dataTransfer);
    gWindowUtils.dispatchDOMEventViaPresShell(destElement, event, true);

    var event = aWindow.document.createEvent("DragEvents");
    event.initDragEvent("dragover", true, true, aWindow, 0, 0, 0, 0, 0, false, false, false, false, 0, null, dataTransfer);
    if (gWindowUtils.dispatchDOMEventViaPresShell(destElement, event, true)) {
      synthesizeMouseAtCenter(destElement, { type: "mouseup" }, aWindow);
      return "none";
    }

    if (dataTransfer.dropEffect != "none") {
      event = aWindow.document.createEvent("DragEvents");
      event.initDragEvent("drop", true, true, aWindow, 0, 0, 0, 0, 0, false, false, false, false, 0, null, dataTransfer);
      gWindowUtils.dispatchDOMEventViaPresShell(destElement, event, true);
    }

    synthesizeMouseAtCenter(destElement, { type: "mouseup" }, aWindow);

    return dataTransfer.dropEffect;
  } finally {
    ds.endDragSession(true);
  }
}

function disableNonTestMouseEvents(aDisable)
{
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

  var utils =
    window.QueryInterface(Components.interfaces.nsIInterfaceRequestor).
           getInterface(Components.interfaces.nsIDOMWindowUtils);
  if (utils)
    utils.disableNonTestMouseEvents(aDisable);
}

function _getDOMWindowUtils(aWindow)
{
  if (!aWindow) {
    aWindow = window;
  }
  return aWindow.QueryInterface(Components.interfaces.nsIInterfaceRequestor).
                 getInterface(Components.interfaces.nsIDOMWindowUtils);
}

/*
 * synthesizeComposition, synthesizeText and synthesizeQuerySelectedText
 * are only used by layout/base/tests/test_reftests_with_caret.html.
 */

/**
 * Synthesize a composition event.
 *
 * @param aIsCompositionStart  If true, this synthesize compositionstart event.
 *                             Otherwise, compositionend event.
 * @param aWindow              Optional (If null, current |window| will be used)
 */
function synthesizeComposition(aIsCompositionStart, aWindow)
{
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return;
  }

  utils.sendCompositionEvent(aIsCompositionStart ?
                               "compositionstart" : "compositionend");
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
 *                 Set nsIDOMWindowUtils.COMPOSITION_ATTR_* to the
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
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return;
  }

  if (!aEvent.composition || !aEvent.composition.clauses ||
      !aEvent.composition.clauses[0]) {
    return;
  }

  var firstClauseLength = aEvent.composition.clauses[0].length;
  var firstClauseAttr   = aEvent.composition.clauses[0].attr;
  var secondClauseLength = 0;
  var secondClauseAttr = 0;
  var thirdClauseLength = 0;
  var thirdClauseAttr = 0;
  if (aEvent.composition.clauses[1]) {
    secondClauseLength = aEvent.composition.clauses[1].length;
    secondClauseAttr   = aEvent.composition.clauses[1].attr;
    if (aEvent.composition.clauses[2]) {
      thirdClauseLength = aEvent.composition.clauses[2].length;
      thirdClauseAttr   = aEvent.composition.clauses[2].attr;
    }
  }

  var caretStart = -1;
  var caretLength = 0;
  if (aEvent.caret) {
    caretStart = aEvent.caret.start;
    caretLength = aEvent.caret.length;
  }

  utils.sendTextEvent(aEvent.composition.string,
                      firstClauseLength, firstClauseAttr,
                      secondClauseLength, secondClauseAttr,
                      thirdClauseLength, thirdClauseAttr,
                      caretStart, caretLength);
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
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return nsnull;
  }
  return utils.sendQueryContentEvent(utils.QUERY_SELECTED_TEXT, 0, 0, 0, 0);
}


