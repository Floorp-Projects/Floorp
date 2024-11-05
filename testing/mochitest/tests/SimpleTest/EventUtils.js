/* eslint-disable no-nested-ternary */
/**
 * EventUtils provides some utility methods for creating and sending DOM events.
 *
 *  When adding methods to this file, please add a performance test for it.
 */

// Certain functions assume this is loaded into browser window scope.
// This is modifiable because certain chrome tests create their own gBrowser.
/* global gBrowser:true */

// This file is used both in privileged and unprivileged contexts, so we have to
// be careful about our access to Components.interfaces. We also want to avoid
// naming collisions with anything that might be defined in the scope that imports
// this script.
//
// Even if the real |Components| doesn't exist, we might shim in a simple JS
// placebo for compat. An easy way to differentiate this from the real thing
// is whether the property is read-only or not.  The real |Components| property
// is read-only.
/* global _EU_Ci, _EU_Cc, _EU_Cu, _EU_ChromeUtils, _EU_OS */
window.__defineGetter__("_EU_Ci", function () {
  var c = Object.getOwnPropertyDescriptor(window, "Components");
  return c && c.value && !c.writable ? Ci : SpecialPowers.Ci;
});

window.__defineGetter__("_EU_Cc", function () {
  var c = Object.getOwnPropertyDescriptor(window, "Components");
  return c && c.value && !c.writable ? Cc : SpecialPowers.Cc;
});

window.__defineGetter__("_EU_Cu", function () {
  var c = Object.getOwnPropertyDescriptor(window, "Components");
  return c && c.value && !c.writable ? Cu : SpecialPowers.Cu;
});

window.__defineGetter__("_EU_ChromeUtils", function () {
  var c = Object.getOwnPropertyDescriptor(window, "ChromeUtils");
  return c && c.value && !c.writable ? ChromeUtils : SpecialPowers.ChromeUtils;
});

window.__defineGetter__("_EU_OS", function () {
  delete this._EU_OS;
  try {
    this._EU_OS = _EU_ChromeUtils.importESModule(
      "resource://gre/modules/AppConstants.sys.mjs"
    ).platform;
  } catch (ex) {
    this._EU_OS = null;
  }
  return this._EU_OS;
});

function _EU_isMac(aWindow = window) {
  if (window._EU_OS) {
    return window._EU_OS == "macosx";
  }
  if (aWindow) {
    try {
      return aWindow.navigator.platform.indexOf("Mac") > -1;
    } catch (ex) {}
  }
  return navigator.platform.indexOf("Mac") > -1;
}

function _EU_isWin(aWindow = window) {
  if (window._EU_OS) {
    return window._EU_OS == "win";
  }
  if (aWindow) {
    try {
      return aWindow.navigator.platform.indexOf("Win") > -1;
    } catch (ex) {}
  }
  return navigator.platform.indexOf("Win") > -1;
}

function _EU_isLinux(aWindow = window) {
  if (window._EU_OS) {
    return window._EU_OS == "linux";
  }
  if (aWindow) {
    try {
      return aWindow.navigator.platform.startsWith("Linux");
    } catch (ex) {}
  }
  return navigator.platform.startsWith("Linux");
}

function _EU_isAndroid(aWindow = window) {
  if (window._EU_OS) {
    return window._EU_OS == "android";
  }
  if (aWindow) {
    try {
      return aWindow.navigator.userAgent.includes("Android");
    } catch (ex) {}
  }
  return navigator.userAgent.includes("Android");
}

function _EU_maybeWrap(o) {
  // We're used in some contexts where there is no SpecialPowers and also in
  // some where it exists but has no wrap() method.  And this is somewhat
  // independent of whether window.Components is a thing...
  var haveWrap = false;
  try {
    haveWrap = SpecialPowers.wrap != undefined;
  } catch (e) {
    // Just leave it false.
  }
  if (!haveWrap) {
    // Not much we can do here.
    return o;
  }
  var c = Object.getOwnPropertyDescriptor(window, "Components");
  return c && c.value && !c.writable ? o : SpecialPowers.wrap(o);
}

function _EU_maybeUnwrap(o) {
  var haveWrap = false;
  try {
    haveWrap = SpecialPowers.unwrap != undefined;
  } catch (e) {
    // Just leave it false.
  }
  if (!haveWrap) {
    // Not much we can do here.
    return o;
  }
  var c = Object.getOwnPropertyDescriptor(window, "Components");
  return c && c.value && !c.writable ? o : SpecialPowers.unwrap(o);
}

function _EU_getPlatform() {
  if (_EU_isWin()) {
    return "windows";
  }
  if (_EU_isMac()) {
    return "mac";
  }
  if (_EU_isAndroid()) {
    return "android";
  }
  if (_EU_isLinux()) {
    return "linux";
  }
  return "unknown";
}

/**
 * promiseElementReadyForUserInput() dispatches mousemove events to aElement
 * and waits one of them for a while.  Then, returns "resolved" state when it's
 * successfully received.  Otherwise, if it couldn't receive mousemove event on
 * it, this throws an exception.  So, aElement must be an element which is
 * assumed non-collapsed visible element in the window.
 *
 * This is useful if you need to synthesize mouse events via the main process
 * but your test cannot check whether the element is now in APZ to deliver
 * a user input event.
 */
async function promiseElementReadyForUserInput(
  aElement,
  aWindow = window,
  aLogFunc = null
) {
  if (typeof aElement == "string") {
    aElement = aWindow.document.getElementById(aElement);
  }

  function waitForMouseMoveForHittest() {
    return new Promise(resolve => {
      let timeout;
      const onHit = () => {
        if (aLogFunc) {
          aLogFunc("mousemove received");
        }
        aWindow.clearInterval(timeout);
        resolve(true);
      };
      aElement.addEventListener("mousemove", onHit, {
        capture: true,
        once: true,
      });
      timeout = aWindow.setInterval(() => {
        if (aLogFunc) {
          aLogFunc("mousemove not received in this 300ms");
        }
        aElement.removeEventListener("mousemove", onHit, {
          capture: true,
        });
        resolve(false);
      }, 300);
      synthesizeMouseAtCenter(aElement, { type: "mousemove" }, aWindow);
    });
  }
  for (let i = 0; i < 20; i++) {
    if (await waitForMouseMoveForHittest()) {
      return Promise.resolve();
    }
  }
  throw new Error("The element or the window did not become interactive");
}

function getElement(id) {
  return typeof id == "string" ? document.getElementById(id) : id;
}

this.$ = this.getElement;

function computeButton(aEvent) {
  if (typeof aEvent.button != "undefined") {
    return aEvent.button;
  }
  return aEvent.type == "contextmenu" ? 2 : 0;
}

function computeButtons(aEvent, utils) {
  if (typeof aEvent.buttons != "undefined") {
    return aEvent.buttons;
  }

  if (typeof aEvent.button != "undefined") {
    return utils.MOUSE_BUTTONS_NOT_SPECIFIED;
  }

  if (typeof aEvent.type != "undefined" && aEvent.type != "mousedown") {
    return utils.MOUSE_BUTTONS_NO_BUTTON;
  }

  return utils.MOUSE_BUTTONS_NOT_SPECIFIED;
}

/**
 * Send a mouse event to the node aTarget (aTarget can be an id, or an
 * actual node) . The "event" passed in to aEvent is just a JavaScript
 * object with the properties set that the real mouse event object should
 * have. This includes the type of the mouse event. Pretty much all those
 * properties are optional.
 * E.g. to send an click event to the node with id 'node' you might do this:
 *
 * ``sendMouseEvent({type:'click'}, 'node');``
 */
function sendMouseEvent(aEvent, aTarget, aWindow) {
  if (
    ![
      "click",
      "contextmenu",
      "dblclick",
      "mousedown",
      "mouseup",
      "mouseover",
      "mouseout",
    ].includes(aEvent.type)
  ) {
    throw new Error(
      "sendMouseEvent doesn't know about event type '" + aEvent.type + "'"
    );
  }

  if (!aWindow) {
    aWindow = window;
  }

  if (typeof aTarget == "string") {
    aTarget = aWindow.document.getElementById(aTarget);
  }

  var event = aWindow.document.createEvent("MouseEvent");

  var typeArg = aEvent.type;
  var canBubbleArg = true;
  var cancelableArg = true;
  var viewArg = aWindow;
  var detailArg =
    aEvent.detail ||
    // eslint-disable-next-line no-nested-ternary
    (aEvent.type == "click" ||
    aEvent.type == "mousedown" ||
    aEvent.type == "mouseup"
      ? 1
      : aEvent.type == "dblclick"
      ? 2
      : 0);
  var screenXArg = aEvent.screenX || 0;
  var screenYArg = aEvent.screenY || 0;
  var clientXArg = aEvent.clientX || 0;
  var clientYArg = aEvent.clientY || 0;
  var ctrlKeyArg = aEvent.ctrlKey || false;
  var altKeyArg = aEvent.altKey || false;
  var shiftKeyArg = aEvent.shiftKey || false;
  var metaKeyArg = aEvent.metaKey || false;
  var buttonArg = computeButton(aEvent);
  var relatedTargetArg = aEvent.relatedTarget || null;

  event.initMouseEvent(
    typeArg,
    canBubbleArg,
    cancelableArg,
    viewArg,
    detailArg,
    screenXArg,
    screenYArg,
    clientXArg,
    clientYArg,
    ctrlKeyArg,
    altKeyArg,
    shiftKeyArg,
    metaKeyArg,
    buttonArg,
    relatedTargetArg
  );

  // If documentURIObject exists or `window` is a stub object, we're in
  // a chrome scope, so don't bother trying to go through SpecialPowers.
  if (!window.document || window.document.documentURIObject) {
    return aTarget.dispatchEvent(event);
  }
  return SpecialPowers.dispatchEvent(aWindow, aTarget, event);
}

function isHidden(aElement) {
  var box = aElement.getBoundingClientRect();
  return box.width == 0 && box.height == 0;
}

/**
 * Send a drag event to the node aTarget (aTarget can be an id, or an
 * actual node) . The "event" passed in to aEvent is just a JavaScript
 * object with the properties set that the real drag event object should
 * have. This includes the type of the drag event.
 */
function sendDragEvent(aEvent, aTarget, aWindow = window) {
  if (
    ![
      "drag",
      "dragstart",
      "dragend",
      "dragover",
      "dragenter",
      "dragleave",
      "drop",
    ].includes(aEvent.type)
  ) {
    throw new Error(
      "sendDragEvent doesn't know about event type '" + aEvent.type + "'"
    );
  }

  if (typeof aTarget == "string") {
    aTarget = aWindow.document.getElementById(aTarget);
  }

  /*
   * Drag event cannot be performed if the element is hidden, except 'dragend'
   * event where the element can becomes hidden after start dragging.
   */
  if (aEvent.type != "dragend" && isHidden(aTarget)) {
    var targetName = aTarget.nodeName;
    if ("id" in aTarget && aTarget.id) {
      targetName += "#" + aTarget.id;
    }
    throw new Error(`${aEvent.type} event target ${targetName} is hidden`);
  }

  var event = aWindow.document.createEvent("DragEvent");

  var typeArg = aEvent.type;
  var canBubbleArg = true;
  var cancelableArg = true;
  var viewArg = aWindow;
  var detailArg = aEvent.detail || 0;
  var screenXArg = aEvent.screenX || 0;
  var screenYArg = aEvent.screenY || 0;
  var clientXArg = aEvent.clientX || 0;
  var clientYArg = aEvent.clientY || 0;
  var ctrlKeyArg = aEvent.ctrlKey || false;
  var altKeyArg = aEvent.altKey || false;
  var shiftKeyArg = aEvent.shiftKey || false;
  var metaKeyArg = aEvent.metaKey || false;
  var buttonArg = computeButton(aEvent);
  var relatedTargetArg = aEvent.relatedTarget || null;
  var dataTransfer = aEvent.dataTransfer || null;

  event.initDragEvent(
    typeArg,
    canBubbleArg,
    cancelableArg,
    viewArg,
    detailArg,
    Math.round(screenXArg),
    Math.round(screenYArg),
    Math.round(clientXArg),
    Math.round(clientYArg),
    ctrlKeyArg,
    altKeyArg,
    shiftKeyArg,
    metaKeyArg,
    buttonArg,
    relatedTargetArg,
    dataTransfer
  );

  if (aEvent._domDispatchOnly) {
    return aTarget.dispatchEvent(event);
  }

  var utils = _getDOMWindowUtils(aWindow);
  return utils.dispatchDOMEventViaPresShellForTesting(aTarget, event);
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
    case '"':
    case "|":
    case "<":
    case ">":
    case "?":
      hasShift = true;
      break;
    default:
      hasShift =
        aChar.toLowerCase() != aChar.toUpperCase() &&
        aChar == aChar.toUpperCase();
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
  for (let i = 0; i < aStr.length; ++i) {
    // Do not split a surrogate pair to call synthesizeKey.  Dispatching two
    // sets of keydown and keyup caused by two calls of synthesizeKey is not
    // good behavior.  It could happen due to a bug, but a surrogate pair should
    // be introduced with one key press operation.  Therefore, calling it with
    // a surrogate pair is the right thing.
    // Note that TextEventDispatcher will consider whether a surrogate pair
    // should cause one or two keypress events automatically.  Therefore, we
    // don't need to check the related prefs here.
    if (
      (aStr.charCodeAt(i) & 0xfc00) == 0xd800 &&
      i + 1 < aStr.length &&
      (aStr.charCodeAt(i + 1) & 0xfc00) == 0xdc00
    ) {
      sendChar(aStr.substring(i, i + 2), aWindow);
      i++;
    } else {
      sendChar(aStr.charAt(i), aWindow);
    }
  }
}

/**
 * Send the non-character key aKey to the focused node.
 * The name of the key should be the part that comes after ``DOM_VK_`` in the
 * KeyEvent constant name for this key.
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
function _parseModifiers(aEvent, aWindow = window) {
  var nsIDOMWindowUtils = _EU_Ci.nsIDOMWindowUtils;
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
    mval |= _EU_isMac(aWindow)
      ? nsIDOMWindowUtils.MODIFIER_META
      : nsIDOMWindowUtils.MODIFIER_CONTROL;
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

  return mval;
}

/**
 * Synthesize a mouse event on a target. The actual client point is determined
 * by taking the aTarget's client box and offseting it by aOffsetX and
 * aOffsetY. This allows mouse clicks to be simulated by calling this method.
 *
 * aEvent is an object which may contain the properties:
 *   `shiftKey`, `ctrlKey`, `altKey`, `metaKey`, `accessKey`, `clickCount`,
 *   `button`, `type`.
 *   For valid `type`s see nsIDOMWindowUtils' `sendMouseEvent`.
 *
 * If the type is specified, an mouse event of that type is fired. Otherwise,
 * a mousedown followed by a mouseup is performed.
 *
 * aWindow is optional, and defaults to the current window object.
 *
 * Returns whether the event had preventDefault() called on it.
 */
function synthesizeMouse(aTarget, aOffsetX, aOffsetY, aEvent, aWindow) {
  var rect = aTarget.getBoundingClientRect();
  return synthesizeMouseAtPoint(
    rect.left + aOffsetX,
    rect.top + aOffsetY,
    aEvent,
    aWindow
  );
}

/**
 * Synthesize one or more touches on aTarget. aTarget can be either Element
 * or Array of Elements.  aOffsetX, aOffsetY, aEvent.id, aEvent.rx, aEvent.ry,
 * aEvent.angle, aEvent.force, aEvent.tiltX, aEvent.tiltY and aEvent.twist can
 * be either Number or Array of Numbers (can be mixed).  If you specify array
 * to synthesize a multi-touch, you need to specify same length arrays.  If
 * you don't specify array to them, same values (or computed default values for
 * aEvent.id) are used for all touches.
 *
 * @param {Element | Element[]} aTarget The target element which you specify
 * relative offset from its top-left.
 * @param {Number | Number[]} aOffsetX The relative offset from left of aTarget.
 * @param {Number | Number[]} aOffsetY The relative offset from top of aTarget.
 * @param {Object} aEvent
 * type: The touch event type.  If undefined, "touchstart" and "touchend" will
 * be synthesized at same point.
 *
 * id: The touch id.  If you don't specify this, default touch id will be used
 * for first touch and further touch ids are the values incremented from the
 * first id.
 *
 * rx, ry: The radii of the touch.
 *
 * angle: The angle in degree.
 *
 * force: The force of the touch.  If the type is "touchend", this should be 0.
 * If unspecified, this is default to 0 for "touchend"  or 1 for the others.
 *
 * tiltX, tiltY: The tilt of the touch.
 *
 * twist: The twist of the touch.
 * @param {Window} aWindow Default to `window`.
 * @returns true if and only if aEvent.type is specified and default of the
 * event is prevented.
 */
function synthesizeTouch(
  aTarget,
  aOffsetX,
  aOffsetY,
  aEvent = {},
  aWindow = window
) {
  let rectX, rectY;
  if (Array.isArray(aTarget)) {
    let lastTarget, lastTargetRect;
    aTarget.forEach(target => {
      const rect =
        target == lastTarget ? lastTargetRect : target.getBoundingClientRect();
      rectX.push(rect.left);
      rectY.push(rect.top);
      lastTarget = target;
      lastTargetRect = rect;
    });
  } else {
    const rect = aTarget.getBoundingClientRect();
    rectX = [rect.left];
    rectY = [rect.top];
  }
  const offsetX = (() => {
    if (Array.isArray(aOffsetX)) {
      let ret = [];
      aOffsetX.forEach((value, index) => {
        ret.push(value + rectX[Math.min(index, rectX.length - 1)]);
      });
      return ret;
    }
    return aOffsetX + rectX[0];
  })();
  const offsetY = (() => {
    if (Array.isArray(aOffsetY)) {
      let ret = [];
      aOffsetY.forEach((value, index) => {
        ret.push(value + rectY[Math.min(index, rectY.length - 1)]);
      });
      return ret;
    }
    return aOffsetY + rectY[0];
  })();
  return synthesizeTouchAtPoint(offsetX, offsetY, aEvent, aWindow);
}

