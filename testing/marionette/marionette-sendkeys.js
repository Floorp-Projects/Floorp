/*
 *  Copyright 2007-2009 WebDriver committers
 *  Copyright 2007-2009 Google Inc.
 *  Portions copyright 2012 Software Freedom Conservancy
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */


var type = function(doc, element, text, releaseModifiers,
    opt_keysState) {

  var currentTextLength = element.value ? element.value.length : 0;
  element.selectionStart = currentTextLength;
  element.selectionEnd = currentTextLength;

  // For consistency between native and synthesized events, convert common
  // escape sequences to their Key enum aliases.
  text = text.replace(new RegExp('\b', 'g'), '\uE003').   // DOM_VK_BACK_SPACE
      replace(/\t/g, '\uE004').                           // DOM_VK_TAB
      replace(/(\r\n|\n|\r)/g, '\uE006');                 // DOM_VK_RETURN

  var controlKey = false;
  var shiftKey = false;
  var altKey = false;
  var metaKey = false;
  if (opt_keysState) {
    controlKey = opt_keysState.control;
    shiftKey = opt_keysState.shiftKey;
    altKey = opt_keysState.alt;
    metaKey = opt_keysState.meta;
  }

  shiftCount = 0;

  var upper = text.toUpperCase();

  for (var i = 0; i < text.length; i++) {
    var c = text.charAt(i);

    // NULL key: reset modifier key states, and continue

    if (c == '\uE000') {
      if (controlKey) {
        var kCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_CONTROL;
        keyEvent(doc, element, "keyup", kCode, 0,
            controlKey = false, shiftKey, altKey, metaKey, false);
      }

      if (shiftKey) {
        var kCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_SHIFT;
        keyEvent(doc, element, "keyup", kCode, 0,
            controlKey, shiftKey = false, altKey, metaKey, false);
      }

      if (altKey) {
        var kCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_ALT;
        keyEvent(doc, element, "keyup", kCode, 0,
            controlKey, shiftKey, altKey = false, metaKey, false);
      }

      if (metaKey) {
        var kCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_META;
        keyEvent(doc, element, "keyup", kCode, 0,
            controlKey, shiftKey, altKey, metaKey = false, false);
      }

      continue;
    }

    // otherwise decode keyCode, charCode, modifiers ...

    var modifierEvent = "";
    var charCode = 0;
    var keyCode = 0;

    if (c == '\uE001') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_CANCEL;
    } else if (c == '\uE002') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_HELP;
    } else if (c == '\uE003') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_BACK_SPACE;
    } else if (c == '\uE004') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_TAB;
    } else if (c == '\uE005') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_CLEAR;
    } else if (c == '\uE006' || c == '\uE007') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_RETURN;
    } else if (c == '\uE008') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_SHIFT;
      shiftKey = !shiftKey;
      modifierEvent = shiftKey ? "keydown" : "keyup";
    } else if (c == '\uE009') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_CONTROL;
      controlKey = !controlKey;
      modifierEvent = controlKey ? "keydown" : "keyup";
    } else if (c == '\uE00A') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_ALT;
      altKey = !altKey;
      modifierEvent = altKey ? "keydown" : "keyup";
    } else if (c == '\uE03D') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_META;
      metaKey = !metaKey;
      modifierEvent = metaKey ? "keydown" : "keyup";
    } else if (c == '\uE00B') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_PAUSE;
    } else if (c == '\uE00C') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_ESCAPE;
    } else if (c == '\uE00D') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_SPACE;
      keyCode = charCode = ' '.charCodeAt(0);  // printable
    } else if (c == '\uE00E') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_PAGE_UP;
    } else if (c == '\uE00F') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_PAGE_DOWN;
    } else if (c == '\uE010') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_END;
    } else if (c == '\uE011') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_HOME;
    } else if (c == '\uE012') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_LEFT;
    } else if (c == '\uE013') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_UP;
    } else if (c == '\uE014') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_RIGHT;
    } else if (c == '\uE015') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_DOWN;
    } else if (c == '\uE016') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_INSERT;
    } else if (c == '\uE017') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_DELETE;
    } else if (c == '\uE018') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_SEMICOLON;
      charCode = ';'.charCodeAt(0);
    } else if (c == '\uE019') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_EQUALS;
      charCode = '='.charCodeAt(0);
    } else if (c == '\uE01A') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_NUMPAD0;
      charCode = '0'.charCodeAt(0);
    } else if (c == '\uE01B') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_NUMPAD1;
      charCode = '1'.charCodeAt(0);
    } else if (c == '\uE01C') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_NUMPAD2;
      charCode = '2'.charCodeAt(0);
    } else if (c == '\uE01D') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_NUMPAD3;
      charCode = '3'.charCodeAt(0);
    } else if (c == '\uE01E') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_NUMPAD4;
      charCode = '4'.charCodeAt(0);
    } else if (c == '\uE01F') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_NUMPAD5;
      charCode = '5'.charCodeAt(0);
    } else if (c == '\uE020') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_NUMPAD6;
      charCode = '6'.charCodeAt(0);
    } else if (c == '\uE021') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_NUMPAD7;
      charCode = '7'.charCodeAt(0);
    } else if (c == '\uE022') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_NUMPAD8;
      charCode = '8'.charCodeAt(0);
    } else if (c == '\uE023') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_NUMPAD9;
      charCode = '9'.charCodeAt(0);
    } else if (c == '\uE024') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_MULTIPLY;
      charCode = '*'.charCodeAt(0);
    } else if (c == '\uE025') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_ADD;
      charCode = '+'.charCodeAt(0);
    } else if (c == '\uE026') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_SEPARATOR;
      charCode = ','.charCodeAt(0);
    } else if (c == '\uE027') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_SUBTRACT;
      charCode = '-'.charCodeAt(0);
    } else if (c == '\uE028') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_DECIMAL;
      charCode = '.'.charCodeAt(0);
    } else if (c == '\uE029') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_DIVIDE;
      charCode = '/'.charCodeAt(0);
    } else if (c == '\uE031') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_F1;
    } else if (c == '\uE032') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_F2;
    } else if (c == '\uE033') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_F3;
    } else if (c == '\uE034') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_F4;
    } else if (c == '\uE035') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_F5;
    } else if (c == '\uE036') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_F6;
    } else if (c == '\uE037') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_F7;
    } else if (c == '\uE038') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_F8;
    } else if (c == '\uE039') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_F9;
    } else if (c == '\uE03A') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_F10;
    } else if (c == '\uE03B') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_F11;
    } else if (c == '\uE03C') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_F12;
    } else if (c == ',' || c == '<') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_COMMA;
      charCode = c.charCodeAt(0);
    } else if (c == '.' || c == '>') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_PERIOD;
      charCode = c.charCodeAt(0);
    } else if (c == '/' || c == '?') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_SLASH;
      charCode = text.charCodeAt(i);
    } else if (c == '`' || c == '~') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_BACK_QUOTE;
      charCode = c.charCodeAt(0);
    } else if (c == '{' || c == '[') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_OPEN_BRACKET;
      charCode = c.charCodeAt(0);
    } else if (c == '\\' || c == '|') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_BACK_SLASH;
      charCode = c.charCodeAt(0);
    } else if (c == '}' || c == ']') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_CLOSE_BRACKET;
      charCode = c.charCodeAt(0);
    } else if (c == '\'' || c == '"') {
      keyCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_QUOTE;
      charCode = c.charCodeAt(0);
    } else {
      keyCode = upper.charCodeAt(i);
      charCode = text.charCodeAt(i);
    }

    // generate modifier key event if needed, and continue

    if (modifierEvent) {
      keyEvent(doc, element, modifierEvent, keyCode, 0,
          controlKey, shiftKey, altKey, metaKey, false);
      continue;
    }

    // otherwise, shift down if needed

    var needsShift = false;
    if (charCode) {
      needsShift = /[A-Z\!\$\^\*\(\)\+\{\}\:\?\|~@#%&_"<>]/.test(c);
    }

    if (needsShift && !shiftKey) {
      var kCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_SHIFT;
      keyEvent(doc, element, "keydown", kCode, 0,
          controlKey, true, altKey, metaKey, false);
      shiftCount += 1;
    }

    // generate key[down/press/up] for key

    var pressCode = keyCode;
    if (charCode >= 32 && charCode < 127) {
      pressCode = 0;
      if (!needsShift && shiftKey && charCode > 32) {
        // If typing a lowercase character key and the shiftKey is down, the
        // charCode should be mapped to the shifted key value. This assumes
        // a default 104 international keyboard layout.
        if (charCode >= 97 && charCode <= 122) {
          charCode = charCode + 65 - 97;  // [a-z] -> [A-Z]
        } else {
          var mapFrom = '`1234567890-=[]\\;\',./';
          var mapTo = '~!@#$%^&*()_+{}|:"<>?';

          var value = String.fromCharCode(charCode).
              replace(/([\[\\\.])/g, '\\$1');
          var index = mapFrom.search(value);
          if (index >= 0) {
            charCode = mapTo.charCodeAt(index);
          }
        }
      }
    }

    var accepted =
        keyEvent(doc, element, "keydown", keyCode, 0,
            controlKey, needsShift || shiftKey, altKey, metaKey, false);

    keyEvent(doc, element, "keypress", pressCode, charCode,
        controlKey, needsShift || shiftKey, altKey, metaKey, !accepted);

    keyEvent(doc, element, "keyup", keyCode, 0,
        controlKey, needsShift || shiftKey, altKey, metaKey, false);

    // shift up if needed

    if (needsShift && !shiftKey) {
      var kCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_SHIFT;
      keyEvent(doc, element, "keyup", kCode, 0,
          controlKey, false, altKey, metaKey, false);
    }
  }

  // exit cleanup: keyup active modifier keys

  if (controlKey && releaseModifiers) {
    var kCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_CONTROL;
    keyEvent(doc, element, "keyup", kCode, 0,
        controlKey = false, shiftKey, altKey, metaKey, false);
  }

  if (shiftKey && releaseModifiers) {
    var kCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_SHIFT;
    keyEvent(doc, element, "keyup", kCode, 0,
        controlKey, shiftKey = false, altKey, metaKey, false);
  }

  if (altKey && releaseModifiers) {
    var kCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_ALT;
    keyEvent(doc, element, "keyup", kCode, 0,
        controlKey, shiftKey, altKey = false, metaKey, false);
  }

  if (metaKey && releaseModifiers) {
    var kCode = Components.interfaces.nsIDOMKeyEvent.DOM_VK_META;
    keyEvent(doc, element, "keyup", kCode, 0,
        controlKey, shiftKey, altKey, metaKey = false, false);
  }

  return {
    shiftKey: shiftKey,
    alt: altKey,
    meta: metaKey,
    control: controlKey
  };
};


var keyEvent = function(doc, element, type, keyCode, charCode,
                          controlState, shiftState, altState, metaState,
                          shouldPreventDefault) {
  var preventDefault = shouldPreventDefault == undefined ? false
      : shouldPreventDefault;

  var keyboardEvent = doc.createEvent("KeyEvents");
  var currentView = doc.defaultView;

  keyboardEvent.initKeyEvent(
      type, //  in DOMString typeArg,
      true, //  in boolean canBubbleArg
      true, //  in boolean cancelableArg
      currentView, //  in nsIDOMAbstractView viewArg
      controlState, //  in boolean ctrlKeyArg
      altState, //  in boolean altKeyArg
      shiftState, //  in boolean shiftKeyArg
      metaState, //  in boolean metaKeyArg
      keyCode, //  in unsigned long keyCodeArg
      charCode);    //  in unsigned long charCodeArg

  if (preventDefault) {
    keyboardEvent.preventDefault();
  }

  var win = doc.defaultView;
  var domUtil = win.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                .getInterface(Components.interfaces.nsIDOMWindowUtils);
  return domUtil.dispatchDOMEventViaPresShell(element, keyboardEvent, true);
};

