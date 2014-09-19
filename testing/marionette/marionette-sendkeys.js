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

function sendKeysToElement (document, element, keysToSend, successCallback, errorCallback, command_id) {
  if (checkVisible(element)) {
    element.focus();
    var value = keysToSend.join("");
    let hasShift = null;
    let hasCtrl = null;
    let hasAlt = null;
    let hasMeta = null;
    for (var i = 0; i < value.length; i++) {
      let upper = value.charAt(i).toUpperCase();
      var keyCode = null;
      var c = value.charAt(i);
      switch (c) {
        case '\uE001':
          keyCode = "VK_CANCEL";
          break;
        case '\uE002':
          keyCode = "VK_HELP";
          break;
        case '\uE003':
          keyCode = "VK_BACK_SPACE";
          break;
        case '\uE004':
          keyCode = "VK_TAB";
          break;
        case '\uE005':
          keyCode = "VK_CLEAR";
          break;
        case '\uE006':
        case '\uE007':
          keyCode = "VK_RETURN";
          break;
        case '\uE008':
          keyCode = "VK_SHIFT";
          hasShift = !hasShift;
          break;
        case '\uE009':
          keyCode = "VK_CONTROL";
          controlKey = !controlKey;
          break;
        case '\uE00A':
          keyCode = "VK_ALT";
          altKey = !altKey;
          break;
        case '\uE03D':
          keyCode = "VK_META";
          metaKey = !metaKey;
          break;
        case '\uE00B':
          keyCode = "VK_PAUSE";
          break;
        case '\uE00C':
          keyCode = "VK_ESCAPE";
          break;
        case '\uE00D':
          keyCode = "VK_SPACE";  // printable
          break;
        case '\uE00E':
          keyCode = "VK_PAGE_UP";
          break;
        case '\uE00F':
          keyCode = "VK_PAGE_DOWN";
          break;
        case '\uE010':
          keyCode = "VK_END";
          break;
        case '\uE011':
          keyCode = "VK_HOME";
          break;
        case '\uE012':
          keyCode = "VK_LEFT";
          break;
        case '\uE013':
          keyCode = "VK_UP";
          break;
        case '\uE014':
          keyCode = "VK_RIGHT";
          break;
        case '\uE015':
          keyCode = "VK_DOWN";
          break;
        case '\uE016':
          keyCode = "VK_INSERT";
          break;
        case '\uE017':
          keyCode = "VK_DELETE";
          break;
        case '\uE018':
          keyCode = "VK_SEMICOLON";
          break;
        case '\uE019':
          keyCode = "VK_EQUALS";
          break;
        case '\uE01A':
          keyCode = "VK_NUMPAD0";
          break;
        case '\uE01B':
          keyCode = "VK_NUMPAD1";
          break;
        case '\uE01C':
          keyCode = "VK_NUMPAD2";
          break;
        case '\uE01D':
          keyCode = "VK_NUMPAD3";
          break;
        case '\uE01E':
          keyCode = "VK_NUMPAD4";
          break;
        case '\uE01F':
          keyCode = "VK_NUMPAD5";
          break;
        case '\uE020':
          keyCode = "VK_NUMPAD6";
          break;
        case '\uE021':
          keyCode = "VK_NUMPAD7";
          break;
        case '\uE022':
          keyCode = "VK_NUMPAD8";
          break;
        case '\uE023':
          keyCode = "VK_NUMPAD9";
          break;
        case '\uE024':
          keyCode = "VK_MULTIPLY";
          break;
        case '\uE025':
          keyCode = "VK_ADD";
          break;
        case '\uE026':
          keyCode = "VK_SEPARATOR";
          break;
        case '\uE027':
          keyCode = "VK_SUBTRACT";
          break;
        case '\uE028':
          keyCode = "VK_DECIMAL";
          break;
        case '\uE029':
          keyCode = "VK_DIVIDE";
          break;
        case '\uE031':
          keyCode = "VK_F1";
          break;
        case '\uE032':
          keyCode = "VK_F2";
          break;
        case '\uE033':
          keyCode = "VK_F3";
          break;
        case '\uE034':
          keyCode = "VK_F4";
          break;
        case '\uE035':
          keyCode = "VK_F5";
          break;
        case '\uE036':
          keyCode = "VK_F6";
          break;
        case '\uE037':
          keyCode = "VK_F7";
          break;
        case '\uE038':
          keyCode = "VK_F8";
          break;
        case '\uE039':
          keyCode = "VK_F9";
          break;
        case '\uE03A':
          keyCode = "VK_F10";
          break;
        case '\uE03B':
          keyCode = "VK_F11";
          break;
        case '\uE03C':
          keyCode = "VK_F12";
          break;
      }
      hasShift = value.charAt(i) == upper;
      utils.synthesizeKey(keyCode || value[i],
                          { shiftKey: hasShift, ctrlKey: hasCtrl, altKey: hasAlt, metaKey: hasMeta },
                          document);
    }
    successCallback(command_id);
  }
  else {
    errorCallback("Element is not visible", 11, null, command_id);
  }
};