/**
 * Return the drag service.  Note that if we're in the headless mode, this
 * may return null because the service may be never instantiated (e.g., on
 * Linux).
 */
function getDragService() {
  try {
    return _EU_Cc["@mozilla.org/widget/dragservice;1"].getService(
      _EU_Ci.nsIDragService
    );
  } catch (e) {
    // If we're in the headless mode, the drag service may be never
    // instantiated.  In this case, an exception is thrown.  Let's ignore
    // any exceptions since without the drag service, nobody can create a
    // drag session.
    return null;
  }
}

/**
 * End drag session if there is.
 *
 * TODO: This should synthesize "drop" if necessary.
 *
 * @param left          X offset in the viewport
 * @param top           Y offset in the viewport
 * @param aEvent        The event data, the modifiers are applied to the
 *                      "dragend" event.
 * @param aWindow       The window.
 * @return              true if handled.  In this case, the caller should not
 *                      synthesize DOM events basically.
 */
function _maybeEndDragSession(left, top, aEvent, aWindow) {
  let utils = _getDOMWindowUtils(aWindow);
  const dragSession = utils.dragSession;
  if (!dragSession) {
    return false;
  }
  // FIXME: If dragSession.dragAction is not
  // nsIDragService.DRAGDROP_ACTION_NONE nor aEvent.type is not `keydown`, we
  // need to synthesize a "drop" event or call setDragEndPointForTests here to
  // set proper left/top to `dragend` event.
  try {
    dragSession.endDragSession(false, _parseModifiers(aEvent, aWindow));
  } catch (e) {}
  return true;
}

function _maybeSynthesizeDragOver(left, top, aEvent, aWindow) {
  let utils = _getDOMWindowUtils(aWindow);
  const dragSession = utils.dragSession;
  if (!dragSession) {
    return false;
  }
  const target = aWindow.document.elementFromPoint(left, top);
  if (target) {
    sendDragEvent(
      createDragEventObject(
        "dragover",
        target,
        aWindow,
        dragSession.dataTransfer,
        {
          accelKey: aEvent.accelKey,
          altKey: aEvent.altKey,
          altGrKey: aEvent.altGrKey,
          ctrlKey: aEvent.ctrlKey,
          metaKey: aEvent.metaKey,
          shiftKey: aEvent.shiftKey,
          capsLockKey: aEvent.capsLockKey,
          fnKey: aEvent.fnKey,
          fnLockKey: aEvent.fnLockKey,
          numLockKey: aEvent.numLockKey,
          scrollLockKey: aEvent.scrollLockKey,
          symbolKey: aEvent.symbolKey,
          symbolLockKey: aEvent.symbolLockKey,
        }
      ),
      target,
      aWindow
    );
  }
  return true;
}

/*
 * Synthesize a mouse event at a particular point in aWindow.
 *
 * aEvent is an object which may contain the properties:
 *   `shiftKey`, `ctrlKey`, `altKey`, `metaKey`, `accessKey`, `clickCount`,
 *   `button`, `type`.
 *   For valid `type`s see nsIDOMWindowUtils' `sendMouseEvent`.
 *
 * If the type is specified, an mouse event of that type is fired. Otherwise,
 * a mousedown followed by a mouseup is performed.
 *
 * aWindow is optional, and defaults to the current window object.
 */
function synthesizeMouseAtPoint(left, top, aEvent, aWindow = window) {
  if (aEvent.allowToHandleDragDrop) {
    if (aEvent.type == "mouseup" || !aEvent.type) {
      if (_maybeEndDragSession(left, top, aEvent, aWindow)) {
        return false;
      }
    } else if (aEvent.type == "mousemove") {
      if (_maybeSynthesizeDragOver(left, top, aEvent, aWindow)) {
        return false;
      }
    }
  }

  var utils = _getDOMWindowUtils(aWindow);
  var defaultPrevented = false;

  if (utils) {
    var button = computeButton(aEvent);
    var clickCount = aEvent.clickCount || 1;
    var modifiers = _parseModifiers(aEvent, aWindow);
    var pressure = "pressure" in aEvent ? aEvent.pressure : 0;

    // aWindow might be cross-origin from us.
    var MouseEvent = _EU_maybeWrap(aWindow).MouseEvent;

    // Default source to mouse.
    var inputSource =
      "inputSource" in aEvent
        ? aEvent.inputSource
        : MouseEvent.MOZ_SOURCE_MOUSE;
    // Compute a pointerId if needed.
    var id;
    if ("id" in aEvent) {
      id = aEvent.id;
    } else {
      var isFromPen = inputSource === MouseEvent.MOZ_SOURCE_PEN;
      id = isFromPen
        ? utils.DEFAULT_PEN_POINTER_ID
        : utils.DEFAULT_MOUSE_POINTER_ID;
    }

    var isDOMEventSynthesized =
      "isSynthesized" in aEvent ? aEvent.isSynthesized : true;
    var isWidgetEventSynthesized =
      "isWidgetEventSynthesized" in aEvent
        ? aEvent.isWidgetEventSynthesized
        : false;
    if ("type" in aEvent && aEvent.type) {
      defaultPrevented = utils.sendMouseEvent(
        aEvent.type,
        left,
        top,
        button,
        clickCount,
        modifiers,
        false,
        pressure,
        inputSource,
        isDOMEventSynthesized,
        isWidgetEventSynthesized,
        computeButtons(aEvent, utils),
        id
      );
    } else {
      utils.sendMouseEvent(
        "mousedown",
        left,
        top,
        button,
        clickCount,
        modifiers,
        false,
        pressure,
        inputSource,
        isDOMEventSynthesized,
        isWidgetEventSynthesized,
        computeButtons(Object.assign({ type: "mousedown" }, aEvent), utils),
        id
      );
      utils.sendMouseEvent(
        "mouseup",
        left,
        top,
        button,
        clickCount,
        modifiers,
        false,
        pressure,
        inputSource,
        isDOMEventSynthesized,
        isWidgetEventSynthesized,
        computeButtons(Object.assign({ type: "mouseup" }, aEvent), utils),
        id
      );
    }
  }

  return defaultPrevented;
}

/**
 * Synthesize one or more touches at the points. aLeft, aTop, aEvent.id,
 * aEvent.rx, aEvent.ry, aEvent.angle, aEvent.force, aEvent.tiltX, aEvent.tiltY
 * and aEvent.twist can be either Number or Array of Numbers (can be mixed).
 * If you specify array to synthesize a multi-touch, you need to specify same
 * length arrays.  If you don't specify array to them, same values are used for
 * all touches.
 *
 * @param {Element | Element[]} aTarget The target element which you specify
 * relative offset from its top-left.
 * @param {Number | Number[]} aOffsetX The relative offset from left of aTarget.
 * @param {Number | Number[]} aOffsetY The relative offset from top of aTarget.
 * @param {Object} aEvent
 * type: The touch event type.  If undefined, "touchstart" and "touchend" will
 * be synthesized at same point.
 *
 * id: The touch id.  If you don't specify this, default touch id will be used
 * for first touch and further touch ids are the values incremented from the
 * first id.
 *
 * rx, ry: The radii of the touch.
 *
 * angle: The angle in degree.
 *
 * force: The force of the touch.  If the type is "touchend", this should be 0.
 * If unspecified, this is default to 0 for "touchend"  or 1 for the others.
 *
 * tiltX, tiltY: The tilt of the touch.
 *
 * twist: The twist of the touch.
 * @param {Window} aWindow Default to `window`.
 * @returns true if and only if aEvent.type is specified and default of the
 * event is prevented.
 */
function synthesizeTouchAtPoint(aLeft, aTop, aEvent = {}, aWindow = window) {
  let utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return false;
  }

  if (
    Array.isArray(aLeft) &&
    Array.isArray(aTop) &&
    aLeft.length != aTop.length
  ) {
    throw new Error(`aLeft and aTop should be same length array`);
  }

  const arrayLength = Array.isArray(aLeft)
    ? aLeft.length
    : Array.isArray(aTop)
    ? aTop.length
    : 1;

  function throwExceptionIfDifferentLengthArray(aArray, aName) {
    if (Array.isArray(aArray) && arrayLength !== aArray.length) {
      throw new Error(`${aName} is different length array`);
    }
  }
  const leftArray = (() => {
    if (Array.isArray(aLeft)) {
      return aLeft;
    }
    return new Array(arrayLength).fill(aLeft);
  })();
  const topArray = (() => {
    if (Array.isArray(aTop)) {
      throwExceptionIfDifferentLengthArray(aTop, "aTop");
      return aTop;
    }
    return new Array(arrayLength).fill(aTop);
  })();
  const idArray = (() => {
    if ("id" in aEvent && Array.isArray(aEvent.id)) {
      throwExceptionIfDifferentLengthArray(aEvent.id, "aEvent.id");
      return aEvent.id;
    }
    let id = aEvent.id || utils.DEFAULT_TOUCH_POINTER_ID;
    let ret = [];
    for (let i = 0; i < arrayLength; i++) {
      ret.push(id++);
    }
    return ret;
  })();
  function getSameLengthArrayOfEventProperty(aProperty, aDefaultValue) {
    if (aProperty in aEvent && Array.isArray(aEvent[aProperty])) {
      throwExceptionIfDifferentLengthArray(
        aEvent.rx,
        arrayLength,
        `aEvent.${aProperty}`
      );
      return aEvent[aProperty];
    }
    return new Array(arrayLength).fill(aEvent[aProperty] || aDefaultValue);
  }
  const rxArray = getSameLengthArrayOfEventProperty("rx", 1);
  const ryArray = getSameLengthArrayOfEventProperty("ry", 1);
  const angleArray = getSameLengthArrayOfEventProperty("angle", 0);
  const forceArray = getSameLengthArrayOfEventProperty(
    "force",
    aEvent.type === "touchend" ? 0 : 1
  );
  const tiltXArray = getSameLengthArrayOfEventProperty("tiltX", 0);
  const tiltYArray = getSameLengthArrayOfEventProperty("tiltY", 0);
  const twistArray = getSameLengthArrayOfEventProperty("twist", 0);

  const modifiers = _parseModifiers(aEvent, aWindow);

  const args = [
    idArray,
    leftArray,
    topArray,
    rxArray,
    ryArray,
    angleArray,
    forceArray,
    tiltXArray,
    tiltYArray,
    twistArray,
    modifiers,
  ];

  const sender =
    aEvent.mozInputSource === "pen" ? "sendTouchEventAsPen" : "sendTouchEvent";

  if ("type" in aEvent && aEvent.type) {
    return utils[sender](aEvent.type, ...args);
  }

  utils[sender]("touchstart", ...args);
  utils[sender]("touchend", ...args);
  return false;
}

// Call synthesizeMouse with coordinates at the center of aTarget.
function synthesizeMouseAtCenter(aTarget, aEvent, aWindow) {
  var rect = aTarget.getBoundingClientRect();
  return synthesizeMouse(
    aTarget,
    rect.width / 2,
    rect.height / 2,
    aEvent,
    aWindow
  );
}
function synthesizeTouchAtCenter(aTarget, aEvent = {}, aWindow = window) {
  var rect = aTarget.getBoundingClientRect();
  synthesizeTouchAtPoint(
    rect.left + rect.width / 2,
    rect.top + rect.height / 2,
    aEvent,
    aWindow
  );
}

/**
 * Synthesize a wheel event without flush layout at a particular point in
 * aWindow.
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
function synthesizeWheelAtPoint(aLeft, aTop, aEvent, aWindow = window) {
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return;
  }

  var modifiers = _parseModifiers(aEvent, aWindow);
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
    // eslint-disable-next-line no-nested-ternary
    aEvent.lineOrPageDeltaX != null
      ? aEvent.lineOrPageDeltaX
      : aEvent.deltaX > 0
      ? Math.floor(aEvent.deltaX)
      : Math.ceil(aEvent.deltaX);
  var lineOrPageDeltaY =
    // eslint-disable-next-line no-nested-ternary
    aEvent.lineOrPageDeltaY != null
      ? aEvent.lineOrPageDeltaY
      : aEvent.deltaY > 0
      ? Math.floor(aEvent.deltaY)
      : Math.ceil(aEvent.deltaY);
  utils.sendWheelEvent(
    aLeft,
    aTop,
    aEvent.deltaX,
    aEvent.deltaY,
    aEvent.deltaZ,
    aEvent.deltaMode,
    modifiers,
    lineOrPageDeltaX,
    lineOrPageDeltaY,
    options
  );
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
function synthesizeWheel(aTarget, aOffsetX, aOffsetY, aEvent, aWindow) {
  var rect = aTarget.getBoundingClientRect();
  synthesizeWheelAtPoint(
    rect.left + aOffsetX,
    rect.top + aOffsetY,
    aEvent,
    aWindow
  );
}

const _FlushModes = {
  FLUSH: 0,
  NOFLUSH: 1,
};

function _sendWheelAndPaint(
  aTarget,
  aOffsetX,
  aOffsetY,
  aEvent,
  aCallback,
  aFlushMode = _FlushModes.FLUSH,
  aWindow = window
) {
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return;
  }

  if (utils.isMozAfterPaintPending) {
    // If a paint is pending, then APZ may be waiting for a scroll acknowledgement
    // from the content thread. If we send a wheel event now, it could be ignored
    // by APZ (or its scroll offset could be overridden). To avoid problems we
    // just wait for the paint to complete.
    aWindow.waitForAllPaintsFlushed(function () {
      _sendWheelAndPaint(
        aTarget,
        aOffsetX,
        aOffsetY,
        aEvent,
        aCallback,
        aFlushMode,
        aWindow
      );
    });
    return;
  }

  var onwheel = function () {
    SpecialPowers.wrap(window).removeEventListener("wheel", onwheel, {
      mozSystemGroup: true,
    });

    // Wait one frame since the wheel event has not caused a refresh observer
    // to be added yet.
    setTimeout(function () {
      utils.advanceTimeAndRefresh(1000);

      if (!aCallback) {
        utils.advanceTimeAndRefresh(0);
        return;
      }

      var waitForPaints = function () {
        SpecialPowers.Services.obs.removeObserver(
          waitForPaints,
          "apz-repaints-flushed"
        );
        aWindow.waitForAllPaintsFlushed(function () {
          utils.restoreNormalRefresh();
          aCallback();
        });
      };

      SpecialPowers.Services.obs.addObserver(
        waitForPaints,
        "apz-repaints-flushed"
      );
      if (!utils.flushApzRepaints(aWindow)) {
        waitForPaints();
      }
    }, 0);
  };

  // Listen for the system wheel event, because it happens after all of
  // the other wheel events, including legacy events.
  SpecialPowers.wrap(aWindow).addEventListener("wheel", onwheel, {
    mozSystemGroup: true,
  });
  if (aFlushMode === _FlushModes.FLUSH) {
    synthesizeWheel(aTarget, aOffsetX, aOffsetY, aEvent, aWindow);
  } else {
    synthesizeWheelAtPoint(aOffsetX, aOffsetY, aEvent, aWindow);
  }
}

/**
 * This is a wrapper around synthesizeWheel that waits for the wheel event
 * to be dispatched and for the subsequent layout/paints to be flushed.
 *
 * This requires including paint_listener.js. Tests must call
 * DOMWindowUtils.restoreNormalRefresh() before finishing, if they use this
 * function.
 *
 * If no callback is provided, the caller is assumed to have its own method of
 * determining scroll completion and the refresh driver is not automatically
 * restored.
 */
function sendWheelAndPaint(
  aTarget,
  aOffsetX,
  aOffsetY,
  aEvent,
  aCallback,
  aWindow = window
) {
  _sendWheelAndPaint(
    aTarget,
    aOffsetX,
    aOffsetY,
    aEvent,
    aCallback,
    _FlushModes.FLUSH,
    aWindow
  );
}

/**
 * Similar to sendWheelAndPaint but without flushing layout for obtaining
 * ``aTarget`` position in ``aWindow`` before sending the wheel event.
 * ``aOffsetX`` and ``aOffsetY`` should be offsets against aWindow.
 */
function sendWheelAndPaintNoFlush(
  aTarget,
  aOffsetX,
  aOffsetY,
  aEvent,
  aCallback,
  aWindow = window
) {
  _sendWheelAndPaint(
    aTarget,
    aOffsetX,
    aOffsetY,
    aEvent,
    aCallback,
    _FlushModes.NOFLUSH,
    aWindow
  );
}

function synthesizeNativeTapAtCenter(
  aTarget,
  aLongTap = false,
  aCallback = null,
  aWindow = window
) {
  let rect = aTarget.getBoundingClientRect();
  return synthesizeNativeTap(
    aTarget,
    rect.width / 2,
    rect.height / 2,
    aLongTap,
    aCallback,
    aWindow
  );
}

function synthesizeNativeTap(
  aTarget,
  aOffsetX,
  aOffsetY,
  aLongTap = false,
  aCallback = null,
  aWindow = window
) {
  let utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return;
  }

  let scale = aWindow.devicePixelRatio;
  let rect = aTarget.getBoundingClientRect();
  let x = (aWindow.mozInnerScreenX + rect.left + aOffsetX) * scale;
  let y = (aWindow.mozInnerScreenY + rect.top + aOffsetY) * scale;

  let observer = {
    observe: (subject, topic, data) => {
      if (aCallback && topic == "mouseevent") {
        aCallback(data);
      }
    },
  };
  utils.sendNativeTouchTap(x, y, aLongTap, observer);
}

