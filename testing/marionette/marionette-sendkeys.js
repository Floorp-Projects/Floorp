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

let {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
               .getService(Ci.mozIJSSubScriptLoader);

let utils = {};
loader.loadSubScript("chrome://marionette/content/EventUtils.js", utils);
loader.loadSubScript("chrome://marionette/content/ChromeUtils.js", utils);

let keyModifierNames = {
    "VK_SHIFT": 'shiftKey',
    "VK_CONTROL": 'ctrlKey',
    "VK_ALT": 'altKey',
    "VK_META": 'metaKey'
};

let keyCodes = {
  '\uE001': "VK_CANCEL",
  '\uE002': "VK_HELP",
  '\uE003': "VK_BACK_SPACE",
  '\uE004': "VK_TAB",
  '\uE005': "VK_CLEAR",
  '\uE006': "VK_RETURN",
  '\uE007': "VK_RETURN",
  '\uE008': "VK_SHIFT",
  '\uE009': "VK_CONTROL",
  '\uE00A': "VK_ALT",
  '\uE03D': "VK_META",
  '\uE00B': "VK_PAUSE",
  '\uE00C': "VK_ESCAPE",
  '\uE00D': "VK_SPACE",  // printable
  '\uE00E': "VK_PAGE_UP",
  '\uE00F': "VK_PAGE_DOWN",
  '\uE010': "VK_END",
  '\uE011': "VK_HOME",
  '\uE012': "VK_LEFT",
  '\uE013': "VK_UP",
  '\uE014': "VK_RIGHT",
  '\uE015': "VK_DOWN",
  '\uE016': "VK_INSERT",
  '\uE017': "VK_DELETE",
  '\uE018': "VK_SEMICOLON",
  '\uE019': "VK_EQUALS",
  '\uE01A': "VK_NUMPAD0",
  '\uE01B': "VK_NUMPAD1",
  '\uE01C': "VK_NUMPAD2",
  '\uE01D': "VK_NUMPAD3",
  '\uE01E': "VK_NUMPAD4",
  '\uE01F': "VK_NUMPAD5",
  '\uE020': "VK_NUMPAD6",
  '\uE021': "VK_NUMPAD7",
  '\uE022': "VK_NUMPAD8",
  '\uE023': "VK_NUMPAD9",
  '\uE024': "VK_MULTIPLY",
  '\uE025': "VK_ADD",
  '\uE026': "VK_SEPARATOR",
  '\uE027': "VK_SUBTRACT",
  '\uE028': "VK_DECIMAL",
  '\uE029': "VK_DIVIDE",
  '\uE031': "VK_F1",
  '\uE032': "VK_F2",
  '\uE033': "VK_F3",
  '\uE034': "VK_F4",
  '\uE035': "VK_F5",
  '\uE036': "VK_F6",
  '\uE037': "VK_F7",
  '\uE038': "VK_F8",
  '\uE039': "VK_F9",
  '\uE03A': "VK_F10",
  '\uE03B': "VK_F11",
  '\uE03C': "VK_F12"
};

function getKeyCode (c) {
  if (c in keyCodes) {
    return keyCodes[c];
  }
  return c;
};

function sendKeyDown (keyToSend, modifiers, document) {
  modifiers.type = "keydown";
  sendSingleKey(keyToSend, modifiers, document);
  if (["VK_SHIFT", "VK_CONTROL",
       "VK_ALT", "VK_META"].indexOf(getKeyCode(keyToSend)) == -1) {
    modifiers.type = "keypress";
    sendSingleKey(keyToSend, modifiers, document);
  }
  delete modifiers.type;
}

function sendKeyUp (keyToSend, modifiers, document) {
  modifiers.type = "keyup";
  sendSingleKey(keyToSend, modifiers, document);
  delete modifiers.type;
}

function sendSingleKey (keyToSend, modifiers, document) {
  let keyCode = getKeyCode(keyToSend);
  if (keyCode in keyModifierNames) {
    let modName = keyModifierNames[keyCode];
    modifiers[modName] = !modifiers[modName];
  } else if (modifiers.shiftKey) {
    keyCode = keyCode.toUpperCase();
  }
  utils.synthesizeKey(keyCode, modifiers, document);
}

function sendKeysToElement (document, element, keysToSend, successCallback, errorCallback, command_id, context) {
  if (context == "chrome" || checkVisible(element)) {
    element.focus();
    let modifiers = {
      shiftKey: false,
      ctrlKey: false,
      altKey: false,
      metaKey: false
    };
    let value = keysToSend.join("");
    for (var i = 0; i < value.length; i++) {
      var c = value.charAt(i);
      sendSingleKey(c, modifiers, document);
    }
    successCallback(command_id);
  }
  else {
    errorCallback("Element is not visible", 11, null, command_id);
  }
};
