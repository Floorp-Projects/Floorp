/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* exported render */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  ShortcutUtils: "resource://gre/modules/ShortcutUtils.jsm",
});

let templatesLoaded = false;
const templates = {};

function loadTemplates() {
  if (templatesLoaded) return;
  templatesLoaded = true;

  templates.card = document.getElementById("card-template");
  templates.row = document.getElementById("shortcut-row-template");
  templates.empty = document.getElementById("shortcuts-empty-template");
  templates.noAddons = document.getElementById("shortcuts-no-addons");
}

function extensionForAddonId(id) {
  let policy = WebExtensionPolicy.getByID(id);
  return policy && policy.extension;
}

let builtInNames = new Map([
  ["_execute_browser_action", "shortcuts-browserAction"],
  ["_execute_page_action", "shortcuts-pageAction"],
  ["_execute_sidebar_action", "shortcuts-sidebarAction"],
]);
let getCommandDescriptionId = (command) => {
  if (!command.description && builtInNames.has(command.name)) {
    return builtInNames.get(command.name);
  }
  return null;
};

const _functionKeys = [
  "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
];
const functionKeys = new Set(_functionKeys);
const validKeys = new Set([
  "Home", "End", "PageUp", "PageDown", "Insert", "Delete",
  "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
  ..._functionKeys,
  "MediaNextTrack", "MediaPlayPause", "MediaPrevTrack", "MediaStop",
  "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
  "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
  "Up", "Down", "Left", "Right",
  "Comma", "Period", "Space",
]);

/**
 * Trim a valid prefix from an event string.
 *
 *     "Digit3" ~> "3"
 *     "ArrowUp" ~> "Up"
 *     "W" ~> "W"
 *
 * @param {string} string The input string.
 * @returns {string} The trimmed string, or unchanged.
 */
function trimPrefix(string) {
  return string.replace(/^(?:Digit|Numpad|Arrow)/, "");
}

const remapKeys = {
  ",": "Comma",
  ".": "Period",
  " ": "Space",
};
/**
 * Map special keys to their shortcut name.
 *
 *     "," ~> "Comma"
 *     " " ~> "Space"
 *
 * @param {string} string The input string.
 * @returns {string} The remapped string, or unchanged.
 */
function remapKey(string) {
  if (remapKeys.hasOwnProperty(string)) {
    return remapKeys[string];
  }
  return string;
}

const keyOptions = [
  e => String.fromCharCode(e.which), // A letter?
  e => e.code.toUpperCase(), // A letter.
  e => trimPrefix(e.code), // Digit3, ArrowUp, Numpad9.
  e => trimPrefix(e.key), // Digit3, ArrowUp, Numpad9.
  e => remapKey(e.key), // Comma, Period, Space.
];
/**
 * Map a DOM event to a shortcut string character.
 *
 * For example:
 *
 *    "a" ~> "A"
 *    "Digit3" ~> "3"
 *    "," ~> "Comma"
 *
 * @param {object} event A KeyboardEvent.
 * @returns {string} A string corresponding to the pressed key.
 */
function getStringForEvent(event) {
  for (let option of keyOptions) {
    let value = option(event);
    if (validKeys.has(value)) {
      return value;
    }
  }

  return "";
}

function getShortcutValue(shortcut) {
  if (!shortcut) {
    // Ensure the shortcut is a string, even if it is unset.
    return null;
  }

  let modifiers = shortcut.split("+");
  let key = modifiers.pop();

  if (modifiers.length > 0) {
    let modifiersAttribute = ShortcutUtils.getModifiersAttribute(modifiers);
    let displayString =
      ShortcutUtils.getModifierString(modifiersAttribute) + key;
    return displayString;
  }

  if (functionKeys.has(key)) {
    return key;
  }

  return null;
}

let error;

function setError(input, messageId) {
  if (!error) error = document.querySelector(".error-message");

  let {x, y, height} = input.getBoundingClientRect();
  error.style.top = `${y + window.scrollY + height - 5}px`;
  error.style.left = `${x}px`;
  document.l10n.setAttributes(
    error.querySelector(".error-message-label"), messageId);
  error.style.visibility = "visible";
}

function inputBlurred(e) {
  if (!error) error = document.querySelector(".error-message");

  error.style.visibility = "hidden";
  e.target.value = getShortcutValue(e.target.getAttribute("shortcut"));
}

function clearValue(e) {
  e.target.value = "";
}