/**
 * Similar to synthesizeMouse but generates a native widget level event
 * (so will actually move the "real" mouse cursor etc. Be careful because
 * this can impact later code as well! (e.g. with hover states etc.)
 *
 * @description There are 3 mutually exclusive ways of indicating the location of the
 * mouse event: set ``atCenter``, or pass ``offsetX`` and ``offsetY``,
 * or pass ``screenX`` and ``screenY``. Do not attempt to mix these.
 *
 * @param {object} aParams
 * @param {string} aParams.type "click", "mousedown", "mouseup" or "mousemove"
 * @param {Element} aParams.target Origin of offsetX and offsetY, must be an element
 * @param {Boolean} [aParams.atCenter]
 *        Instead of offsetX/Y, synthesize the event at center of `target`.
 * @param {Number} [aParams.offsetX]
 *        X offset in `target` (in CSS pixels if `scale` is "screenPixelsPerCSSPixel")
 * @param {Number} [aParams.offsetY]
 *        Y offset in `target` (in CSS pixels if `scale` is "screenPixelsPerCSSPixel")
 * @param {Number} [aParams.screenX]
 *        X offset in screen (in CSS pixels if `scale` is "screenPixelsPerCSSPixel"),
 *        Neither offsetX/Y nor atCenter must be set if this is set.
 * @param {Number} [aParams.screenY]
 *        Y offset in screen (in CSS pixels if `scale` is "screenPixelsPerCSSPixel"),
 *        Neither offsetX/Y nor atCenter must be set if this is set.
 * @param {String} [aParams.scale="screenPixelsPerCSSPixel"]
 *        If scale is "screenPixelsPerCSSPixel", devicePixelRatio will be used.
 *        If scale is "inScreenPixels", clientX/Y nor scaleX/Y are not adjusted with screenPixelsPerCSSPixel.
 * @param {Number} [aParams.button=0]
 *        Defaults to 0, if "click", "mousedown", "mouseup", set same value as DOM MouseEvent.button
 * @param {Object} [aParams.modifiers={}]
 *        Active modifiers, see `_parseNativeModifiers`
 * @param {Window} [aParams.win=window]
 *        The window to use its utils. Defaults to the window in which EventUtils.js is running.
 * @param {Element} [aParams.elementOnWidget=target]
 *        Defaults to target. If element under the point is in another widget from target's widget,
 *        e.g., when it's in a XUL <panel>, specify this.
 */
function synthesizeNativeMouseEvent(aParams, aCallback = null) {
  const {
    type,
    target,
    offsetX,
    offsetY,
    atCenter,
    screenX,
    screenY,
    scale = "screenPixelsPerCSSPixel",
    button = 0,
    modifiers = {},
    win = window,
    elementOnWidget = target,
  } = aParams;
  if (atCenter) {
    if (offsetX != undefined || offsetY != undefined) {
      throw Error(
        `atCenter is specified, but offsetX (${offsetX}) and/or offsetY (${offsetY}) are also specified`
      );
    }
    if (screenX != undefined || screenY != undefined) {
      throw Error(
        `atCenter is specified, but screenX (${screenX}) and/or screenY (${screenY}) are also specified`
      );
    }
    if (!target) {
      throw Error("atCenter is specified, but target is not specified");
    }
  } else if (offsetX != undefined && offsetY != undefined) {
    if (screenX != undefined || screenY != undefined) {
      throw Error(
        `offsetX/Y are specified, but screenX (${screenX}) and/or screenY (${screenY}) are also specified`
      );
    }
    if (!target) {
      throw Error(
        "offsetX and offsetY are specified, but target is not specified"
      );
    }
  } else if (screenX != undefined && screenY != undefined) {
    if (offsetX != undefined || offsetY != undefined) {
      throw Error(
        `screenX/Y are specified, but offsetX (${offsetX}) and/or offsetY (${offsetY}) are also specified`
      );
    }
  }
  const utils = _getDOMWindowUtils(win);
  if (!utils) {
    return;
  }

  const rect = target?.getBoundingClientRect();
  let resolution = 1.0;
  try {
    resolution = _getDOMWindowUtils(win.top).getResolution();
  } catch (e) {
    // XXX How to get mobile viewport scale on Fission+xorigin since
    //     window.top access isn't allowed due to cross-origin?
  }
  const scaleValue = (() => {
    if (scale === "inScreenPixels") {
      return 1.0;
    }
    if (scale === "screenPixelsPerCSSPixel") {
      return win.devicePixelRatio;
    }
    throw Error(`invalid scale value (${scale}) is specified`);
  })();
  // XXX mozInnerScreen might be invalid value on mobile viewport (Bug 1701546),
  //     so use window.top's mozInnerScreen. But this won't work fission+xorigin
  //     with mobile viewport until mozInnerScreen returns valid value with
  //     scale.
  const x = (() => {
    if (screenX != undefined) {
      return screenX * scaleValue;
    }
    let winInnerOffsetX = win.mozInnerScreenX;
    try {
      winInnerOffsetX =
        win.top.mozInnerScreenX +
        (win.mozInnerScreenX - win.top.mozInnerScreenX) * resolution;
    } catch (e) {
      // XXX fission+xorigin test throws permission denied since win.top is
      //     cross-origin.
    }
    return (
      (((atCenter ? rect.width / 2 : offsetX) + rect.left) * resolution +
        winInnerOffsetX) *
      scaleValue
    );
  })();
  const y = (() => {
    if (screenY != undefined) {
      return screenY * scaleValue;
    }
    let winInnerOffsetY = win.mozInnerScreenY;
    try {
      winInnerOffsetY =
        win.top.mozInnerScreenY +
        (win.mozInnerScreenY - win.top.mozInnerScreenY) * resolution;
    } catch (e) {
      // XXX fission+xorigin test throws permission denied since win.top is
      //     cross-origin.
    }
    return (
      (((atCenter ? rect.height / 2 : offsetY) + rect.top) * resolution +
        winInnerOffsetY) *
      scaleValue
    );
  })();
  const modifierFlags = _parseNativeModifiers(modifiers);

  const observer = {
    observe: (subject, topic, data) => {
      if (aCallback && topic == "mouseevent") {
        aCallback(data);
      }
    },
  };
  if (type === "click") {
    utils.sendNativeMouseEvent(
      x,
      y,
      utils.NATIVE_MOUSE_MESSAGE_BUTTON_DOWN,
      button,
      modifierFlags,
      elementOnWidget,
      function () {
        utils.sendNativeMouseEvent(
          x,
          y,
          utils.NATIVE_MOUSE_MESSAGE_BUTTON_UP,
          button,
          modifierFlags,
          elementOnWidget,
          observer
        );
      }
    );
    return;
  }
  utils.sendNativeMouseEvent(
    x,
    y,
    (() => {
      switch (type) {
        case "mousedown":
          return utils.NATIVE_MOUSE_MESSAGE_BUTTON_DOWN;
        case "mouseup":
          return utils.NATIVE_MOUSE_MESSAGE_BUTTON_UP;
        case "mousemove":
          return utils.NATIVE_MOUSE_MESSAGE_MOVE;
        default:
          throw Error(`Invalid type is specified: ${type}`);
      }
    })(),
    button,
    modifierFlags,
    elementOnWidget,
    observer
  );
}

function promiseNativeMouseEvent(aParams) {
  return new Promise(resolve => synthesizeNativeMouseEvent(aParams, resolve));
}

function synthesizeNativeMouseEventAndWaitForEvent(aParams, aCallback) {
  const listener = aParams.eventTargetToListen || aParams.target;
  const eventType = aParams.eventTypeToWait || aParams.type;
  listener.addEventListener(eventType, aCallback, {
    capture: true,
    once: true,
  });
  synthesizeNativeMouseEvent(aParams);
}

function promiseNativeMouseEventAndWaitForEvent(aParams) {
  return new Promise(resolve =>
    synthesizeNativeMouseEventAndWaitForEvent(aParams, resolve)
  );
}

/**
 * This is a wrapper around synthesizeNativeMouseEvent that waits for the mouse
 * event to be dispatched to the target content.
 *
 * This API is supposed to be used in those test cases that synthesize some
 * input events to chrome process and have some checks in content.
 */
function synthesizeAndWaitNativeMouseMove(
  aTarget,
  aOffsetX,
  aOffsetY,
  aCallback,
  aWindow = window
) {
  let browser = gBrowser.selectedTab.linkedBrowser;
  let mm = browser.messageManager;
  let { ContentTask } = _EU_ChromeUtils.importESModule(
    "resource://testing-common/ContentTask.sys.mjs"
  );

  let eventRegisteredPromise = new Promise(resolve => {
    mm.addMessageListener("Test:MouseMoveRegistered", function processed() {
      mm.removeMessageListener("Test:MouseMoveRegistered", processed);
      resolve();
    });
  });
  let eventReceivedPromise = ContentTask.spawn(
    browser,
    [aOffsetX, aOffsetY],
    ([clientX, clientY]) => {
      return new Promise(resolve => {
        addEventListener("mousemove", function onMouseMoveEvent(e) {
          if (e.clientX == clientX && e.clientY == clientY) {
            removeEventListener("mousemove", onMouseMoveEvent);
            resolve();
          }
        });
        sendAsyncMessage("Test:MouseMoveRegistered");
      });
    }
  );
  eventRegisteredPromise.then(() => {
    synthesizeNativeMouseEvent({
      type: "mousemove",
      target: aTarget,
      offsetX: aOffsetX,
      offsetY: aOffsetY,
      win: aWindow,
    });
  });
  return eventReceivedPromise;
}

/**
 * Synthesize a key event. It is targeted at whatever would be targeted by an
 * actual keypress by the user, typically the focused element.
 *
 * @param {String} aKey
 *        Should be either:
 *
 *        - key value (recommended).  If you specify a non-printable key name,
 *          prepend the ``KEY_`` prefix.  Otherwise, specifying a printable key, the
 *          key value should be specified.
 *
 *        - keyCode name starting with ``VK_`` (e.g., ``VK_RETURN``).  This is available
 *          only for compatibility with legacy API.  Don't use this with new tests.
 *
 * @param {Object} [aEvent]
 *        Optional event object with more specifics about the key event to
 *        synthesize.
 * @param {String} [aEvent.code]
 *        If you don't specify this explicitly, it'll be guessed from aKey
 *        of US keyboard layout.  Note that this value may be different
 *        between browsers.  For example, "Insert" is never set only on
 *        macOS since actual key operation won't cause this code value.
 *        In such case, the value becomes empty string.
 *        If you need to emulate non-US keyboard layout or virtual keyboard
 *        which doesn't emulate hardware key input, you should set this value
 *        to empty string explicitly.
 * @param {Number} [aEvent.repeat]
 *        If you emulate auto-repeat, you should set the count of repeat.
 *        This method will automatically synthesize keydown (and keypress).
 * @param {*} aEvent.location
 *        If you want to specify this, you can specify this explicitly.
 *        However, if you don't specify this value, it will be computed
 *        from code value.
 * @param {String} aEvent.type
 *        Basically, you shouldn't specify this.  Then, this function will
 *        synthesize keydown (, keypress) and keyup.
 *        If keydown is specified, this only fires keydown (and keypress if
 *        it should be fired).
 *        If keyup is specified, this only fires keyup.
 * @param {Number} aEvent.keyCode
 *        Must be 0 - 255 (0xFF). If this is specified explicitly,
 *        .keyCode value is initialized with this value.
 * @param {Window} aWindow
 *        Is optional and defaults to the current window object.
 * @param {Function} aCallback
 *        Is optional and can be used to receive notifications from TIP.
 *
 * @description
 * ``accelKey``, ``altKey``, ``altGraphKey``, ``ctrlKey``, ``capsLockKey``,
 * ``fnKey``, ``fnLockKey``, ``numLockKey``, ``metaKey``, ``scrollLockKey``,
 * ``shiftKey``, ``symbolKey``, ``symbolLockKey``
 * Basically, you shouldn't use these attributes.  nsITextInputProcessor
 * manages modifier key state when you synthesize modifier key events.
 * However, if some of these attributes are true, this function activates
 * the modifiers only during dispatching the key events.
 * Note that if some of these values are false, they are ignored (i.e.,
 * not inactivated with this function).
 *
 */
function synthesizeKey(aKey, aEvent = undefined, aWindow = window, aCallback) {
  const event = aEvent === undefined || aEvent === null ? {} : aEvent;
  let dispatchKeydown =
    !("type" in event) || event.type === "keydown" || !event.type;
  const dispatchKeyup =
    !("type" in event) || event.type === "keyup" || !event.type;

  if (dispatchKeydown && aKey == "KEY_Escape") {
    let eventForKeydown = Object.assign({}, JSON.parse(JSON.stringify(event)));
    eventForKeydown.type = "keydown";
    if (
      _maybeEndDragSession(
        // TODO: We should set the last dragover point instead
        0,
        0,
        eventForKeydown,
        aWindow
      )
    ) {
      if (!dispatchKeyup) {
        return;
      }
      // We don't need to dispatch only keydown event because it's consumed by
      // the drag session.
      dispatchKeydown = false;
    }
  }

  var TIP = _getTIP(aWindow, aCallback);
  if (!TIP) {
    return;
  }
  var KeyboardEvent = _getKeyboardEvent(aWindow);
  var modifiers = _emulateToActivateModifiers(TIP, event, aWindow);
  var keyEventDict = _createKeyboardEventDictionary(aKey, event, TIP, aWindow);
  var keyEvent = new KeyboardEvent("", keyEventDict.dictionary);

  try {
    if (dispatchKeydown) {
      TIP.keydown(keyEvent, keyEventDict.flags);
      if ("repeat" in event && event.repeat > 1) {
        keyEventDict.dictionary.repeat = true;
        var repeatedKeyEvent = new KeyboardEvent("", keyEventDict.dictionary);
        for (var i = 1; i < event.repeat; i++) {
          TIP.keydown(repeatedKeyEvent, keyEventDict.flags);
        }
      }
    }
    if (dispatchKeyup) {
      TIP.keyup(keyEvent, keyEventDict.flags);
    }
  } finally {
    _emulateToInactivateModifiers(TIP, modifiers, aWindow);
  }
}

/**
 * This is a wrapper around synthesizeKey that waits for the key event to be
 * dispatched to the target content. It returns a promise which is resolved
 * when the content receives the key event.
 *
 * This API is supposed to be used in those test cases that synthesize some
 * input events to chrome process and have some checks in content.
 */
function synthesizeAndWaitKey(
  aKey,
  aEvent,
  aWindow = window,
  checkBeforeSynthesize,
  checkAfterSynthesize
) {
  let browser = gBrowser.selectedTab.linkedBrowser;
  let mm = browser.messageManager;
  let keyCode = _createKeyboardEventDictionary(aKey, aEvent, null, aWindow)
    .dictionary.keyCode;
  let { ContentTask } = _EU_ChromeUtils.importESModule(
    "resource://testing-common/ContentTask.sys.mjs"
  );

  let keyRegisteredPromise = new Promise(resolve => {
    mm.addMessageListener("Test:KeyRegistered", function processed() {
      mm.removeMessageListener("Test:KeyRegistered", processed);
      resolve();
    });
  });
  // eslint-disable-next-line no-shadow
  let keyReceivedPromise = ContentTask.spawn(browser, keyCode, keyCode => {
    return new Promise(resolve => {
      addEventListener("keyup", function onKeyEvent(e) {
        if (e.keyCode == keyCode) {
          removeEventListener("keyup", onKeyEvent);
          resolve();
        }
      });
      sendAsyncMessage("Test:KeyRegistered");
    });
  });
  keyRegisteredPromise.then(() => {
    if (checkBeforeSynthesize) {
      checkBeforeSynthesize();
    }
    synthesizeKey(aKey, aEvent, aWindow);
    if (checkAfterSynthesize) {
      checkAfterSynthesize();
    }
  });
  return keyReceivedPromise;
}

