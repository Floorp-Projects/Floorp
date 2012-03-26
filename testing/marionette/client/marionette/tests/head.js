MARIONETTE_CONTEXT="chrome";
MARIONETTE_TIMEOUT=120000;

// Must be synchronized with nsIDOMWindowUtils.
const COMPOSITION_ATTR_RAWINPUT              = 0x02;
const COMPOSITION_ATTR_SELECTEDRAWTEXT       = 0x03;
const COMPOSITION_ATTR_CONVERTEDTEXT         = 0x04;
const COMPOSITION_ATTR_SELECTEDCONVERTEDTEXT = 0x05;

var EventUtils = {
  /**
   * see http://mxr.mozilla.org/mozilla-central/source/testing/mochitest/tests/SimpleTest/EventUtils.js
   */
  sendMouseEvent: function EventUtils__sendMouseEvent(aEvent, aTarget, aWindow) {
    if (['click', 'dblclick', 'mousedown', 'mouseup', 'mouseover', 'mouseout'].indexOf(aEvent.type) == -1) {
      throw new Error("sendMouseEvent doesn't know about event type '" + aEvent.type + "'");
    }

    if (!aWindow) {
      aWindow = window;
    }

    if (typeof(aTarget) == "string") {
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
    var buttonArg        = aEvent.button        || 0;
    var relatedTargetArg = aEvent.relatedTarget || null;

    event.initMouseEvent(typeArg, canBubbleArg, cancelableArg, viewArg, detailArg,
                         screenXArg, screenYArg, clientXArg, clientYArg,
                         ctrlKeyArg, altKeyArg, shiftKeyArg, metaKeyArg,
                         buttonArg, relatedTargetArg);

    aTarget.dispatchEvent(event);
  },

  sendChar: function EventUtils_sendChar(aChar, aWindow) {
    // DOM event charcodes match ASCII (JS charcodes) for a-zA-Z0-9.
    var hasShift = (aChar == aChar.toUpperCase());
    this.synthesizeKey(aChar, { shiftKey: hasShift }, aWindow);
  },

  sendString: function EventUtils_sendString(aStr, aWindow) {
    for (var i = 0; i < aStr.length; ++i) {
      this.sendChar(aStr.charAt(i), aWindow);
    }
  },

  sendKey: function EventUtils_sendKey(aKey, aWindow) {
    var keyName = "VK_" + aKey.toUpperCase();
    this.synthesizeKey(keyName, { shiftKey: false }, aWindow);
  },

  _getDOMWindowUtils: function EventUtils__getDOMWindowUtils(aWindow) {
    if (!aWindow) {
      aWindow = window;
    }

    // we need parent.SpecialPowers for:
    //  layout/base/tests/test_reftests_with_caret.html
    //  chrome: toolkit/content/tests/chrome/test_findbar.xul
    //  chrome: toolkit/content/tests/chrome/test_popup_anchor.xul
    /*if ("SpecialPowers" in window && window.SpecialPowers != undefined) {
      return SpecialPowers.getDOMWindowUtils(aWindow);
    }
    if ("SpecialPowers" in parent && parent.SpecialPowers != undefined) {
      return parent.SpecialPowers.getDOMWindowUtils(aWindow);
    }*/

    //TODO: this is assuming we are in chrome space
    return aWindow.QueryInterface(Components.interfaces.nsIInterfaceRequestor).
                                 getInterface(Components.interfaces.nsIDOMWindowUtils);
  },

  _computeKeyCodeFromChar: function EventUtils__computeKeyCodeFromChar(aChar) {
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
  },

  _parseModifiers: function EventUtils__parseModifiers(aEvent) {
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
  },

  isKeypressFiredKey: function EventUtils_isKeypressFiredKey(aDOMKeyCode) {
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
  },

  synthesizeKey: function EventUtils_synthesizeKey(aKey, aEvent, aWindow) {
    var utils = this._getDOMWindowUtils(aWindow);
    if (utils) {
      var keyCode = 0, charCode = 0;
      if (aKey.indexOf("VK_") == 0) {
        keyCode = KeyEvent["DOM_" + aKey];
        if (!keyCode) {
          throw "Unknown key: " + aKey;
        }
      } else {
        charCode = aKey.charCodeAt(0);
        keyCode = this._computeKeyCodeFromChar(aKey.charAt(0));
      }

      var modifiers = this._parseModifiers(aEvent);

      if (!("type" in aEvent) || !aEvent.type) {
        // Send keydown + (optional) keypress + keyup events.
        var keyDownDefaultHappened =
            utils.sendKeyEvent("keydown", keyCode, 0, modifiers);
        if (this.isKeypressFiredKey(keyCode)) {
          utils.sendKeyEvent("keypress", charCode ? 0 : keyCode, charCode,
                             modifiers, !keyDownDefaultHappened);
        }
        utils.sendKeyEvent("keyup", keyCode, 0, modifiers);
      } else if (aEvent.type == "keypress") {
        // Send standalone keypress event.
        utils.sendKeyEvent(aEvent.type, charCode ? 0 : keyCode,
                           charCode, modifiers);
      } else {
        // Send other standalone event than keypress.
        utils.sendKeyEvent(aEvent.type, keyCode, 0, modifiers);
      }
    }
  },
};

function waitForExplicitFinish() {}

var SpecialPowers = {
  _prefService: Components.classes["@mozilla.org/preferences-service;1"]
                .getService(Components.interfaces.nsIPrefBranch),

  setBoolPref: function SpecialPowers__setBoolPref(pref, value) {
    this._prefService.setBoolPref(pref, value);
  },
};

var readyAndUnlocked;

// see http://mxr.mozilla.org/mozilla-central/source/testing/mochitest/browser-test.js#478
function nextStep(arg) {
  try {
    __generator.send(arg);
  } catch(ex if ex instanceof StopIteration) {
    finish();
  } catch(ex) {
    ok(false, "Unhandled exception: " + ex);
    finish();
  }
}

// see http://mxr.mozilla.org/mozilla-central/source/testing/mochitest/browser-test.js#523
function requestLongerTimeout() {
  /* no-op! */
}

// The browser-chrome tests either start with test() or generatorTest().
var __generator = null;
if (typeof(test) != 'undefined')
  test();
else if (typeof(generatorTest) != 'undefined') {
  __generator = generatorTest();
  __generator.next();
}