function getShortcutForEvent(e) {
  let modifierMap;

  if (AppConstants.platform == "macosx") {
    modifierMap = {
      MacCtrl: e.ctrlKey,
      Alt: e.altKey,
      Command: e.metaKey,
      Shift: e.shiftKey,
    };
  } else {
    modifierMap = {
      Ctrl: e.ctrlKey,
      Alt: e.altKey,
      Shift: e.shiftKey,
    };
  }

  return Object.entries(modifierMap)
    .filter(([key, isDown]) => isDown)
    .map(([key]) => key)
    .concat(getStringForEvent(e))
    .join("+");
}

function onShortcutChange(e) {
  let input = e.target;

  if (e.key == "Escape") {
    input.blur();
    return;
  }

  if (e.key == "Tab") {
    return;
  }

  e.preventDefault();
  e.stopPropagation();

  let shortcutString = getShortcutForEvent(e);
  input.value = getShortcutValue(shortcutString);

  if (e.type == "keyup" || shortcutString.length == 0) {
    return;
  }

  let validation = ShortcutUtils.validate(shortcutString);
  switch (validation) {
    case ShortcutUtils.IS_VALID:
      // Show an error if this is already a system shortcut.
      let chromeWindow = window.windowRoot.ownerGlobal;
      if (ShortcutUtils.isSystem(chromeWindow, shortcutString)) {
        setError(input, "shortcuts-system");
        break;
      }

      // Update the shortcut if it isn't reserved.
      let addonId = input.closest(".card").getAttribute("addon-id");
      let extension = extensionForAddonId(addonId);

      // This is async, but we're not awaiting it to keep the handler sync.
      extension.shortcuts.updateCommand({
        name: input.getAttribute("name"),
        shortcut: shortcutString,
      });
      input.setAttribute("shortcut", shortcutString);
      input.blur();
      break;
    case ShortcutUtils.MODIFIER_REQUIRED:
      if (AppConstants.platform == "macosx")
        setError(input, "shortcuts-modifier-mac");
      else
        setError(input, "shortcuts-modifier-other");
      break;
    case ShortcutUtils.INVALID_COMBINATION:
      setError(input, "shortcuts-invalid");
      break;
    case ShortcutUtils.INVALID_KEY:
      setError(input, "shortcuts-letter");
      break;
  }
}

async function renderAddons(addons) {
  let frag = document.createDocumentFragment();
  for (let addon of addons) {
    let extension = extensionForAddonId(addon.id);

    // Skip this extension if it isn't a webextension.
    if (!extension) continue;

    let card = document.importNode(
      templates.card.content, true).firstElementChild;
    let icon = AddonManager.getPreferredIconURL(addon, 24, window);
    card.setAttribute("addon-id", addon.id);
    card.querySelector(".addon-icon").src = icon;
    card.querySelector(".addon-name").textContent = addon.name;

    if (extension.shortcuts) {
      let commands = await extension.shortcuts.allCommands();

      for (let command of commands) {
        let row = document.importNode(templates.row.content, true);
        let label = row.querySelector(".shortcut-label");
        let descriptionId = getCommandDescriptionId(command);
        if (descriptionId) {
          document.l10n.setAttributes(label, descriptionId);
        } else {
          label.textContent = command.description || command.name;
        }
        let input = row.querySelector(".shortcut-input");
        input.value = getShortcutValue(command.shortcut);
        input.setAttribute("name", command.name);
        input.setAttribute("shortcut", command.shortcut);
        input.addEventListener("keydown", onShortcutChange);
        input.addEventListener("keyup", onShortcutChange);
        input.addEventListener("blur", inputBlurred);
        input.addEventListener("focus", clearValue);

        card.appendChild(row);
      }
    } else {
      card.appendChild(document.importNode(templates.empty.content, true));
    }

    frag.appendChild(card);
  }
  return frag;
}

async function render() {
  loadTemplates();
  let allAddons = await AddonManager.getAddonsByTypes(["extension"]);
  let addons = allAddons
    .filter(addon => !addon.isSystem && addon.isActive)
    .sort((a, b) => a.name.localeCompare(b.name));
  let frag;

  if (addons.length > 0) {
    frag = await renderAddons(addons);
  } else {
    frag = document.importNode(templates.noAddons.content, true);
  }

  let container = document.getElementById("addon-shortcuts");
  container.textContent = "";
  container.appendChild(frag);
}