function _parseNativeModifiers(aModifiers, aWindow = window) {
  let modifiers = 0;
  if (aModifiers.capsLockKey) {
    modifiers |= SpecialPowers.Ci.nsIDOMWindowUtils.NATIVE_MODIFIER_CAPS_LOCK;
  }
  if (aModifiers.numLockKey) {
    modifiers |= SpecialPowers.Ci.nsIDOMWindowUtils.NATIVE_MODIFIER_NUM_LOCK;
  }
  if (aModifiers.shiftKey) {
    modifiers |= SpecialPowers.Ci.nsIDOMWindowUtils.NATIVE_MODIFIER_SHIFT_LEFT;
  }
  if (aModifiers.shiftRightKey) {
    modifiers |= SpecialPowers.Ci.nsIDOMWindowUtils.NATIVE_MODIFIER_SHIFT_RIGHT;
  }
  if (aModifiers.ctrlKey) {
    modifiers |=
      SpecialPowers.Ci.nsIDOMWindowUtils.NATIVE_MODIFIER_CONTROL_LEFT;
  }
  if (aModifiers.ctrlRightKey) {
    modifiers |=
      SpecialPowers.Ci.nsIDOMWindowUtils.NATIVE_MODIFIER_CONTROL_RIGHT;
  }
  if (aModifiers.altKey) {
    modifiers |= SpecialPowers.Ci.nsIDOMWindowUtils.NATIVE_MODIFIER_ALT_LEFT;
  }
  if (aModifiers.altRightKey) {
    modifiers |= SpecialPowers.Ci.nsIDOMWindowUtils.NATIVE_MODIFIER_ALT_RIGHT;
  }
  if (aModifiers.metaKey) {
    modifiers |=
      SpecialPowers.Ci.nsIDOMWindowUtils.NATIVE_MODIFIER_COMMAND_LEFT;
  }
  if (aModifiers.metaRightKey) {
    modifiers |=
      SpecialPowers.Ci.nsIDOMWindowUtils.NATIVE_MODIFIER_COMMAND_RIGHT;
  }
  if (aModifiers.helpKey) {
    modifiers |= SpecialPowers.Ci.nsIDOMWindowUtils.NATIVE_MODIFIER_HELP;
  }
  if (aModifiers.fnKey) {
    modifiers |= SpecialPowers.Ci.nsIDOMWindowUtils.NATIVE_MODIFIER_FUNCTION;
  }
  if (aModifiers.numericKeyPadKey) {
    modifiers |=
      SpecialPowers.Ci.nsIDOMWindowUtils.NATIVE_MODIFIER_NUMERIC_KEY_PAD;
  }

  if (aModifiers.accelKey) {
    modifiers |= _EU_isMac(aWindow)
      ? SpecialPowers.Ci.nsIDOMWindowUtils.NATIVE_MODIFIER_COMMAND_LEFT
      : SpecialPowers.Ci.nsIDOMWindowUtils.NATIVE_MODIFIER_CONTROL_LEFT;
  }
  if (aModifiers.accelRightKey) {
    modifiers |= _EU_isMac(aWindow)
      ? SpecialPowers.Ci.nsIDOMWindowUtils.NATIVE_MODIFIER_COMMAND_RIGHT
      : SpecialPowers.Ci.nsIDOMWindowUtils.NATIVE_MODIFIER_CONTROL_RIGHT;
  }
  if (aModifiers.altGrKey) {
    modifiers |= _EU_isMac(aWindow)
      ? SpecialPowers.Ci.nsIDOMWindowUtils.NATIVE_MODIFIER_ALT_LEFT
      : SpecialPowers.Ci.nsIDOMWindowUtils.NATIVE_MODIFIER_ALT_GRAPH;
  }
  return modifiers;
}

// Mac: Any unused number is okay for adding new keyboard layout.
//      When you add new keyboard layout here, you need to modify
//      TISInputSourceWrapper::InitByLayoutID().
// Win: These constants can be found by inspecting registry keys under
//      HKEY_LOCAL_MACHINE\System\CurrentControlSet\Control\Keyboard Layouts

const KEYBOARD_LAYOUT_ARABIC = {
  name: "Arabic",
  Mac: 6,
  Win: 0x00000401,
  hasAltGrOnWin: false,
};
_defineConstant("KEYBOARD_LAYOUT_ARABIC", KEYBOARD_LAYOUT_ARABIC);
const KEYBOARD_LAYOUT_ARABIC_PC = {
  name: "Arabic - PC",
  Mac: 7,
  Win: null,
  hasAltGrOnWin: false,
};
_defineConstant("KEYBOARD_LAYOUT_ARABIC_PC", KEYBOARD_LAYOUT_ARABIC_PC);
const KEYBOARD_LAYOUT_BRAZILIAN_ABNT = {
  name: "Brazilian ABNT",
  Mac: null,
  Win: 0x00000416,
  hasAltGrOnWin: true,
};
_defineConstant(
  "KEYBOARD_LAYOUT_BRAZILIAN_ABNT",
  KEYBOARD_LAYOUT_BRAZILIAN_ABNT
);
const KEYBOARD_LAYOUT_DVORAK_QWERTY = {
  name: "Dvorak-QWERTY",
  Mac: 4,
  Win: null,
  hasAltGrOnWin: false,
};
_defineConstant("KEYBOARD_LAYOUT_DVORAK_QWERTY", KEYBOARD_LAYOUT_DVORAK_QWERTY);
const KEYBOARD_LAYOUT_EN_US = {
  name: "US",
  Mac: 0,
  Win: 0x00000409,
  hasAltGrOnWin: false,
};
_defineConstant("KEYBOARD_LAYOUT_EN_US", KEYBOARD_LAYOUT_EN_US);
const KEYBOARD_LAYOUT_FRENCH = {
  name: "French",
  Mac: 8, // Some keys mapped different from PC, e.g., Digit6, Digit8, Equal, Slash and Backslash
  Win: 0x0000040c,
  hasAltGrOnWin: true,
};
_defineConstant("KEYBOARD_LAYOUT_FRENCH", KEYBOARD_LAYOUT_FRENCH);
const KEYBOARD_LAYOUT_FRENCH_PC = {
  name: "French-PC",
  Mac: 13, // Compatible with Windows
  Win: 0x0000040c,
  hasAltGrOnWin: true,
};
_defineConstant("KEYBOARD_LAYOUT_FRENCH_PC", KEYBOARD_LAYOUT_FRENCH_PC);
const KEYBOARD_LAYOUT_GREEK = {
  name: "Greek",
  Mac: 1,
  Win: 0x00000408,
  hasAltGrOnWin: true,
};
_defineConstant("KEYBOARD_LAYOUT_GREEK", KEYBOARD_LAYOUT_GREEK);
const KEYBOARD_LAYOUT_GERMAN = {
  name: "German",
  Mac: 2,
  Win: 0x00000407,
  hasAltGrOnWin: true,
};
_defineConstant("KEYBOARD_LAYOUT_GERMAN", KEYBOARD_LAYOUT_GERMAN);
const KEYBOARD_LAYOUT_HEBREW = {
  name: "Hebrew",
  Mac: 9,
  Win: 0x0000040d,
  hasAltGrOnWin: true,
};
_defineConstant("KEYBOARD_LAYOUT_HEBREW", KEYBOARD_LAYOUT_HEBREW);
const KEYBOARD_LAYOUT_JAPANESE = {
  name: "Japanese",
  Mac: null,
  Win: 0x00000411,
  hasAltGrOnWin: false,
};
_defineConstant("KEYBOARD_LAYOUT_JAPANESE", KEYBOARD_LAYOUT_JAPANESE);
const KEYBOARD_LAYOUT_KHMER = {
  name: "Khmer",
  Mac: null,
  Win: 0x00000453,
  hasAltGrOnWin: true,
}; // available on Win7 or later.
_defineConstant("KEYBOARD_LAYOUT_KHMER", KEYBOARD_LAYOUT_KHMER);
const KEYBOARD_LAYOUT_LITHUANIAN = {
  name: "Lithuanian",
  Mac: 10,
  Win: 0x00010427,
  hasAltGrOnWin: true,
};
_defineConstant("KEYBOARD_LAYOUT_LITHUANIAN", KEYBOARD_LAYOUT_LITHUANIAN);
const KEYBOARD_LAYOUT_NORWEGIAN = {
  name: "Norwegian",
  Mac: 11,
  Win: 0x00000414,
  hasAltGrOnWin: true,
};
_defineConstant("KEYBOARD_LAYOUT_NORWEGIAN", KEYBOARD_LAYOUT_NORWEGIAN);
const KEYBOARD_LAYOUT_RUSSIAN = {
  name: "Russian",
  Mac: null,
  Win: 0x00000419,
  hasAltGrOnWin: true, // No AltGr, but Ctrl + Alt + Digit8 introduces a char
};
_defineConstant("KEYBOARD_LAYOUT_RUSSIAN", KEYBOARD_LAYOUT_RUSSIAN);
const KEYBOARD_LAYOUT_RUSSIAN_MNEMONIC = {
  name: "Russian - Mnemonic",
  Mac: null,
  Win: 0x00020419,
  hasAltGrOnWin: true,
}; // available on Win8 or later.
_defineConstant(
  "KEYBOARD_LAYOUT_RUSSIAN_MNEMONIC",
  KEYBOARD_LAYOUT_RUSSIAN_MNEMONIC
);
const KEYBOARD_LAYOUT_SPANISH = {
  name: "Spanish",
  Mac: 12,
  Win: 0x0000040a,
  hasAltGrOnWin: true,
};
_defineConstant("KEYBOARD_LAYOUT_SPANISH", KEYBOARD_LAYOUT_SPANISH);
const KEYBOARD_LAYOUT_SWEDISH = {
  name: "Swedish",
  Mac: 3,
  Win: 0x0000041d,
  hasAltGrOnWin: true,
};
_defineConstant("KEYBOARD_LAYOUT_SWEDISH", KEYBOARD_LAYOUT_SWEDISH);
const KEYBOARD_LAYOUT_THAI = {
  name: "Thai",
  Mac: 5,
  Win: 0x0002041e,
  hasAltGrOnWin: false,
};
_defineConstant("KEYBOARD_LAYOUT_THAI", KEYBOARD_LAYOUT_THAI);

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

function synthesizeNativeKey(
  aKeyboardLayout,
  aNativeKeyCode,
  aModifiers,
  aChars,
  aUnmodifiedChars,
  aCallback,
  aWindow = window
) {
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return false;
  }
  var nativeKeyboardLayout = null;
  if (_EU_isMac(aWindow)) {
    nativeKeyboardLayout = aKeyboardLayout.Mac;
  } else if (_EU_isWin(aWindow)) {
    nativeKeyboardLayout = aKeyboardLayout.Win;
  }
  if (nativeKeyboardLayout === null) {
    return false;
  }

  var observer = {
    observe(aSubject, aTopic, aData) {
      if (aCallback && aTopic == "keyevent") {
        aCallback(aData);
      }
    },
  };
  utils.sendNativeKeyEvent(
    nativeKeyboardLayout,
    aNativeKeyCode,
    _parseNativeModifiers(aModifiers, aWindow),
    aChars,
    aUnmodifiedChars,
    observer
  );
  return true;
}

var _gSeenEvent = false;

/**
 * Indicate that an event with an original target of aExpectedTarget and
 * a type of aExpectedEvent is expected to be fired, or not expected to
 * be fired.
 */
function _expectEvent(aExpectedTarget, aExpectedEvent, aTestName) {
  if (!aExpectedTarget || !aExpectedEvent) {
    return null;
  }

  _gSeenEvent = false;

  var type =
    aExpectedEvent.charAt(0) == "!"
      ? aExpectedEvent.substring(1)
      : aExpectedEvent;
  var eventHandler = function (event) {
    var epassed =
      !_gSeenEvent &&
      event.originalTarget == aExpectedTarget &&
      event.type == type;
    is(
      epassed,
      true,
      aTestName + " " + type + " event target " + (_gSeenEvent ? "twice" : "")
    );
    _gSeenEvent = true;
  };

  aExpectedTarget.addEventListener(type, eventHandler);
  return eventHandler;
}

/**
 * Check if the event was fired or not. The event handler aEventHandler
 * will be removed.
 */
