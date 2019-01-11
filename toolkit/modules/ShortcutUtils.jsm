/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ShortcutUtils"];

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "PlatformKeys", function() {
  return Services.strings.createBundle(
    "chrome://global-platform/locale/platformKeys.properties");
});

XPCOMUtils.defineLazyGetter(this, "Keys", function() {
  return Services.strings.createBundle(
    "chrome://global/locale/keys.properties");
});

var ShortcutUtils = {
  IS_VALID: "valid",
  INVALID_KEY: "invalid_key",
  INVALID_MODIFIER: "invalid_modifier",
  INVALID_COMBINATION: "invalid_combination",
  DUPLICATE_MODIFIER: "duplicate_modifier",
  MODIFIER_REQUIRED: "modifier_required",

  /**
    * Prettifies the modifier keys for an element.
    *
    * @param Node aElemKey
    *        The key element to get the modifiers from.
    * @param boolean aNoCloverLeaf
    *        Pass true to use a descriptive string instead of the cloverleaf symbol. (OS X only)
    * @return string
    *         A prettified and properly separated modifier keys string.
    */
  prettifyShortcut(aElemKey, aNoCloverLeaf) {
    let elemString = this.getModifierString(
      aElemKey.getAttribute("modifiers"),
      aNoCloverLeaf);
    let key = this.getKeyString(
      aElemKey.getAttribute("keycode"),
      aElemKey.getAttribute("key"));
    return elemString + key;
  },

  getModifierString(elemMod, aNoCloverLeaf) {
    let elemString = "";
    let haveCloverLeaf = false;

    if (elemMod.match("accel")) {
      if (Services.appinfo.OS == "Darwin") {
        // XXX bug 779642 Use "Cmd-" literal vs. cloverleaf meta-key until
        // Orion adds variable height lines.
        if (aNoCloverLeaf) {
          elemString += "Cmd-";
        } else {
          haveCloverLeaf = true;
        }
      } else {
        elemString += PlatformKeys.GetStringFromName("VK_CONTROL") +
          PlatformKeys.GetStringFromName("MODIFIER_SEPARATOR");
      }
    }
    if (elemMod.match("access")) {
      if (Services.appinfo.OS == "Darwin") {
        elemString += PlatformKeys.GetStringFromName("VK_CONTROL") +
          PlatformKeys.GetStringFromName("MODIFIER_SEPARATOR");
      } else {
        elemString += PlatformKeys.GetStringFromName("VK_ALT") +
          PlatformKeys.GetStringFromName("MODIFIER_SEPARATOR");
      }
    }
    if (elemMod.match("os")) {
      elemString += PlatformKeys.GetStringFromName("VK_WIN") +
        PlatformKeys.GetStringFromName("MODIFIER_SEPARATOR");
    }
    if (elemMod.match("shift")) {
      elemString += PlatformKeys.GetStringFromName("VK_SHIFT") +
        PlatformKeys.GetStringFromName("MODIFIER_SEPARATOR");
    }
    if (elemMod.match("alt")) {
      elemString += PlatformKeys.GetStringFromName("VK_ALT") +
        PlatformKeys.GetStringFromName("MODIFIER_SEPARATOR");
    }
    if (elemMod.match("ctrl") || elemMod.match("control")) {
      elemString += PlatformKeys.GetStringFromName("VK_CONTROL") +
        PlatformKeys.GetStringFromName("MODIFIER_SEPARATOR");
    }
    if (elemMod.match("meta")) {
      elemString += PlatformKeys.GetStringFromName("VK_META") +
        PlatformKeys.GetStringFromName("MODIFIER_SEPARATOR");
    }

    if (haveCloverLeaf) {
      elemString += PlatformKeys.GetStringFromName("VK_META") +
        PlatformKeys.GetStringFromName("MODIFIER_SEPARATOR");
    }

    return elemString;
  },

  getKeyString(keyCode, keyAttribute) {
    let key;
    if (keyCode) {
      keyCode = keyCode.toUpperCase();
      try {
        let bundle = keyCode == "VK_RETURN" ? PlatformKeys : Keys;
        // Some keys might not exist in the locale file, which will throw.
        key = bundle.GetStringFromName(keyCode);
      } catch (ex) {
        Cu.reportError("Error finding " + keyCode + ": " + ex);
        key = keyCode.replace(/^VK_/, "");
      }
    } else {
      key = keyAttribute.toUpperCase();
    }

    return key;
  },

  getKeyAttribute(chromeKey) {
    if (/^[A-Z]$/.test(chromeKey)) {
      // We use the key attribute for single characters.
      return ["key", chromeKey];
    }
    return ["keycode", this.getKeycodeAttribute(chromeKey)];
  },

  /**
   * Determines the corresponding XUL keycode from the given chrome key.
   *
   * For example:
   *
   *    input     |  output
   *    ---------------------------------------
   *    "PageUp"  |  "VK_PAGE_UP"
   *    "Delete"  |  "VK_DELETE"
   *
   * @param {string} chromeKey The chrome key (e.g. "PageUp", "Space", ...)
   * @returns {string} The constructed value for the Key's 'keycode' attribute.
   */
  getKeycodeAttribute(chromeKey) {
    if (/^[0-9]/.test(chromeKey)) {
      return `VK_${chromeKey}`;
    }
    return `VK${chromeKey.replace(/([A-Z])/g, "_$&").toUpperCase()}`;
  },

  findShortcut(aElemCommand) {
    let document = aElemCommand.ownerDocument;
    return document.querySelector("key[command=\"" + aElemCommand.getAttribute("id") + "\"]");
  },

  chromeModifierKeyMap: {
    "Alt": "alt",
    "Command": "accel",
    "Ctrl": "accel",
    "MacCtrl": "control",
    "Shift": "shift",
  },

  /**
   * Determines the corresponding XUL modifiers from the chrome modifiers.
   *
   * For example:
   *
   *    input             |   output
   *    ---------------------------------------
   *    ["Ctrl", "Shift"] |   "accel,shift"
   *    ["MacCtrl"]       |   "control"
   *
   * @param {Array} chromeModifiers The array of chrome modifiers.
   * @returns {string} The constructed value for the Key's 'modifiers' attribute.
   */
  getModifiersAttribute(chromeModifiers) {
    return Array.from(chromeModifiers, modifier => {
      return ShortcutUtils.chromeModifierKeyMap[modifier];
    }).sort().join(",");
  },

  /**
   * Validate if a shortcut string is valid and return an error code if it
   * isn't valid.
   *
   * For example:
   *
   *    input            |   output
   *    ---------------------------------------
   *    "Ctrl+Shift+A"   |   IS_VALID
   *    "Shift+F"        |   MODIFIER_REQUIRED
   *    "Command+>"      |   INVALID_KEY
   *
   * @param {string} string The shortcut string.
   * @returns {string} The code for the validation result.
   */
  validate(string) {
    // A valid shortcut key for a webextension manifest
    const MEDIA_KEYS = /^(MediaNextTrack|MediaPlayPause|MediaPrevTrack|MediaStop)$/;
    const BASIC_KEYS = /^([A-Z0-9]|Comma|Period|Home|End|PageUp|PageDown|Space|Insert|Delete|Up|Down|Left|Right)$/;
    const FUNCTION_KEYS = /^(F[1-9]|F1[0-2])$/;

    if (MEDIA_KEYS.test(string.trim())) {
      return this.IS_VALID;
    }

    let modifiers = string.split("+").map(s => s.trim());
    let key = modifiers.pop();

    let chromeModifiers = modifiers.map(m => ShortcutUtils.chromeModifierKeyMap[m]);
    // If the modifier wasn't found it will be undefined.
    if (chromeModifiers.some(modifier => !modifier)) {
      return this.INVALID_MODIFIER;
    }

    switch (modifiers.length) {
      case 0:
        // A lack of modifiers is only allowed with function keys.
        if (!FUNCTION_KEYS.test(key)) {
          return this.MODIFIER_REQUIRED;
        }
        break;
      case 1:
        // Shift is only allowed on its own with function keys.
        if (chromeModifiers[0] == "shift" && !FUNCTION_KEYS.test(key)) {
          return this.MODIFIER_REQUIRED;
        }
        break;
      case 2:
        if (chromeModifiers[0] == chromeModifiers[1]) {
          return this.DUPLICATE_MODIFIER;
        }
        break;
      default:
        return this.INVALID_COMBINATION;
    }

    if (!BASIC_KEYS.test(key) && !FUNCTION_KEYS.test(key)) {
      return this.INVALID_KEY;
    }

    return this.IS_VALID;
  },

  /**
   * Attempt to find a key for a given shortcut string, such as
   * "Ctrl+Shift+A" and determine if it is a system shortcut.
   *
   * @param {Object} win The window to look for key elements in.
   * @param {string} value The shortcut string.
   * @returns {boolean} Whether a system shortcut was found or not.
   */
  isSystem(win, value) {
    let modifiers = value.split("+");
    let chromeKey = modifiers.pop();
    let modifiersString = this.getModifiersAttribute(modifiers);
    let keycode = this.getKeycodeAttribute(chromeKey);

    let baseSelector = "key";
    if (modifiers.length > 0) {
      baseSelector += `[modifiers="${modifiersString}"]`;
    }

    let keyEl = win.document.querySelector([
      `${baseSelector}[key="${chromeKey}"]`,
      `${baseSelector}[key="${chromeKey.toLowerCase()}"]`,
      `${baseSelector}[keycode="${keycode}"]`,
    ].join(","));
    return keyEl && !keyEl.closest("keyset").id.startsWith("ext-keyset-id");
  },
};

Object.freeze(ShortcutUtils);

