/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* exported render */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  ShortcutUtils: "resource://gre/modules/ShortcutUtils.jsm",
});

const FALLBACK_ICON = "chrome://mozapps/skin/extensions/extensionGeneric.svg";
const COLLAPSE_OPTIONS = {
  limit: 5, // We only want to show 5 when collapsed.
  allowOver: 1, // Avoid collapsing to hide 1 row.
};
const SHORTCUT_KEY_SEPARATOR = "|";

let templatesLoaded = false;
let shortcutKeyMap = new Map();
const templates = {};

function loadTemplates() {
  if (templatesLoaded) {
    return;
  }
  templatesLoaded = true;

  templates.card = document.getElementById("card-template");
  templates.row = document.getElementById("shortcut-row-template");
  templates.noAddons = document.getElementById("shortcuts-no-addons");
  templates.expandRow = document.getElementById("expand-row-template");
  templates.noShortcutAddons = document.getElementById(
    "shortcuts-no-commands-template"
  );
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
let getCommandDescriptionId = command => {
  if (!command.description && builtInNames.has(command.name)) {
    return builtInNames.get(command.name);
  }
  return null;
};

const _functionKeys = [
  "F1",
  "F2",
  "F3",
  "F4",
  "F5",
  "F6",
  "F7",
  "F8",
  "F9",
  "F10",
  "F11",
  "F12",
];
const functionKeys = new Set(_functionKeys);
const validKeys = new Set([
  "Home",
  "End",
  "PageUp",
  "PageDown",
  "Insert",
  "Delete",
  "0",
  "1",
  "2",
  "3",
  "4",
  "5",
  "6",
  "7",
  "8",
  "9",
  ..._functionKeys,
  "MediaNextTrack",
  "MediaPlayPause",
  "MediaPrevTrack",
  "MediaStop",
  "A",
  "B",
  "C",
  "D",
  "E",
  "F",
  "G",
  "H",
  "I",
  "J",
  "K",
  "L",
  "M",
  "N",
  "O",
  "P",
  "Q",
  "R",
  "S",
  "T",
  "U",
  "V",
  "W",
  "X",
  "Y",
  "Z",
  "Up",
  "Down",
  "Left",
  "Right",
  "Comma",
  "Period",
  "Space",
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

function setError(...args) {
  setInputMessage("error", ...args);
}

function setWarning(...args) {
  setInputMessage("warning", ...args);
}

function setInputMessage(type, input, messageId, args) {
  if (!error) {
    error = document.querySelector(".error-message");
  }

  let { x, y, height } = input.getBoundingClientRect();
  error.style.top = `${y + window.scrollY + height - 5}px`;
  error.style.left = `${x}px`;
  error.setAttribute("type", type);
  document.l10n.setAttributes(
    error.querySelector(".error-message-label"),
    messageId,
    args
  );
  error.style.visibility = "visible";
}

function inputBlurred(e) {
  if (!error) {
    error = document.querySelector(".error-message");
  }

  error.style.visibility = "hidden";
  e.target.value = getShortcutValue(e.target.getAttribute("shortcut"));
}

function onFocus(e) {
  e.target.value = "";

  let warning = e.target.getAttribute("warning");
  if (warning) {
    setWarning(e.target, warning);
  }
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

function buildDuplicateShortcutsMap(addons) {
  return Promise.all(
    addons.map(async addon => {
      let extension = extensionForAddonId(addon.id);
      if (extension && extension.shortcuts) {
        let commands = await extension.shortcuts.allCommands();
        for (let command of commands) {
          recordShortcut(command.shortcut, addon.name, command.name);
        }
      }
    })
  );
}

function recordShortcut(shortcut, addonName, commandName) {
  if (!shortcut) {
    return;
  }
  let addons = shortcutKeyMap.get(shortcut);
  let addonString = `${addonName}${SHORTCUT_KEY_SEPARATOR}${commandName}`;
  if (addons) {
    addons.add(addonString);
  } else {
    shortcutKeyMap.set(shortcut, new Set([addonString]));
  }
}

function removeShortcut(shortcut, addonName, commandName) {
  let addons = shortcutKeyMap.get(shortcut);
  let addonString = `${addonName}${SHORTCUT_KEY_SEPARATOR}${commandName}`;
  if (addons) {
    addons.delete(addonString);
    if (addons.size === 0) {
      shortcutKeyMap.delete(shortcut);
    }
  }
}

function getAddonName(shortcut) {
  let addons = shortcutKeyMap.get(shortcut);
  // Get the first addon name with given shortcut.
  let name = addons.values().next().value;
  return name.split(SHORTCUT_KEY_SEPARATOR)[0];
}

function setDuplicateWarnings() {
  let warningHolder = document.getElementById("duplicate-warning-messages");
  clearWarnings(warningHolder);
  for (let [shortcut, addons] of shortcutKeyMap) {
    if (addons.size > 1) {
      warningHolder.appendChild(createDuplicateWarningBar(shortcut));
      markDuplicates(shortcut);
    }
  }
}

function clearWarnings(warningHolder) {
  warningHolder.textContent = "";
  let inputs = document.querySelectorAll(".shortcut-input[warning]");
  for (let input of inputs) {
    input.removeAttribute("warning");
    let row = input.closest(".shortcut-row");
    if (row.hasAttribute("hide-before-expand")) {
      row
        .closest(".card")
        .querySelector(".expand-button")
        .removeAttribute("warning");
    }
  }
}

function createDuplicateWarningBar(shortcut) {
  let messagebar = document.createElement("message-bar");
  messagebar.setAttribute("type", "warning");

  let message = document.createElement("span");
  document.l10n.setAttributes(message, "shortcuts-duplicate-warning-message", {
    shortcut,
  });

  messagebar.append(message);
  return messagebar;
}

function markDuplicates(shortcut) {
  let inputs = document.querySelectorAll(
    `.shortcut-input[shortcut="${shortcut}"]`
  );
  for (let input of inputs) {
    input.setAttribute("warning", "shortcuts-duplicate");
    let row = input.closest(".shortcut-row");
    if (row.hasAttribute("hide-before-expand")) {
      row
        .closest(".card")
        .querySelector(".expand-button")
        .setAttribute("warning", "shortcuts-duplicate");
    }
  }
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

  // Some system actions aren't in the keyset, handle them independantly.
  if (ShortcutUtils.getSystemActionForEvent(e)) {
    e.defaultCancelled = true;
    setError(input, "shortcuts-system");
    return;
  }

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

      let addonId = input.closest(".card").getAttribute("addon-id");
      let extension = extensionForAddonId(addonId);

      // Check if shortcut is already assigned.
      if (shortcutKeyMap.has(shortcutString)) {
        setError(input, "shortcuts-exists", {
          addon: getAddonName(shortcutString),
        });
        break;
      } else {
        // Update the shortcut if it isn't reserved or assigned.
        let oldShortcut = input.getAttribute("shortcut");
        let addonName = input.closest(".card").getAttribute("addon-name");
        let commandName = input.getAttribute("name");

        removeShortcut(oldShortcut, addonName, commandName);
        recordShortcut(shortcutString, addonName, commandName);
      }

      // This is async, but we're not awaiting it to keep the handler sync.
      extension.shortcuts.updateCommand({
        name: input.getAttribute("name"),
        shortcut: shortcutString,
      });
      input.setAttribute("shortcut", shortcutString);
      input.blur();
      setDuplicateWarnings();
      break;
    case ShortcutUtils.MODIFIER_REQUIRED:
      if (AppConstants.platform == "macosx") {
        setError(input, "shortcuts-modifier-mac");
      } else {
        setError(input, "shortcuts-modifier-other");
      }
      break;
    case ShortcutUtils.INVALID_COMBINATION:
      setError(input, "shortcuts-invalid");
      break;
    case ShortcutUtils.INVALID_KEY:
      setError(input, "shortcuts-letter");
      break;
  }
}

function renderNoShortcutAddons(addons) {
  let fragment = document.importNode(templates.noShortcutAddons.content, true);
  let list = fragment.querySelector(".shortcuts-no-commands-list");
  for (let addon of addons) {
    let addonItem = document.createElement("li");
    addonItem.textContent = addon.name;
    addonItem.setAttribute("addon-id", addon.id);
    list.appendChild(addonItem);
  }

  return fragment;
}

async function renderAddons(addons) {
  let frag = document.createDocumentFragment();
  let noShortcutAddons = [];

  await buildDuplicateShortcutsMap(addons);

  let isDuplicate = command => {
    if (command.shortcut) {
      let dupes = shortcutKeyMap.get(command.shortcut);
      return dupes.size > 1;
    }
    return false;
  };

  for (let addon of addons) {
    let extension = extensionForAddonId(addon.id);

    // Skip this extension if it isn't a webextension.
    if (!extension) {
      continue;
    }

    if (extension.shortcuts) {
      let card = document.importNode(templates.card.content, true)
        .firstElementChild;
      let icon = AddonManager.getPreferredIconURL(addon, 24, window);
      card.setAttribute("addon-id", addon.id);
      card.setAttribute("addon-name", addon.name);
      card.querySelector(".addon-icon").src = icon || FALLBACK_ICON;
      card.querySelector(".addon-name").textContent = addon.name;

      let commands = await extension.shortcuts.allCommands();

      // Sort the commands so the ones with shortcuts are at the top.
      commands.sort((a, b) => {
        if (isDuplicate(a) && isDuplicate(b)) {
          return 0;
        }
        if (isDuplicate(a)) {
          return -1;
        }
        if (isDuplicate(b)) {
          return 1;
        }
        // Boolean compare the shortcuts to see if they're both set or unset.
        if (!a.shortcut == !b.shortcut) {
          return 0;
        }
        if (a.shortcut) {
          return -1;
        }
        return 1;
      });

      let { limit, allowOver } = COLLAPSE_OPTIONS;
      let willHideCommands = commands.length > limit + allowOver;
      let firstHiddenInput;

      for (let i = 0; i < commands.length; i++) {
        let command = commands[i];

        let row = document.importNode(templates.row.content, true)
          .firstElementChild;

        if (willHideCommands && i >= limit) {
          row.setAttribute("hide-before-expand", "true");
        }

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
        input.addEventListener("focus", onFocus);

        if (willHideCommands && i == limit) {
          firstHiddenInput = input;
        }

        card.appendChild(row);
      }

      // Add an expand button, if needed.
      if (willHideCommands) {
        let row = document.importNode(templates.expandRow.content, true);
        let button = row.querySelector(".expand-button");
        let numberToShow = commands.length - limit;
        let setLabel = type => {
          document.l10n.setAttributes(button, `shortcuts-card-${type}-button`, {
            numberToShow,
          });
        };

        setLabel("expand");
        button.addEventListener("click", event => {
          let expanded = card.hasAttribute("expanded");
          if (expanded) {
            card.removeAttribute("expanded");
            setLabel("expand");
          } else {
            card.setAttribute("expanded", "true");
            setLabel("collapse");
            // If this as a keyboard event then focus the next input.
            if (event.mozInputSource == MouseEvent.MOZ_SOURCE_KEYBOARD) {
              firstHiddenInput.focus();
            }
          }
        });
        card.appendChild(row);
      }

      frag.appendChild(card);
    } else if (!addon.hidden) {
      noShortcutAddons.push({ id: addon.id, name: addon.name });
    }
  }

  if (noShortcutAddons.length > 0) {
    frag.appendChild(renderNoShortcutAddons(noShortcutAddons));
  }

  return frag;
}

async function render() {
  loadTemplates();
  let allAddons = await AddonManager.getAddonsByTypes(["extension"]);
  let addons = allAddons
    .filter(addon => addon.isActive)
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
  setDuplicateWarnings();
}