function _checkExpectedEvent(
  aExpectedTarget,
  aExpectedEvent,
  aEventHandler,
  aTestName
) {
  if (aEventHandler) {
    var expectEvent = aExpectedEvent.charAt(0) != "!";
    var type = expectEvent ? aExpectedEvent : aExpectedEvent.substring(1);
    aExpectedTarget.removeEventListener(type, aEventHandler);
    var desc = type + " event";
    if (!expectEvent) {
      desc += " not";
    }
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
function synthesizeMouseExpectEvent(
  aTarget,
  aOffsetX,
  aOffsetY,
  aEvent,
  aExpectedTarget,
  aExpectedEvent,
  aTestName,
  aWindow
) {
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
function synthesizeKeyExpectEvent(
  key,
  aEvent,
  aExpectedTarget,
  aExpectedEvent,
  aTestName,
  aWindow
) {
  var eventHandler = _expectEvent(aExpectedTarget, aExpectedEvent, aTestName);
  synthesizeKey(key, aEvent, aWindow);
  _checkExpectedEvent(aExpectedTarget, aExpectedEvent, eventHandler, aTestName);
}

function disableNonTestMouseEvents(aDisable) {
  var domutils = _getDOMWindowUtils();
  domutils.disableNonTestMouseEvents(aDisable);
}

function _getDOMWindowUtils(aWindow = window) {
  // Leave this here as something, somewhere, passes a falsy argument
  // to this, causing the |window| default argument not to get picked up.
  if (!aWindow) {
    aWindow = window;
  }

  // If documentURIObject exists or `window` is a stub object, we're in
  // a chrome scope, so don't bother trying to go through SpecialPowers.
  if (!aWindow.document || aWindow.document.documentURIObject) {
    return aWindow.windowUtils;
  }

  // we need parent.SpecialPowers for:
  //  layout/base/tests/test_reftests_with_caret.html
  //  chrome: toolkit/content/tests/chrome/test_findbar.xul
  //  chrome: toolkit/content/tests/chrome/test_popup_anchor.xul
  if ("SpecialPowers" in aWindow && aWindow.SpecialPowers != undefined) {
    return aWindow.SpecialPowers.getDOMWindowUtils(aWindow);
  }
  if (
    "SpecialPowers" in aWindow.parent &&
    aWindow.parent.SpecialPowers != undefined
  ) {
    return aWindow.parent.SpecialPowers.getDOMWindowUtils(aWindow);
  }

  // TODO: this is assuming we are in chrome space
  return aWindow.windowUtils;
}

function _defineConstant(name, value) {
  Object.defineProperty(this, name, {
    value,
    enumerable: true,
    writable: false,
  });
}

const COMPOSITION_ATTR_RAW_CLAUSE =
  _EU_Ci.nsITextInputProcessor.ATTR_RAW_CLAUSE;
_defineConstant("COMPOSITION_ATTR_RAW_CLAUSE", COMPOSITION_ATTR_RAW_CLAUSE);
const COMPOSITION_ATTR_SELECTED_RAW_CLAUSE =
  _EU_Ci.nsITextInputProcessor.ATTR_SELECTED_RAW_CLAUSE;
_defineConstant(
  "COMPOSITION_ATTR_SELECTED_RAW_CLAUSE",
  COMPOSITION_ATTR_SELECTED_RAW_CLAUSE
);
const COMPOSITION_ATTR_CONVERTED_CLAUSE =
  _EU_Ci.nsITextInputProcessor.ATTR_CONVERTED_CLAUSE;
_defineConstant(
  "COMPOSITION_ATTR_CONVERTED_CLAUSE",
  COMPOSITION_ATTR_CONVERTED_CLAUSE
);
const COMPOSITION_ATTR_SELECTED_CLAUSE =
  _EU_Ci.nsITextInputProcessor.ATTR_SELECTED_CLAUSE;
_defineConstant(
  "COMPOSITION_ATTR_SELECTED_CLAUSE",
  COMPOSITION_ATTR_SELECTED_CLAUSE
);

var TIPMap = new WeakMap();

function _getTIP(aWindow, aCallback) {
  if (!aWindow) {
    aWindow = window;
  }
  var tip;
  if (TIPMap.has(aWindow)) {
    tip = TIPMap.get(aWindow);
  } else {
    tip = _EU_Cc["@mozilla.org/text-input-processor;1"].createInstance(
      _EU_Ci.nsITextInputProcessor
    );
    TIPMap.set(aWindow, tip);
  }
  if (!tip.beginInputTransactionForTests(aWindow, aCallback)) {
    tip = null;
    TIPMap.delete(aWindow);
  }
  return tip;
}

function _getKeyboardEvent(aWindow = window) {
  if (typeof KeyboardEvent != "undefined") {
    try {
      // See if the object can be instantiated; sometimes this yields
      // 'TypeError: can't access dead object' or 'KeyboardEvent is not a constructor'.
      new KeyboardEvent("", {});
      return KeyboardEvent;
    } catch (ex) {}
  }
  if (typeof content != "undefined" && "KeyboardEvent" in content) {
    return content.KeyboardEvent;
  }
  return aWindow.KeyboardEvent;
}

// eslint-disable-next-line complexity
function _guessKeyNameFromKeyCode(aKeyCode, aWindow = window) {
  var KeyboardEvent = _getKeyboardEvent(aWindow);
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
      return "AudioVolumeMute";
    case KeyboardEvent.DOM_VK_VOLUME_DOWN:
      return "AudioVolumeDown";
    case KeyboardEvent.DOM_VK_VOLUME_UP:
      return "AudioVolumeUp";
    case KeyboardEvent.DOM_VK_META:
      return "Meta";
    case KeyboardEvent.DOM_VK_ALTGR:
      return "AltGraph";
    case KeyboardEvent.DOM_VK_PROCESSKEY:
      return "Process";
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

function _createKeyboardEventDictionary(
  aKey,
  aKeyEvent,
  aTIP = null,
  aWindow = window
) {
  var result = { dictionary: null, flags: 0 };
  var keyCodeIsDefined = "keyCode" in aKeyEvent;
  var keyCode =
    keyCodeIsDefined && aKeyEvent.keyCode >= 0 && aKeyEvent.keyCode <= 255
      ? aKeyEvent.keyCode
      : 0;
  var keyName = "Unidentified";
  var code = aKeyEvent.code;
  if (!aTIP) {
    aTIP = _getTIP(aWindow);
  }
  if (aKey.indexOf("KEY_") == 0) {
    keyName = aKey.substr("KEY_".length);
    result.flags |= _EU_Ci.nsITextInputProcessor.KEY_NON_PRINTABLE_KEY;
    if (code === undefined) {
      code = aTIP.computeCodeValueOfNonPrintableKey(
        keyName,
        aKeyEvent.location
      );
    }
  } else if (aKey.indexOf("VK_") == 0) {
    keyCode = _getKeyboardEvent(aWindow)["DOM_" + aKey];
    if (!keyCode) {
      throw new Error("Unknown key: " + aKey);
    }
    keyName = _guessKeyNameFromKeyCode(keyCode, aWindow);
    result.flags |= _EU_Ci.nsITextInputProcessor.KEY_NON_PRINTABLE_KEY;
    if (code === undefined) {
      code = aTIP.computeCodeValueOfNonPrintableKey(
        keyName,
        aKeyEvent.location
      );
    }
  } else if (aKey != "") {
    keyName = aKey;
    if (!keyCodeIsDefined) {
      keyCode = aTIP.guessKeyCodeValueOfPrintableKeyInUSEnglishKeyboardLayout(
        aKey,
        aKeyEvent.location
      );
    }
    if (!keyCode) {
      result.flags |= _EU_Ci.nsITextInputProcessor.KEY_KEEP_KEYCODE_ZERO;
    }
    result.flags |= _EU_Ci.nsITextInputProcessor.KEY_FORCE_PRINTABLE_KEY;
    if (code === undefined) {
      code = aTIP.guessCodeValueOfPrintableKeyInUSEnglishKeyboardLayout(
        keyName,
        aKeyEvent.location
      );
    }
  }
  var locationIsDefined = "location" in aKeyEvent;
  if (locationIsDefined && aKeyEvent.location === 0) {
    result.flags |= _EU_Ci.nsITextInputProcessor.KEY_KEEP_KEY_LOCATION_STANDARD;
  }
  if (aKeyEvent.doNotMarkKeydownAsProcessed) {
    result.flags |=
      _EU_Ci.nsITextInputProcessor.KEY_DONT_MARK_KEYDOWN_AS_PROCESSED;
  }
  if (aKeyEvent.markKeyupAsProcessed) {
    result.flags |= _EU_Ci.nsITextInputProcessor.KEY_MARK_KEYUP_AS_PROCESSED;
  }
  result.dictionary = {
    key: keyName,
    code,
    location: locationIsDefined ? aKeyEvent.location : 0,
    repeat: "repeat" in aKeyEvent ? aKeyEvent.repeat === true : false,
    keyCode,
  };
  return result;
}

function _emulateToActivateModifiers(aTIP, aKeyEvent, aWindow = window) {
  if (!aKeyEvent) {
    return null;
  }
  var KeyboardEvent = _getKeyboardEvent(aWindow);

  var modifiers = {
    normal: [
      { key: "Alt", attr: "altKey" },
      { key: "AltGraph", attr: "altGraphKey" },
      { key: "Control", attr: "ctrlKey" },
      { key: "Fn", attr: "fnKey" },
      { key: "Meta", attr: "metaKey" },
      { key: "Shift", attr: "shiftKey" },
      { key: "Symbol", attr: "symbolKey" },
      { key: _EU_isMac(aWindow) ? "Meta" : "Control", attr: "accelKey" },
    ],
    lockable: [
      { key: "CapsLock", attr: "capsLockKey" },
      { key: "FnLock", attr: "fnLockKey" },
      { key: "NumLock", attr: "numLockKey" },
      { key: "ScrollLock", attr: "scrollLockKey" },
      { key: "SymbolLock", attr: "symbolLockKey" },
    ],
  };

  for (let i = 0; i < modifiers.normal.length; i++) {
    if (!aKeyEvent[modifiers.normal[i].attr]) {
      continue;
    }
    if (aTIP.getModifierState(modifiers.normal[i].key)) {
      continue; // already activated.
    }
    let event = new KeyboardEvent("", { key: modifiers.normal[i].key });
    aTIP.keydown(
      event,
      aTIP.KEY_NON_PRINTABLE_KEY | aTIP.KEY_DONT_DISPATCH_MODIFIER_KEY_EVENT
    );
    modifiers.normal[i].activated = true;
  }
  for (let i = 0; i < modifiers.lockable.length; i++) {
    if (!aKeyEvent[modifiers.lockable[i].attr]) {
      continue;
    }
    if (aTIP.getModifierState(modifiers.lockable[i].key)) {
      continue; // already activated.
    }
    let event = new KeyboardEvent("", { key: modifiers.lockable[i].key });
    aTIP.keydown(
      event,
      aTIP.KEY_NON_PRINTABLE_KEY | aTIP.KEY_DONT_DISPATCH_MODIFIER_KEY_EVENT
    );
    aTIP.keyup(
      event,
      aTIP.KEY_NON_PRINTABLE_KEY | aTIP.KEY_DONT_DISPATCH_MODIFIER_KEY_EVENT
    );
    modifiers.lockable[i].activated = true;
  }
  return modifiers;
}

function _emulateToInactivateModifiers(aTIP, aModifiers, aWindow = window) {
  if (!aModifiers) {
    return;
  }
  var KeyboardEvent = _getKeyboardEvent(aWindow);
  for (let i = 0; i < aModifiers.normal.length; i++) {
    if (!aModifiers.normal[i].activated) {
      continue;
    }
    let event = new KeyboardEvent("", { key: aModifiers.normal[i].key });
    aTIP.keyup(
      event,
      aTIP.KEY_NON_PRINTABLE_KEY | aTIP.KEY_DONT_DISPATCH_MODIFIER_KEY_EVENT
    );
  }
  for (let i = 0; i < aModifiers.lockable.length; i++) {
    if (!aModifiers.lockable[i].activated) {
      continue;
    }
    if (!aTIP.getModifierState(aModifiers.lockable[i].key)) {
      continue; // who already inactivated this?
    }
    let event = new KeyboardEvent("", { key: aModifiers.lockable[i].key });
    aTIP.keydown(
      event,
      aTIP.KEY_NON_PRINTABLE_KEY | aTIP.KEY_DONT_DISPATCH_MODIFIER_KEY_EVENT
    );
    aTIP.keyup(
      event,
      aTIP.KEY_NON_PRINTABLE_KEY | aTIP.KEY_DONT_DISPATCH_MODIFIER_KEY_EVENT
    );
  }
}

/**
 * Synthesize a composition event and keydown event and keyup events unless
 * you prevent to dispatch them explicitly (see aEvent.key's explanation).
 *
 * Note that you shouldn't call this with "compositionstart" unless you need to
 * test compositionstart event which is NOT followed by compositionupdate
 * event immediately.  Typically, native IME starts composition with
 * a pair of keydown and keyup event and dispatch compositionstart and
 * compositionupdate (and non-standard text event) between them.  So, in most
 * cases, you should call synthesizeCompositionChange() directly.
 * If you call this with compositionstart, keyup event will be fired
 * immediately after compositionstart.  In other words, you should use
 * "compositionstart" only when you need to emulate IME which just starts
 * composition with compositionstart event but does not send composing text to
 * us until committing the composition.  This is behavior of some Chinese IMEs.
 *
 * @param aEvent               The composition event information.  This must
 *                             have |type| member.  The value must be
 *                             "compositionstart", "compositionend",
 *                             "compositioncommitasis" or "compositioncommit".
 *
 *                             And also this may have |data| and |locale| which
 *                             would be used for the value of each property of
 *                             the composition event.  Note that the |data| is
 *                             ignored if the event type is "compositionstart"
 *                             or "compositioncommitasis".
 *
 *                             If |key| is undefined, "keydown" and "keyup"
 *                             events which are marked as "processed by IME"
 *                             are dispatched.  If |key| is not null, "keydown"
 *                             and/or "keyup" events are dispatched (if the
 *                             |key.type| is specified as "keydown", only
 *                             "keydown" event is dispatched).  Otherwise,
 *                             i.e., if |key| is null, neither "keydown" nor
 *                             "keyup" event is dispatched.
 *
 *                             If |key.doNotMarkKeydownAsProcessed| is not true,
 *                             key value and keyCode value of "keydown" event
 *                             will be set to "Process" and DOM_VK_PROCESSKEY.
 *                             If |key.markKeyupAsProcessed| is true,
 *                             key value and keyCode value of "keyup" event
 *                             will be set to "Process" and DOM_VK_PROCESSKEY.
 * @param aWindow              Optional (If null, current |window| will be used)
 * @param aCallback            Optional (If non-null, use the callback for
 *                             receiving notifications to IME)
 */
function synthesizeComposition(aEvent, aWindow = window, aCallback) {
  var TIP = _getTIP(aWindow, aCallback);
  if (!TIP) {
    return;
  }
  var KeyboardEvent = _getKeyboardEvent(aWindow);
  var modifiers = _emulateToActivateModifiers(TIP, aEvent.key, aWindow);
  var keyEventDict = { dictionary: null, flags: 0 };
  var keyEvent = null;
  if (aEvent.key && typeof aEvent.key.key === "string") {
    keyEventDict = _createKeyboardEventDictionary(
      aEvent.key.key,
      aEvent.key,
      TIP,
      aWindow
    );
    keyEvent = new KeyboardEvent(
      // eslint-disable-next-line no-nested-ternary
      aEvent.key.type === "keydown"
        ? "keydown"
        : aEvent.key.type === "keyup"
        ? "keyup"
        : "",
      keyEventDict.dictionary
    );
  } else if (aEvent.key === undefined) {
    keyEventDict = _createKeyboardEventDictionary(
      "KEY_Process",
      {},
      TIP,
      aWindow
    );
    keyEvent = new KeyboardEvent("", keyEventDict.dictionary);
  }
  try {
    switch (aEvent.type) {
      case "compositionstart":
        TIP.startComposition(keyEvent, keyEventDict.flags);
        break;
      case "compositioncommitasis":
        TIP.commitComposition(keyEvent, keyEventDict.flags);
        break;
      case "compositioncommit":
        TIP.commitCompositionWith(aEvent.data, keyEvent, keyEventDict.flags);
        break;
    }
  } finally {
    _emulateToInactivateModifiers(TIP, modifiers, aWindow);
  }
}
/**
 * Synthesize eCompositionChange event which causes a DOM text event, may
 * cause compositionupdate event, and causes keydown event and keyup event
 * unless you prevent to dispatch them explicitly (see aEvent.key's
 * explanation).
 *
 * Note that if you call this when there is no composition, compositionstart
 * event will be fired automatically.  This is better than you use
 * synthesizeComposition("compositionstart") in most cases.  See the
 * explanation of synthesizeComposition().
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
 *                 If |key| is undefined, "keydown" and "keyup" events which
 *                 are marked as "processed by IME" are dispatched.  If |key|
 *                 is not null, "keydown" and/or "keyup" events are dispatched
 *                 (if the |key.type| is specified as "keydown", only "keydown"
 *                 event is dispatched).  Otherwise, i.e., if |key| is null,
 *                 neither "keydown" nor "keyup" event is dispatched.
 *                 If |key.doNotMarkKeydownAsProcessed| is not true, key value
 *                 and keyCode value of "keydown" event will be set to
 *                 "Process" and DOM_VK_PROCESSKEY.
 *                 If |key.markKeyupAsProcessed| is true key value and keyCode
 *                 value of "keyup" event will be set to "Process" and
 *                 DOM_VK_PROCESSKEY.
 *
 * @param aWindow  Optional (If null, current |window| will be used)
 * @param aCallback     Optional (If non-null, use the callback for receiving
 *                      notifications to IME)
 */
function synthesizeCompositionChange(aEvent, aWindow = window, aCallback) {
  var TIP = _getTIP(aWindow, aCallback);
  if (!TIP) {
    return;
  }
  var KeyboardEvent = _getKeyboardEvent(aWindow);

  if (
    !aEvent.composition ||
    !aEvent.composition.clauses ||
    !aEvent.composition.clauses[0]
  ) {
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
            aEvent.composition.clauses[i].attr
          );
          break;
        case 0:
          // Ignore dummy clause for the argument.
          break;
        default:
          throw new Error("invalid clause attribute specified");
      }
    }
  }

  if (aEvent.caret) {
    TIP.setCaretInPendingComposition(aEvent.caret.start);
  }

  var modifiers = _emulateToActivateModifiers(TIP, aEvent.key, aWindow);
  try {
    var keyEventDict = { dictionary: null, flags: 0 };
    var keyEvent = null;
    if (aEvent.key && typeof aEvent.key.key === "string") {
      keyEventDict = _createKeyboardEventDictionary(
        aEvent.key.key,
        aEvent.key,
        TIP,
        aWindow
      );
      keyEvent = new KeyboardEvent(
        // eslint-disable-next-line no-nested-ternary
        aEvent.key.type === "keydown"
          ? "keydown"
          : aEvent.key.type === "keyup"
          ? "keyup"
          : "",
        keyEventDict.dictionary
      );
    } else if (aEvent.key === undefined) {
      keyEventDict = _createKeyboardEventDictionary(
        "KEY_Process",
        {},
        TIP,
        aWindow
      );
      keyEvent = new KeyboardEvent("", keyEventDict.dictionary);
    }
    TIP.flushPendingComposition(keyEvent, keyEventDict.flags);
  } finally {
    _emulateToInactivateModifiers(TIP, modifiers, aWindow);
  }
}

// Must be synchronized with nsIDOMWindowUtils.
const QUERY_CONTENT_FLAG_USE_NATIVE_LINE_BREAK = 0x0000;
const QUERY_CONTENT_FLAG_USE_XP_LINE_BREAK = 0x0001;

const QUERY_CONTENT_FLAG_SELECTION_NORMAL = 0x0000;
const QUERY_CONTENT_FLAG_SELECTION_SPELLCHECK = 0x0002;
const QUERY_CONTENT_FLAG_SELECTION_IME_RAWINPUT = 0x0004;
const QUERY_CONTENT_FLAG_SELECTION_IME_SELECTEDRAWTEXT = 0x0008;
const QUERY_CONTENT_FLAG_SELECTION_IME_CONVERTEDTEXT = 0x0010;
const QUERY_CONTENT_FLAG_SELECTION_IME_SELECTEDCONVERTEDTEXT = 0x0020;
const QUERY_CONTENT_FLAG_SELECTION_ACCESSIBILITY = 0x0040;
const QUERY_CONTENT_FLAG_SELECTION_FIND = 0x0080;
const QUERY_CONTENT_FLAG_SELECTION_URLSECONDARY = 0x0100;
const QUERY_CONTENT_FLAG_SELECTION_URLSTRIKEOUT = 0x0200;

const QUERY_CONTENT_FLAG_OFFSET_RELATIVE_TO_INSERTION_POINT = 0x0400;

const SELECTION_SET_FLAG_USE_NATIVE_LINE_BREAK = 0x0000;
const SELECTION_SET_FLAG_USE_XP_LINE_BREAK = 0x0001;
const SELECTION_SET_FLAG_REVERSE = 0x0002;

/**
 * Synthesize a query text content event.
 *
 * @param aOffset  The character offset.  0 means the first character in the
 *                 selection root.
 * @param aLength  The length of getting text.  If the length is too long,
 *                 the extra length is ignored.
 * @param aIsRelative   Optional (If true, aOffset is relative to start of
 *                      composition if there is, or start of selection.)
 * @param aWindow  Optional (If null, current |window| will be used)
 * @return         An nsIQueryContentEventResult object.  If this failed,
 *                 the result might be null.
 */
function synthesizeQueryTextContent(aOffset, aLength, aIsRelative, aWindow) {
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return null;
  }
  var flags = QUERY_CONTENT_FLAG_USE_NATIVE_LINE_BREAK;
  if (aIsRelative === true) {
    flags |= QUERY_CONTENT_FLAG_OFFSET_RELATIVE_TO_INSERTION_POINT;
  }
  return utils.sendQueryContentEvent(
    utils.QUERY_TEXT_CONTENT,
    aOffset,
    aLength,
    0,
    0,
    flags
  );
}

/**
 * Synthesize a query selected text event.
 *
 * @param aSelectionType    Optional, one of QUERY_CONTENT_FLAG_SELECTION_*.
 *                          If null, QUERY_CONTENT_FLAG_SELECTION_NORMAL will
 *                          be used.
 * @param aWindow  Optional (If null, current |window| will be used)
 * @return         An nsIQueryContentEventResult object.  If this failed,
 *                 the result might be null.
 */
function synthesizeQuerySelectedText(aSelectionType, aWindow) {
  var utils = _getDOMWindowUtils(aWindow);
  var flags = QUERY_CONTENT_FLAG_USE_NATIVE_LINE_BREAK;
  if (aSelectionType) {
    flags |= aSelectionType;
  }

  return utils.sendQueryContentEvent(
    utils.QUERY_SELECTED_TEXT,
    0,
    0,
    0,
    0,
    flags
  );
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
function synthesizeQueryCaretRect(aOffset, aWindow) {
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return null;
  }
  return utils.sendQueryContentEvent(
    utils.QUERY_CARET_RECT,
    aOffset,
    0,
    0,
    0,
    QUERY_CONTENT_FLAG_USE_NATIVE_LINE_BREAK
  );
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
async function synthesizeSelectionSet(
  aOffset,
  aLength,
  aReverse,
  aWindow = window
) {
  const utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return false;
  }
  // eSetSelection event will be compared with selection cache in
  // IMEContentObserver, but it may have not been updated yet.  Therefore, we
  // need to flush pending things of IMEContentObserver.
  await new Promise(resolve =>
    aWindow.requestAnimationFrame(() => aWindow.requestAnimationFrame(resolve))
  );
  const flags = aReverse ? SELECTION_SET_FLAG_REVERSE : 0;
  return utils.sendSelectionSetEvent(aOffset, aLength, flags);
}

/**
 * Synthesize a query text rect event.
 *
 * @param aOffset  The character offset.  0 means the first character in the
 *                 selection root.
 * @param aLength  The length of the text.  If the length is too long,
 *                 the extra length is ignored.
 * @param aIsRelative   Optional (If true, aOffset is relative to start of
 *                      composition if there is, or start of selection.)
 * @param aWindow  Optional (If null, current |window| will be used)
 * @return         An nsIQueryContentEventResult object.  If this failed,
 *                 the result might be null.
 */
function synthesizeQueryTextRect(aOffset, aLength, aIsRelative, aWindow) {
  if (aIsRelative !== undefined && typeof aIsRelative !== "boolean") {
    throw new Error(
      "Maybe, you set Window object to the 3rd argument, but it should be a boolean value"
    );
  }
  var utils = _getDOMWindowUtils(aWindow);
  let flags = QUERY_CONTENT_FLAG_USE_NATIVE_LINE_BREAK;
  if (aIsRelative === true) {
    flags |= QUERY_CONTENT_FLAG_OFFSET_RELATIVE_TO_INSERTION_POINT;
  }
  return utils.sendQueryContentEvent(
    utils.QUERY_TEXT_RECT,
    aOffset,
    aLength,
    0,
    0,
    flags
  );
}

/**
 * Synthesize a query text rect array event.
 *
 * @param aOffset  The character offset.  0 means the first character in the
 *                 selection root.
 * @param aLength  The length of the text.  If the length is too long,
 *                 the extra length is ignored.
 * @param aWindow  Optional (If null, current |window| will be used)
 * @return         An nsIQueryContentEventResult object.  If this failed,
 *                 the result might be null.
 */
function synthesizeQueryTextRectArray(aOffset, aLength, aWindow) {
  var utils = _getDOMWindowUtils(aWindow);
  return utils.sendQueryContentEvent(
    utils.QUERY_TEXT_RECT_ARRAY,
    aOffset,
    aLength,
    0,
    0,
    QUERY_CONTENT_FLAG_USE_NATIVE_LINE_BREAK
  );
}

/**
 * Synthesize a query editor rect event.
 *
 * @param aWindow  Optional (If null, current |window| will be used)
 * @return         An nsIQueryContentEventResult object.  If this failed,
 *                 the result might be null.
 */
function synthesizeQueryEditorRect(aWindow) {
  var utils = _getDOMWindowUtils(aWindow);
  return utils.sendQueryContentEvent(
    utils.QUERY_EDITOR_RECT,
    0,
    0,
    0,
    0,
    QUERY_CONTENT_FLAG_USE_NATIVE_LINE_BREAK
  );
}

/**
 * Synthesize a character at point event.
 *
 * @param aX, aY   The offset in the client area of the DOM window.
 * @param aWindow  Optional (If null, current |window| will be used)
 * @return         An nsIQueryContentEventResult object.  If this failed,
 *                 the result might be null.
 */
function synthesizeCharAtPoint(aX, aY, aWindow) {
  var utils = _getDOMWindowUtils(aWindow);
  return utils.sendQueryContentEvent(
    utils.QUERY_CHARACTER_AT_POINT,
    0,
    0,
    aX,
    aY,
    QUERY_CONTENT_FLAG_USE_NATIVE_LINE_BREAK
  );
}

/**
 * INTERNAL USE ONLY
 * Create an event object to pass to sendDragEvent.
 *
 * @param aType          The string represents drag event type.
 * @param aDestElement   The element to fire the drag event, used to calculate
 *                       screenX/Y and clientX/Y.
 * @param aDestWindow    Optional; Defaults to the current window object.
 * @param aDataTransfer  dataTransfer for current drag session.
 * @param aDragEvent     The object contains properties to override the event
 *                       object
 * @return               An object to pass to sendDragEvent.
 */
function createDragEventObject(
  aType,
  aDestElement,
  aDestWindow,
  aDataTransfer,
  aDragEvent
) {
  var destRect = aDestElement.getBoundingClientRect();
  var destClientX = destRect.left + destRect.width / 2;
  var destClientY = destRect.top + destRect.height / 2;
  var destScreenX = aDestWindow.mozInnerScreenX + destClientX;
  var destScreenY = aDestWindow.mozInnerScreenY + destClientY;
  if ("clientX" in aDragEvent && !("screenX" in aDragEvent)) {
    destScreenX = aDestWindow.mozInnerScreenX + aDragEvent.clientX;
  }
  if ("clientY" in aDragEvent && !("screenY" in aDragEvent)) {
    destScreenY = aDestWindow.mozInnerScreenY + aDragEvent.clientY;
  }

  // Wrap only in plain mochitests
  let dataTransfer;
  if (aDataTransfer) {
    dataTransfer = _EU_maybeUnwrap(
      _EU_maybeWrap(aDataTransfer).mozCloneForEvent(aType)
    );

    // Copy over the drop effect. This isn't copied over by Clone, as it uses
    // more complex logic in the actual implementation (see
    // nsContentUtils::SetDataTransferInEvent for actual impl).
    dataTransfer.dropEffect = aDataTransfer.dropEffect;
  }

  return Object.assign(
    {
      type: aType,
      screenX: destScreenX,
      screenY: destScreenY,
      clientX: destClientX,
      clientY: destClientY,
      dataTransfer,
      _domDispatchOnly: aDragEvent._domDispatchOnly,
    },
    aDragEvent
  );
}

/**
 * Emulate a event sequence of dragstart, dragenter, and dragover.
 *
 * @param {Element} aSrcElement
 *        The element to use to start the drag.
 * @param {Element} aDestElement
 *        The element to fire the dragover, dragenter events
 * @param {Array}   aDragData
 *        The data to supply for the data transfer.
 *        This data is in the format:
 *
 *        [
 *          [
 *            {"type": value, "data": value },
 *            ...,
 *          ],
 *          ...
 *        ]
 *
 *        Pass null to avoid modifying dataTransfer.
 * @param {String} [aDropEffect="move"]
 *        The drop effect to set during the dragstart event, or 'move' if omitted.
 * @param {Window} [aWindow=window]
 *        The window in which the drag happens. Defaults to the window in which
 *        EventUtils.js is loaded.
 * @param {Window} [aDestWindow=aWindow]
 *        Used when aDestElement is in a different window than aSrcElement.
 *        Default is to match ``aWindow``.
 * @param {Object} [aDragEvent={}]
 *        Defaults to empty object. Overwrites an object passed to sendDragEvent.
 * @return {Array}
 *        A two element array, where the first element is the value returned
 *        from sendDragEvent for dragover event, and the second element is the
 *        dataTransfer for the current drag session.
 */
function synthesizeDragOver(
  aSrcElement,
  aDestElement,
  aDragData,
  aDropEffect,
  aWindow,
  aDestWindow,
  aDragEvent = {}
) {
  if (!aWindow) {
    aWindow = window;
  }
  if (!aDestWindow) {
    aDestWindow = aWindow;
  }

  // eslint-disable-next-line mozilla/use-services
  const obs = _EU_Cc["@mozilla.org/observer-service;1"].getService(
    _EU_Ci.nsIObserverService
  );
  let utils = _getDOMWindowUtils(aWindow);
  var sess = utils.dragSession;

  // This method runs before other callbacks, and acts as a way to inject the
  // initial drag data into the DataTransfer.
  function fillDrag(event) {
    if (aDragData) {
      for (var i = 0; i < aDragData.length; i++) {
        var item = aDragData[i];
        for (var j = 0; j < item.length; j++) {
          _EU_maybeWrap(event.dataTransfer).mozSetDataAt(
            item[j].type,
            item[j].data,
            i
          );
        }
      }
    }
    event.dataTransfer.dropEffect = aDropEffect || "move";
    event.preventDefault();
  }

  function trapDrag(subject, topic) {
    if (topic == "on-datatransfer-available") {
      sess.dataTransfer = _EU_maybeUnwrap(
        _EU_maybeWrap(subject).mozCloneForEvent("drop")
      );
      sess.dataTransfer.dropEffect = subject.dropEffect;
    }
  }

  // need to use real mouse action
  aWindow.addEventListener("dragstart", fillDrag, true);
  obs.addObserver(trapDrag, "on-datatransfer-available");
  synthesizeMouseAtCenter(aSrcElement, { type: "mousedown" }, aWindow);

  var rect = aSrcElement.getBoundingClientRect();
  var x = rect.width / 2;
  var y = rect.height / 2;
  synthesizeMouse(aSrcElement, x, y, { type: "mousemove" }, aWindow);
  synthesizeMouse(aSrcElement, x + 10, y + 10, { type: "mousemove" }, aWindow);
  aWindow.removeEventListener("dragstart", fillDrag, true);
  obs.removeObserver(trapDrag, "on-datatransfer-available");

  var dataTransfer = sess.dataTransfer;
  if (!dataTransfer) {
    throw new Error("No data transfer object after synthesizing the mouse!");
  }

  // The EventStateManager will fire our dragenter event if it needs to.
  var event = createDragEventObject(
    "dragover",
    aDestElement,
    aDestWindow,
    dataTransfer,
    aDragEvent
  );
  var result = sendDragEvent(event, aDestElement, aDestWindow);

  return [result, dataTransfer];
}

/**
 * Emulate the drop event and mouseup event.
 * This should be called after synthesizeDragOver.
 *
 * @param {*} aResult
 *        The first element of the array returned from ``synthesizeDragOver``.
 * @param {DataTransfer} aDataTransfer
 *        The second element of the array returned from ``synthesizeDragOver``.
 * @param {Element} aDestElement
 *        The element on which to fire the drop event.
 * @param {Window} [aDestWindow=window]
 *        The window in which the drop happens. Defaults to the window in which
 *        EventUtils.js is loaded.
 * @param {Object} [aDragEvent={}]
 *        Defaults to empty object. Overwrites an object passed to sendDragEvent.
 * @return {String}
 *        "none" if aResult is true, ``aDataTransfer.dropEffect`` otherwise.
 */
function synthesizeDropAfterDragOver(
  aResult,
  aDataTransfer,
  aDestElement,
  aDestWindow,
  aDragEvent = {}
) {
  if (!aDestWindow) {
    aDestWindow = window;
  }

  var effect = aDataTransfer.dropEffect;
  var event;

  if (aResult) {
    effect = "none";
  } else if (effect != "none") {
    event = createDragEventObject(
      "drop",
      aDestElement,
      aDestWindow,
      aDataTransfer,
      aDragEvent
    );
    sendDragEvent(event, aDestElement, aDestWindow);
  }
  // Don't run accessibility checks for this click, since we're not actually
  // clicking. It's just generated as part of the drop.
  // this.AccessibilityUtils might not be set if this isn't a browser test or
  // if a browser test has loaded its own copy of EventUtils for some reason.
  // In the latter case, the test probably shouldn't do that.
  this.AccessibilityUtils?.suppressClickHandling(true);
  synthesizeMouse(aDestElement, 2, 2, { type: "mouseup" }, aDestWindow);
  this.AccessibilityUtils?.suppressClickHandling(false);

  return effect;
}

/**
 * Emulate a drag and drop by emulating a dragstart and firing events dragenter,
 * dragover, and drop.
 *
 * @param {Element} aSrcElement
 *        The element to use to start the drag.
 * @param {Element} aDestElement
 *        The element to fire the dragover, dragenter events
 * @param {Array}   aDragData
 *        The data to supply for the data transfer.
 *        This data is in the format:
 *
 *            [
 *              [
 *                {"type": value, "data": value },
 *                ...,
 *              ],
 *              ...
 *            ]
 *
 *        Pass null to avoid modifying dataTransfer.
 * @param {String} [aDropEffect="move"]
 *        The drop effect to set during the dragstart event, or 'move' if omitted..
 * @param {Window} [aWindow=window]
 *        The window in which the drag happens. Defaults to the window in which
 *        EventUtils.js is loaded.
 * @param {Window} [aDestWindow=aWindow]
 *        Used when aDestElement is in a different window than aSrcElement.
 *        Default is to match ``aWindow``.
 * @param {Object} [aDragEvent={}]
 *        Defaults to empty object. Overwrites an object passed to sendDragEvent.
 * @return {String}
 *        The drop effect that was desired.
 */
function synthesizeDrop(
  aSrcElement,
  aDestElement,
  aDragData,
  aDropEffect,
  aWindow,
  aDestWindow,
  aDragEvent = {}
) {
  if (!aWindow) {
    aWindow = window;
  }
  if (!aDestWindow) {
    aDestWindow = aWindow;
  }

  var ds = _EU_Cc["@mozilla.org/widget/dragservice;1"].getService(
    _EU_Ci.nsIDragService
  );

  let dropAction;
  switch (aDropEffect) {
    case null:
    case undefined:
    case "move":
      dropAction = _EU_Ci.nsIDragService.DRAGDROP_ACTION_MOVE;
      break;
    case "copy":
      dropAction = _EU_Ci.nsIDragService.DRAGDROP_ACTION_COPY;
      break;
    case "link":
      dropAction = _EU_Ci.nsIDragService.DRAGDROP_ACTION_LINK;
      break;
    default:
      throw new Error(`${aDropEffect} is an invalid drop effect value`);
  }

  ds.startDragSessionForTests(aWindow, dropAction);

  try {
    var [result, dataTransfer] = synthesizeDragOver(
      aSrcElement,
      aDestElement,
      aDragData,
      aDropEffect,
      aWindow,
      aDestWindow,
      aDragEvent
    );
    return synthesizeDropAfterDragOver(
      result,
      dataTransfer,
      aDestElement,
      aDestWindow,
      aDragEvent
    );
  } finally {
    let srcWindowUtils = _getDOMWindowUtils(aWindow);
    const srcDragSession = srcWindowUtils.dragSession;
    srcDragSession.endDragSession(true, _parseModifiers(aDragEvent));
  }
}

function _getFlattenedTreeParentNode(aNode) {
  return _EU_maybeUnwrap(_EU_maybeWrap(aNode).flattenedTreeParentNode);
}

function _getInclusiveFlattenedTreeParentElement(aNode) {
  for (
    let inclusiveAncestor = aNode;
    inclusiveAncestor;
    inclusiveAncestor = _getFlattenedTreeParentNode(inclusiveAncestor)
  ) {
    if (inclusiveAncestor.nodeType == Node.ELEMENT_NODE) {
      return inclusiveAncestor;
    }
  }
  return null;
}

function _nodeIsFlattenedTreeDescendantOf(
  aPossibleDescendant,
  aPossibleAncestor
) {
  do {
    if (aPossibleDescendant == aPossibleAncestor) {
      return true;
    }
    aPossibleDescendant = _getFlattenedTreeParentNode(aPossibleDescendant);
  } while (aPossibleDescendant);
  return false;
}

function _computeSrcElementFromSrcSelection(aSrcSelection) {
  let srcElement = aSrcSelection.focusNode;
  while (_EU_maybeWrap(srcElement).isNativeAnonymous) {
    srcElement = _getFlattenedTreeParentNode(srcElement);
  }
  if (srcElement.nodeType !== Node.ELEMENT_NODE) {
    srcElement = _getInclusiveFlattenedTreeParentElement(srcElement);
  }
  return srcElement;
}

/**
 * Emulate a drag and drop by emulating a dragstart by mousedown and mousemove,
 * and firing events dragenter, dragover, drop, and dragend.
 * This does not modify dataTransfer and tries to emulate the plain drag and
 * drop as much as possible, compared to synthesizeDrop.
 * Note that if synthesized dragstart is canceled, this throws an exception
 * because in such case, Gecko does not start drag session.
 *
 * @param {Object} aParams
 * @param {Event} aParams.dragEvent
 *                The DnD events will be generated with modifiers specified with this.
 * @param {Element} aParams.srcElement
 *                The element to start dragging.  If srcSelection is
 *                set, this is computed for element at focus node.
 * @param {Selection|nil} aParams.srcSelection
 *                The selection to start to drag, set null if srcElement is set.
 * @param {Element|nil} aParams.destElement
 *                The element to drop on. Pass null to emulate a drop on an invalid target.
 * @param {Number} aParams.srcX
 *                The initial x coordinate inside srcElement or ignored if srcSelection is set.
 * @param {Number} aParams.srcY
 *                The initial y coordinate inside srcElement or ignored if srcSelection is set.
 * @param {Number} aParams.stepX
 *                The x-axis step for mousemove inside srcElement
 * @param {Number} aParams.stepY
 *                The y-axis step for mousemove inside srcElement
 * @param {Number} aParams.finalX
 *                The final x coordinate inside srcElement
 * @param {Number} aParams.finalY
 *                The final x coordinate inside srcElement
 * @param {Any} aParams.id
 *                The pointer event id
 * @param {Window} aParams.srcWindow
 *                The window for dispatching event on srcElement, defaults to the current window object.
 * @param {Window} aParams.destWindow
 *                The window for dispatching event on destElement, defaults to the current window object.
 * @param {Boolean} aParams.expectCancelDragStart
 *                Set to true if the test cancels "dragstart"
 * @param {Boolean} aParams.expectSrcElementDisconnected
 *                Set to true if srcElement will be disconnected and
 *                "dragend" event won't be fired.
 * @param {Function} aParams.logFunc
 *                Set function which takes one argument if you need to log rect of target.  E.g., `console.log`.
 */
// eslint-disable-next-line complexity
async function synthesizePlainDragAndDrop(aParams) {
  let {
    dragEvent = {},
    srcElement,
    srcSelection,
    destElement,
    srcX = 2,
    srcY = 2,
    stepX = 9,
    stepY = 9,
    finalX = srcX + stepX * 2,
    finalY = srcY + stepY * 2,
    id = _getDOMWindowUtils(window).DEFAULT_MOUSE_POINTER_ID,
    srcWindow = window,
    destWindow = window,
    expectCancelDragStart = false,
    expectSrcElementDisconnected = false,
    logFunc,
  } = aParams;
  // Don't modify given dragEvent object because we modify dragEvent below and
  // callers may use the object multiple times so that callers must not assume
  // that it'll be modified.
  if (aParams.dragEvent !== undefined) {
    dragEvent = Object.assign({}, aParams.dragEvent);
  }

  function rectToString(aRect) {
    return `left: ${aRect.left}, top: ${aRect.top}, right: ${aRect.right}, bottom: ${aRect.bottom}`;
  }

  let srcWindowUtils = _getDOMWindowUtils(srcWindow);
  let destWindowUtils = _getDOMWindowUtils(destWindow);

  if (logFunc) {
    logFunc("synthesizePlainDragAndDrop() -- START");
  }

  if (srcSelection) {
    srcElement = _computeSrcElementFromSrcSelection(srcSelection);
    let srcElementRect = srcElement.getBoundingClientRect();
    if (logFunc) {
      logFunc(
        `srcElement.getBoundingClientRect(): ${rectToString(srcElementRect)}`
      );
    }
    // Use last selection client rect because nsIDragSession.sourceNode is
    // initialized from focus node which is usually in last rect.
    let selectionRectList = srcSelection.getRangeAt(0).getClientRects();
    let lastSelectionRect = selectionRectList[selectionRectList.length - 1];
    if (logFunc) {
      logFunc(
        `srcSelection.getRangeAt(0).getClientRects()[${
          selectionRectList.length - 1
        }]: ${rectToString(lastSelectionRect)}`
      );
    }
    // Click at center of last selection rect.
    srcX = Math.floor(lastSelectionRect.left + lastSelectionRect.width / 2);
    srcY = Math.floor(lastSelectionRect.top + lastSelectionRect.height / 2);
    // Then, adjust srcX and srcY for making them offset relative to
    // srcElementRect because they will be used when we call synthesizeMouse()
    // with srcElement.
    srcX = Math.floor(srcX - srcElementRect.left);
    srcY = Math.floor(srcY - srcElementRect.top);
    // Finally, recalculate finalX and finalY with new srcX and srcY if they
    // are not specified by the caller.
    if (aParams.finalX === undefined) {
      finalX = srcX + stepX * 2;
    }
    if (aParams.finalY === undefined) {
      finalY = srcY + stepY * 2;
    }
  } else if (logFunc) {
    logFunc(
      `srcElement.getBoundingClientRect(): ${rectToString(
        srcElement.getBoundingClientRect()
      )}`
    );
  }

  const editingHost = (() => {
    if (!srcElement.matches(":read-write")) {
      return null;
    }
    let lastEditableElement = srcElement;
    for (
      let inclusiveAncestor =
        _getInclusiveFlattenedTreeParentElement(srcElement);
      inclusiveAncestor;
      inclusiveAncestor = _getInclusiveFlattenedTreeParentElement(
        _getFlattenedTreeParentNode(inclusiveAncestor)
      )
    ) {
      if (inclusiveAncestor.matches(":read-write")) {
        lastEditableElement = inclusiveAncestor;
        if (lastEditableElement == srcElement.ownerDocument.body) {
          break;
        }
      }
    }
    return lastEditableElement;
  })();
  try {
    srcWindowUtils.disableNonTestMouseEvents(true);

    await new Promise(r => setTimeout(r, 0));

    let mouseDownEvent;
    function onMouseDown(aEvent) {
      mouseDownEvent = aEvent;
      if (logFunc) {
        logFunc(
          `"${aEvent.type}" event is fired on ${
            aEvent.target
          } (composedTarget: ${_EU_maybeUnwrap(
            _EU_maybeWrap(aEvent).composedTarget
          )}`
        );
      }
      if (
        !_nodeIsFlattenedTreeDescendantOf(
          _EU_maybeUnwrap(_EU_maybeWrap(aEvent).composedTarget),
          srcElement
        )
      ) {
        // If srcX and srcY does not point in one of rects in srcElement,
        // "mousedown" target is not in srcElement.  Such case must not
        // be expected by this API users so that we should throw an exception
        // for making debugging easier.
        throw new Error(
          'event target of "mousedown" is not srcElement nor its descendant'
        );
      }
    }
    try {
      srcWindow.addEventListener("mousedown", onMouseDown, { capture: true });
      synthesizeMouse(
        srcElement,
        srcX,
        srcY,
        { type: "mousedown", id },
        srcWindow
      );
      if (logFunc) {
        logFunc(`mousedown at ${srcX}, ${srcY}`);
      }
      if (!mouseDownEvent) {
        throw new Error('"mousedown" event is not fired');
      }
    } finally {
      srcWindow.removeEventListener("mousedown", onMouseDown, {
        capture: true,
      });
    }

    let dragStartEvent;
    function onDragStart(aEvent) {
      dragStartEvent = aEvent;
      if (logFunc) {
        logFunc(`"${aEvent.type}" event is fired`);
      }
      if (
        !_nodeIsFlattenedTreeDescendantOf(
          _EU_maybeUnwrap(_EU_maybeWrap(aEvent).composedTarget),
          srcElement
        )
      ) {
        // If srcX and srcY does not point in one of rects in srcElement,
        // "dragstart" target is not in srcElement.  Such case must not
        // be expected by this API users so that we should throw an exception
        // for making debugging easier.
        throw new Error(
          'event target of "dragstart" is not srcElement nor its descendant'
        );
      }
    }
    let dragEnterEvent;
    function onDragEnterGenerated(aEvent) {
      dragEnterEvent = aEvent;
    }
    srcWindow.addEventListener("dragstart", onDragStart, { capture: true });
    srcWindow.addEventListener("dragenter", onDragEnterGenerated, {
      capture: true,
    });
    try {
      // Wait for the next event tick after each event dispatch, so that UI
      // elements (e.g. menu) work like the real user input.
      await new Promise(r => setTimeout(r, 0));

      srcX += stepX;
      srcY += stepY;
      synthesizeMouse(
        srcElement,
        srcX,
        srcY,
        { type: "mousemove", id },
        srcWindow
      );
      if (logFunc) {
        logFunc(`first mousemove at ${srcX}, ${srcY}`);
      }

      await new Promise(r => setTimeout(r, 0));

      srcX += stepX;
      srcY += stepY;
      synthesizeMouse(
        srcElement,
        srcX,
        srcY,
        { type: "mousemove", id },
        srcWindow
      );
      if (logFunc) {
        logFunc(`second mousemove at ${srcX}, ${srcY}`);
      }

      await new Promise(r => setTimeout(r, 0));

      if (!dragStartEvent) {
        throw new Error('"dragstart" event is not fired');
      }
    } finally {
      srcWindow.removeEventListener("dragstart", onDragStart, {
        capture: true,
      });
      srcWindow.removeEventListener("dragenter", onDragEnterGenerated, {
        capture: true,
      });
    }

    let srcSession = srcWindowUtils.dragSession;
    if (!srcSession) {
      if (expectCancelDragStart) {
        synthesizeMouse(
          srcElement,
          finalX,
          finalY,
          { type: "mouseup", id },
          srcWindow
        );
        return;
      }
      throw new Error("drag hasn't been started by the operation");
    } else if (expectCancelDragStart) {
      throw new Error("drag has been started by the operation");
    }

    if (destElement) {
      if (
        (srcElement != destElement && !dragEnterEvent) ||
        destElement != dragEnterEvent.target
      ) {
        if (logFunc) {
          logFunc(
            `destElement.getBoundingClientRect(): ${rectToString(
              destElement.getBoundingClientRect()
            )}`
          );
        }

        function onDragEnter(aEvent) {
          dragEnterEvent = aEvent;
          if (logFunc) {
            logFunc(`"${aEvent.type}" event is fired`);
          }
          if (aEvent.target != destElement) {
            throw new Error('event target of "dragenter" is not destElement');
          }
        }
        destWindow.addEventListener("dragenter", onDragEnter, {
          capture: true,
        });
        try {
          let event = createDragEventObject(
            "dragenter",
            destElement,
            destWindow,
            null,
            dragEvent
          );
          sendDragEvent(event, destElement, destWindow);
          if (!dragEnterEvent && !destElement.disabled) {
            throw new Error('"dragenter" event is not fired');
          }
          if (dragEnterEvent && destElement.disabled) {
            throw new Error(
              '"dragenter" event should not be fired on disable element'
            );
          }
        } finally {
          destWindow.removeEventListener("dragenter", onDragEnter, {
            capture: true,
          });
        }
      }

      let dragOverEvent;
      function onDragOver(aEvent) {
        dragOverEvent = aEvent;
        if (logFunc) {
          logFunc(`"${aEvent.type}" event is fired`);
        }
        if (aEvent.target != destElement) {
          throw new Error('event target of "dragover" is not destElement');
        }
      }
      destWindow.addEventListener("dragover", onDragOver, { capture: true });
      try {
        // dragover and drop are only fired to a valid drop target. If the
        // destElement parameter is null, this function is being used to
        // simulate a drag'n'drop over an invalid drop target.
        let event = createDragEventObject(
          "dragover",
          destElement,
          destWindow,
          null,
          dragEvent
        );
        sendDragEvent(event, destElement, destWindow);
        if (!dragOverEvent && !destElement.disabled) {
          throw new Error('"dragover" event is not fired');
        }
        if (dragEnterEvent && destElement.disabled) {
          throw new Error(
            '"dragover" event should not be fired on disable element'
          );
        }
      } finally {
        destWindow.removeEventListener("dragover", onDragOver, {
          capture: true,
        });
      }

      await new Promise(r => setTimeout(r, 0));

      // If there is not accept to drop the data, "drop" event shouldn't be
      // fired.
      // XXX nsIDragSession.canDrop is different only on Linux.  It must be
      //     a bug of gtk/nsDragService since it manages `mCanDrop` by itself.
      //     Thus, we should use nsIDragSession.dragAction instead.
      let destSession = destWindowUtils.dragSession;
      if (
        destSession.dragAction != _EU_Ci.nsIDragService.DRAGDROP_ACTION_NONE
      ) {
        let dropEvent;
        function onDrop(aEvent) {
          dropEvent = aEvent;
          if (logFunc) {
            logFunc(`"${aEvent.type}" event is fired`);
          }
          if (
            !_nodeIsFlattenedTreeDescendantOf(
              _EU_maybeUnwrap(_EU_maybeWrap(aEvent).composedTarget),
              destElement
            )
          ) {
            throw new Error(
              'event target of "drop" is not destElement nor its descendant'
            );
          }
        }
        destWindow.addEventListener("drop", onDrop, { capture: true });
        try {
          let event = createDragEventObject(
            "drop",
            destElement,
            destWindow,
            null,
            dragEvent
          );
          sendDragEvent(event, destElement, destWindow);
          if (!dropEvent && destSession.canDrop) {
            throw new Error('"drop" event is not fired');
          }
        } finally {
          destWindow.removeEventListener("drop", onDrop, { capture: true });
        }
        return;
      }
    }

    // Since we don't synthesize drop event, we need to set drag end point
    // explicitly for "dragEnd" event which will be fired by
    // endDragSession().
    dragEvent.clientX = srcElement.getBoundingClientRect().x + finalX;
    dragEvent.clientY = srcElement.getBoundingClientRect().y + finalY;
    let event = createDragEventObject(
      "dragend",
      srcElement,
      srcWindow,
      null,
      dragEvent
    );
    srcSession.setDragEndPointForTests(event.screenX, event.screenY);
  } finally {
    await new Promise(r => setTimeout(r, 0));

    if (srcWindowUtils.dragSession) {
      const sourceNode = srcWindowUtils.dragSession.sourceNode;
      let dragEndEvent;
      function onDragEnd(aEvent) {
        dragEndEvent = aEvent;
        if (logFunc) {
          logFunc(`"${aEvent.type}" event is fired`);
        }
        if (
          !_nodeIsFlattenedTreeDescendantOf(
            _EU_maybeUnwrap(_EU_maybeWrap(aEvent).composedTarget),
            srcElement
          ) &&
          _EU_maybeUnwrap(_EU_maybeWrap(aEvent).composedTarget) != editingHost
        ) {
          throw new Error(
            'event target of "dragend" is not srcElement nor its descendant'
          );
        }
        if (expectSrcElementDisconnected) {
          throw new Error(
            `"dragend" event shouldn't be fired when the source node is disconnected (the source node is ${
              sourceNode?.isConnected ? "connected" : "null or disconnected"
            })`
          );
        }
      }
      srcWindow.addEventListener("dragend", onDragEnd, { capture: true });
      try {
        srcWindowUtils.dragSession.endDragSession(
          true,
          _parseModifiers(dragEvent)
        );
        if (!expectSrcElementDisconnected && !dragEndEvent) {
          // eslint-disable-next-line no-unsafe-finally
          throw new Error(
            `"dragend" event is not fired by nsIDragSession.endDragSession()${
              srcWindowUtils.dragSession.sourceNode &&
              !srcWindowUtils.dragSession.sourceNode.isConnected
                ? "(sourceNode was disconnected)"
                : ""
            }`
          );
        }
      } finally {
        srcWindow.removeEventListener("dragend", onDragEnd, { capture: true });
      }
    }
    srcWindowUtils.disableNonTestMouseEvents(false);
    if (logFunc) {
      logFunc("synthesizePlainDragAndDrop() -- END");
    }
  }
}

function _checkDataTransferItems(aDataTransfer, aExpectedDragData) {
  try {
    // We must wrap only in plain mochitests, not chrome
    let dataTransfer = _EU_maybeWrap(aDataTransfer);
    if (!dataTransfer) {
      return null;
    }
    if (
      aExpectedDragData == null ||
      dataTransfer.mozItemCount != aExpectedDragData.length
    ) {
      return dataTransfer;
    }
    for (let i = 0; i < dataTransfer.mozItemCount; i++) {
      let dtTypes = dataTransfer.mozTypesAt(i);
      if (dtTypes.length != aExpectedDragData[i].length) {
        return dataTransfer;
      }
      for (let j = 0; j < dtTypes.length; j++) {
        if (dtTypes[j] != aExpectedDragData[i][j].type) {
          return dataTransfer;
        }
        let dtData = dataTransfer.mozGetDataAt(dtTypes[j], i);
        if (aExpectedDragData[i][j].eqTest) {
          if (
            !aExpectedDragData[i][j].eqTest(
              dtData,
              aExpectedDragData[i][j].data
            )
          ) {
            return dataTransfer;
          }
        } else if (aExpectedDragData[i][j].data != dtData) {
          return dataTransfer;
        }
      }
    }
  } catch (ex) {
    return ex;
  }
  return true;
}

/**
 * This callback type is used with ``synthesizePlainDragAndCancel()``.
 * It should compare ``actualData`` and ``expectedData`` and return
 * true if the two should be considered equal, false otherwise.
 *
 * @callback eqTest
 * @param {*} actualData
 * @param {*} expectedData
 * @return {boolean}
 */

/**
 * synthesizePlainDragAndCancel() synthesizes drag start with
 * synthesizePlainDragAndDrop(), but always cancel it with preventing default
 * of "dragstart".  Additionally, this checks whether the dataTransfer of
 * "dragstart" event has only expected items.
 *
 * @param {Object} aParams
 *        The params which is set to the argument of ``synthesizePlainDragAndDrop()``.
 * @param {Array} aExpectedDataTransferItems
 *        All expected dataTransfer items.
 *        This data is in the format:
 *
 *        [
 *          [
 *            {"type": value, "data": value, eqTest: function}
 *            ...,
 *          ],
 *          ...
 *        ]
 *
 *        This can also be null.
 *        You can optionally provide ``eqTest`` {@type eqTest} if the
 *        comparison to the expected data transfer items can't be done
 *        with x == y;
 * @return {boolean}
 *        true if aExpectedDataTransferItems matches with
 *        DragEvent.dataTransfer of "dragstart" event.
 *        Otherwise, the dataTransfer object (may be null) or
 *        thrown exception, NOT false.  Therefore, you shouldn't
 *        use.
 */
async function synthesizePlainDragAndCancel(
  aParams,
  aExpectedDataTransferItems
) {
  let srcElement = aParams.srcSelection
    ? _computeSrcElementFromSrcSelection(aParams.srcSelection)
    : aParams.srcElement;
  let result;
  function onDragStart(aEvent) {
    aEvent.preventDefault();
    result = _checkDataTransferItems(
      aEvent.dataTransfer,
      aExpectedDataTransferItems
    );
  }
  SpecialPowers.wrap(srcElement.ownerDocument).addEventListener(
    "dragstart",
    onDragStart,
    { capture: true, mozSystemGroup: true }
  );
  try {
    aParams.expectCancelDragStart = true;
    await synthesizePlainDragAndDrop(aParams);
  } finally {
    SpecialPowers.wrap(srcElement.ownerDocument).removeEventListener(
      "dragstart",
      onDragStart,
      { capture: true, mozSystemGroup: true }
    );
  }
  return result;
}

/**
 * Emulate a drag and drop by generating a dragstart from mousedown and mousemove,
 * then firing events dragover and drop (or dragleave if expectDragLeave is set).
 * This does not modify dataTransfer and tries to emulate the plain drag and
 * drop as much as possible, compared to synthesizeDrop and
 * synthesizePlainDragAndDrop.  MockDragService is used in place of the native
 * nsIDragService implementation.  All coordinates are in client space.
 *
 * @param {Object} aParams
 * @param {Window} aParams.sourceBrowsingCxt
 *                The BrowsingContext (possibly remote) that contains
 *                srcElement.
 * @param {Window} aParams.targetBrowsingCxt
 *                The BrowsingContext (possibly remote) that contains
 *                targetElement.  Default is sourceBrowsingCxt.
 * @param {Element} aParams.srcElement
 *                The element to drag.
 * @param {Element|nil} aParams.targetElement
 *                The element to drop on.
 * @param {Number} aParams.step
 *                The 2D step for mousemoves
 * @param {Boolean} aParams.expectCancelDragStart
 *                Set to true if srcElement is set up to cancel "dragstart"
 * @param {Number} aParams.cancel
 *                The 2D coord the mouse is moved to as the last step if
 *                expectCancelDragStart is set
 * @param {Boolean} aParams.expectSrcElementDisconnected
 *                Set to true if srcElement will be disconnected and
 *                "dragend" event won't be fired.
 * @param {Boolean} aParams.expectDragLeave
 *                Set to true if the drop event will be converted to a
 *                dragleave before it is sent (e.g. it was rejected by a
 *                content analysis check).
 * @param {Boolean} aParams.expectNoDragEvents
 *                Set to true if no mouse or drag events should be received
 *                on the source or target.
 * @param {Boolean} aParams.expectNoDragTargetEvents
 *                Set to true if the drag should be blocked from sending
 *                events to the target.
 * @param {Boolean} aParams.dropPromise
 *                A promise that the caller will resolve before we check
 *                that the drop has happened.  Default is a pre-resolved
 *                promise.
 * @param {String} aParms.contextLabel
 *                Label that will appear in each output message.  Useful to
 *                distinguish between concurrent calls.  Default is none.
 * @param {Boolean} aParams.throwOnExtraMessage
 *                Throw an exception in child process when an unexpected
 *                event is received.  Used for debugging.  Default is false.
 * @param {Function} aParams.record
 *                Four-parameter function that logs the results of a remote
 *                assertion.  The parameters are (condition, message, ignored,
 *                stack).  This is the type of the mochitest report function.
 * @param {Function} aParams.info
 *                One-parameter info logging function.  Default is console.log.
 *                This is the type of the mochitest info function.
 * @param {Object} aParams.dragController
 *                MockDragController that the function should use.  This
 *                function will automatically generate one if none is given.
 */
async function synthesizeMockDragAndDrop(aParams) {
  const {
    srcElement,
    targetElement,
    step = [5, 5],
    cancel = [0, 0],
    sourceBrowsingCxt,
    targetBrowsingCxt = sourceBrowsingCxt,
    expectCancelDragStart = false,
    expectSrcElementDisconnected = false,
    expectDragLeave = false,
    expectNoDragEvents = false,
    dropPromise = Promise.resolve(undefined),
    contextLabel = "",
    throwOnExtraMessage = false,
  } = aParams;

  let { dragController = null, expectNoDragTargetEvents = false } = aParams;

  // Configure test reporting functions
  const prefix = contextLabel ? `[${contextLabel}]| ` : "";
  const info = msg => {
    aParams.info(`${prefix}${msg}`);
  };
  const record = (cond, msg, _, stack) => {
    aParams.record(cond, `${prefix}${msg}`, null, stack);
  };
  const ok = (cond, msg) => {
    record(cond, msg, null, Components.stack.caller);
  };

  info("synthesizeMockDragAndDrop() -- START");

  // Validate parameters
  ok(sourceBrowsingCxt, "sourceBrowsingCxt was given");
  ok(
    sourceBrowsingCxt != targetBrowsingCxt || srcElement != targetElement,
    "sourceBrowsingCxt+Element cannot be the same as targetBrowsingCxt+Element"
  );

  // no drag implies no drag target
  expectNoDragTargetEvents |= expectNoDragEvents;

  // Returns true if one browsing context is an ancestor of the other.
  let browsingContextsAreRelated = function (cxt1, cxt2) {
    let cxt = cxt1;
    while (cxt) {
      if (cxt2 == cxt) {
        return true;
      }
      cxt = cxt.parent;
    }
    cxt = cxt2.parent;
    while (cxt) {
      if (cxt1 == cxt) {
        return true;
      }
      cxt = cxt.parent;
    }
    return false;
  };

  // The rules for accessing the dataTransfer from internal drags in Gecko
  // during drag event handlers are as follows:
  //
  // dragstart:
  //   Always grants read-write access
  // dragenter/dragover/dragleave:
  //   If dom.events.dataTransfer.protected.enabled is set:
  //     Read-only permission is granted if any of these holds:
  //       * The drag target's browsing context is the same as the drag
  //         source's (e.g. dragging inside of one frame on a web page).
  //       * The drag source and target are the same domain/principal and
  //         one has a browsing context that is an ancestor of the other
  //         (e.g. one is an iframe nested inside of the other).
  //       * The principal of the drag target element is privileged (not
  //         a content principal).
  //   Otherwise:
  //     Permission is never granted
  // drop:
  //   Always grants read-only permission
  // dragend:
  //   Read-only permission is granted if
  //   dom.events.dataTransfer.protected.enabled is set.
  //
  // dragstart and dragend are special because they target the drag-source,
  // not the drag-target.
  let expectProtectedDataTransferAccessSource = !SpecialPowers.getBoolPref(
    "dom.events.dataTransfer.protected.enabled"
  );
  let expectProtectedDataTransferAccessTarget =
    expectProtectedDataTransferAccessSource &&
    browsingContextsAreRelated(targetBrowsingCxt, sourceBrowsingCxt);

  info(
    `expectProtectedDataTransferAccessSource: ${expectProtectedDataTransferAccessSource}`
  );
  info(
    `expectProtectedDataTransferAccessTarget: ${expectProtectedDataTransferAccessTarget}`
  );

  // Essentially the entire function is in a try block so that we can make sure
  // that the mock drag service is removed and non-test mouse events are
  // restored.
  const { MockRegistrar } = ChromeUtils.importESModule(
    "resource://testing-common/MockRegistrar.sys.mjs"
  );
  let dragServiceCid;
  let sourceCxt;
  let targetCxt;
  try {
    // Disable native mouse events to avoid external interference while the test
    // runs.  One call disables for all windows.
    _getDOMWindowUtils(sourceBrowsingCxt.ownerGlobal).disableNonTestMouseEvents(
      true
    );

    // Install mock drag service in main process.
    ok(
      Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_DEFAULT,
      "synthesizeMockDragAndDrop is only available in the main process"
    );

    if (!dragController) {
      info("No dragController was given so creating mock drag service");
      const oldDragService = SpecialPowers.Cc[
        "@mozilla.org/widget/dragservice;1"
      ].getService(SpecialPowers.Ci.nsIDragService);
      dragController = oldDragService.getMockDragController();
      dragServiceCid = MockRegistrar.register(
        "@mozilla.org/widget/dragservice;1",
        dragController.mockDragService
      );
      ok(dragServiceCid, "MockDragService was registered");
      // If the mock failed then don't continue or else we will trigger native
      // DND behavior.
      if (!dragServiceCid) {
        throw new Error("MockDragService failed to register");
      }
    }

    // Variables that are added to the child actor objects.
    const srcVars = {
      expectCancelDragStart,
      expectSrcElementDisconnected,
      expectNoDragEvents,
      expectProtectedDataTransferAccess:
        expectProtectedDataTransferAccessSource,
      dragElementId: srcElement,
    };
    const targetVars = {
      expectDragLeave,
      expectNoDragTargetEvents,
      expectProtectedDataTransferAccess:
        expectProtectedDataTransferAccessTarget,
      dragElementId: targetElement,
    };
    const bothVars = {
      contextLabel,
      throwOnExtraMessage,
      relevantEvents: [
        "mousedown",
        "mouseup",
        "dragstart",
        "dragenter",
        "dragover",
        "drop",
        "dragleave",
        "dragend",
      ],
    };

    const makeDragSourceContext = async (aBC, aRemoteVars) => {
      let { DragSourceParentContext } = _EU_ChromeUtils.importESModule(
        "chrome://mochikit/content/tests/SimpleTest/DragSourceParentContext.sys.mjs"
      );

      let ret = new DragSourceParentContext(aBC, aRemoteVars, SpecialPowers);
      await ret.initialize();
      return ret;
    };

    const makeDragTargetContext = async (aBC, aRemoteVars) => {
      let { DragTargetParentContext } = _EU_ChromeUtils.importESModule(
        "chrome://mochikit/content/tests/SimpleTest/DragTargetParentContext.sys.mjs"
      );

      let ret = new DragTargetParentContext(aBC, aRemoteVars, SpecialPowers);
      await ret.initialize();
      return ret;
    };

    [sourceCxt, targetCxt] = await Promise.all([
      makeDragSourceContext(sourceBrowsingCxt, { ...srcVars, ...bothVars }),
      makeDragTargetContext(targetBrowsingCxt, {
        ...targetVars,
        ...bothVars,
      }),
    ]);

    // Get element positions in screen and client coords
    let srcPos = await sourceCxt.getElementPositions();
    let targetPos = await targetCxt.getElementPositions();
    info(
      `screenSrcPos: ${srcPos.screenPos} | screenTargetPos: ${targetPos.screenPos}`
    );

    // Send and verify the mousedown on src.
    if (!expectNoDragEvents) {
      sourceCxt.expect("mousedown");
    }

    // Take ceiling of ccoordinates to make sure that the integer coordinates
    // are over the element.
    let currentSrcScreenPos = [
      Math.ceil(srcPos.screenPos[0]),
      Math.ceil(srcPos.screenPos[1]),
    ];
    info(
      `sending mousedown at ${currentSrcScreenPos[0]}, ${currentSrcScreenPos[1]}`
    );
    dragController.sendEvent(
      sourceBrowsingCxt,
      Ci.nsIMockDragServiceController.eMouseDown,
      currentSrcScreenPos[0],
      currentSrcScreenPos[1]
    );
    info(`mousedown sent`);

    await sourceCxt.synchronize();

    await sourceCxt.checkMouseDown();

    let contentInvokedDragPromise;

    info("setting up content-invoked-drag observer and expecting dragstart");
    if (!expectNoDragEvents) {
      sourceCxt.expect("dragstart");
      // Set up observable for content-invoked-drag, which is sent when the
      // parent learns that content has begun a drag session.
      contentInvokedDragPromise = new Promise(cb => {
        Services.obs.addObserver(function observe() {
          info("content-invoked-drag observer received message");
          Services.obs.removeObserver(observe, "content-invoked-drag");
          cb();
        }, "content-invoked-drag");
      });
    }

    // It takes two mouse-moves to initiate a drag session.
    currentSrcScreenPos = [
      currentSrcScreenPos[0] + step[0],
      currentSrcScreenPos[1] + step[1],
    ];
    info(
      `first mousemove at ${currentSrcScreenPos[0]}, ${currentSrcScreenPos[1]}`
    );
    dragController.sendEvent(
      sourceBrowsingCxt,
      Ci.nsIMockDragServiceController.eMouseMove,
      currentSrcScreenPos[0],
      currentSrcScreenPos[1]
    );
    info(`first mousemove sent`);

    currentSrcScreenPos = [
      currentSrcScreenPos[0] + step[0],
      currentSrcScreenPos[1] + step[1],
    ];
    info(
      `second mousemove at ${currentSrcScreenPos[0]}, ${currentSrcScreenPos[1]}`
    );
    dragController.sendEvent(
      sourceBrowsingCxt,
      Ci.nsIMockDragServiceController.eMouseMove,
      currentSrcScreenPos[0],
      currentSrcScreenPos[1]
    );
    info(`second mousemove sent`);

    if (!expectNoDragEvents) {
      info("waiting for content-invoked-drag observable");
      await contentInvokedDragPromise;
      ok(true, "content-invoked-drag was received");
    }

    info("checking dragstart");
    await sourceCxt.checkDragStart();

    if (expectNoDragEvents) {
      ok(
        !_getDOMWindowUtils(sourceBrowsingCxt.ownerGlobal).dragSession,
        "Drag was properly blocked from starting."
      );
      return;
    }

    // Another move creates the drag session in the parent process (but we need
    // to wait for the src process to get there).
    currentSrcScreenPos = [
      currentSrcScreenPos[0] + step[0],
      currentSrcScreenPos[1] + step[1],
    ];
    info(
      `third mousemove at ${currentSrcScreenPos[0]}, ${currentSrcScreenPos[1]}`
    );
    dragController.sendEvent(
      sourceBrowsingCxt,
      Ci.nsIMockDragServiceController.eMouseMove,
      currentSrcScreenPos[0],
      currentSrcScreenPos[1]
    );
    info(`third mousemove sent`);

    ok(
      _getDOMWindowUtils(sourceBrowsingCxt.ownerGlobal).dragSession,
      `Parent process source widget has drag session.`
    );

    if (expectCancelDragStart) {
      dragController.sendEvent(
        sourceBrowsingCxt,
        Ci.nsIMockDragServiceController.eMouseUp,
        cancel[0],
        cancel[1]
      );
      return;
    }

    await sourceCxt.checkExpected();

    // Implementation detail: EventStateManager::GenerateDragDropEnterExit
    // expects the source to get at least one dragover before leaving the
    // widget or else it fails to send dragenter/dragleave events to the
    // browsers.
    info("synthesizing dragover inside source");
    sourceCxt.expect("dragenter");
    sourceCxt.expect("dragover");
    currentSrcScreenPos = [
      currentSrcScreenPos[0] + step[0],
      currentSrcScreenPos[1] + step[1],
    ];
    info(`dragover at ${currentSrcScreenPos[0]}, ${currentSrcScreenPos[1]}`);
    dragController.sendEvent(
      sourceBrowsingCxt,
      Ci.nsIMockDragServiceController.eDragOver,
      currentSrcScreenPos[0],
      currentSrcScreenPos[1]
    );

    info(`dragover sent`);
    await sourceCxt.checkExpected();

    let currentTargetScreenPos = [
      Math.ceil(targetPos.screenPos[0]),
      Math.ceil(targetPos.screenPos[1]),
    ];

    // The next step is to drag to the target element.
    if (!expectNoDragTargetEvents) {
      sourceCxt.expect("dragleave");
    }

    if (
      sourceBrowsingCxt.top.embedderElement !==
      targetBrowsingCxt.top.embedderElement
    ) {
      // Send dragexit and dragenter only if we are dragging to another widget.
      // If we are dragging in the same widget then dragenter does not involve
      // the parent process.  This mirrors the native behavior. In the
      // widget-to-widget case, the source gets the dragexit immediately but
      // the target won't get a dragenter in content until we send a dragover --
      // this is because dragenters are generated by the EventStateManager and
      // are not forwarded remotely.
      // NB: dragleaves are synthesized by Gecko from dragexits.
      info("synthesizing dragexit and dragenter to enter new widget");
      if (!expectNoDragTargetEvents) {
        info("This will generate dragleave on the source");
      }

      dragController.sendEvent(
        sourceBrowsingCxt,
        Ci.nsIMockDragServiceController.eDragExit,
        currentTargetScreenPos[0],
        currentTargetScreenPos[1]
      );

      dragController.sendEvent(
        targetBrowsingCxt,
        Ci.nsIMockDragServiceController.eDragEnter,
        currentTargetScreenPos[0],
        currentTargetScreenPos[1]
      );

      await sourceCxt.synchronize();

      await sourceCxt.checkExpected();
      await targetCxt.checkExpected();
    }

    info(
      "Synthesizing dragover over target.  This will first generate a dragenter."
    );
    if (!expectNoDragTargetEvents) {
      targetCxt.expect("dragenter");
      targetCxt.expect("dragover");
    }

    dragController.sendEvent(
      targetBrowsingCxt,
      Ci.nsIMockDragServiceController.eDragOver,
      currentTargetScreenPos[0],
      currentTargetScreenPos[1]
    );

    await targetCxt.checkExpected();

    let expectedMessage = expectDragLeave ? "dragleave" : "drop";

    if (expectNoDragTargetEvents) {
      await targetCxt.checkHasDrag(false);
    } else {
      await targetCxt.checkSessionHasAction();
      targetCxt.expect(expectedMessage);
    }

    if (!expectSrcElementDisconnected) {
      await sourceCxt.checkHasDrag(true);
      sourceCxt.expect("dragend");
    }

    info(
      `issuing drop event that should be ` +
        `${
          !expectNoDragTargetEvents
            ? `received as a ${expectedMessage} event`
            : "ignored"
        }, followed by a dragend event`
    );

    currentTargetScreenPos = [
      currentTargetScreenPos[0] + step[0],
      currentTargetScreenPos[1] + step[1],
    ];
    dragController.sendEvent(
      targetBrowsingCxt,
      Ci.nsIMockDragServiceController.eDrop,
      currentTargetScreenPos[0],
      currentTargetScreenPos[1]
    );

    // Wait for any caller-supplied dropPromise before continuing.
    await dropPromise;

    if (!expectNoDragTargetEvents) {
      await targetCxt.checkDropOrDragLeave();
    } else {
      await targetCxt.checkExpected();
    }

    if (!expectSrcElementDisconnected) {
      await sourceCxt.checkDragEnd();
    } else {
      await sourceCxt.checkExpected();
    }

    ok(
      !_getDOMWindowUtils(sourceBrowsingCxt.ownerGlobal).dragSession,
      `Parent process source widget does not have a drag session.`
    );

    ok(
      !_getDOMWindowUtils(targetBrowsingCxt.ownerGlobal).dragSession,
      `Parent process target widget does not have a drag session.`
    );
  } catch (e) {
    // Any exception is a test failure.
    record(false, e.toString(), null, e.stack);
  } finally {
    if (sourceCxt) {
      await sourceCxt.cleanup();
    }
    if (targetCxt) {
      await targetCxt.cleanup();
    }

    if (dragServiceCid) {
      MockRegistrar.unregister(dragServiceCid);
    }

    _getDOMWindowUtils(sourceBrowsingCxt.ownerGlobal).disableNonTestMouseEvents(
      false
    );

    info("synthesizeMockDragAndDrop() -- END");
  }
}

class EventCounter {
  constructor(aTarget, aType, aOptions = {}) {
    this.target = aTarget;
    this.type = aType;
    this.options = aOptions;

    this.eventCount = 0;
    // Bug 1512817:
    // SpecialPowers is picky and needs to be passed an explicit reference to
    // the function to be called. To avoid having to bind "this", we therefore
    // define the method this way, via a property.
    this.handleEvent = () => {
      this.eventCount++;
    };

    SpecialPowers.wrap(aTarget).addEventListener(
      aType,
      this.handleEvent,
      aOptions
    );
  }

  unregister() {
    SpecialPowers.wrap(this.target).removeEventListener(
      this.type,
      this.handleEvent,
      this.options
    );
  }

  get count() {
    return this.eventCount;
  }
}
