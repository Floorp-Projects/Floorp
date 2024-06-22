/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//? originally from https://searchfox.org/mozilla-central/rev/91f80b37a4b2d4c6b05e8403e26bd1f0df3f6057/toolkit/mozapps/extensions/content/shortcuts.js#408

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);

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
 * @param string The input string.
 * @returns The trimmed string, or unchanged.
 */
function trimPrefix(string: string) {
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
function remapKey(string: string) {
  if (Object.hasOwn(remapKeys, string)) {
    //@ts-expect-error
    return remapKeys[string];
  }
  return string;
}

const keyOptions = [
  // biome-ignore lint/suspicious/noExplicitAny: <explanation>
  (e: any) => String.fromCharCode(e.which), // A letter?
  // biome-ignore lint/suspicious/noExplicitAny: <explanation>
  (e: any) => e.code.toUpperCase(), // A letter.
  // biome-ignore lint/suspicious/noExplicitAny: <explanation>
  (e: any) => trimPrefix(e.code), // Digit3, ArrowUp, Numpad9.
  // biome-ignore lint/suspicious/noExplicitAny: <explanation>
  (e: any) => trimPrefix(e.key), // Digit3, ArrowUp, Numpad9.
  // biome-ignore lint/suspicious/noExplicitAny: <explanation>
  (e: any) => remapKey(e.key), // Comma, Period, Space.
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
function getStringForEvent(event: Event) {
  for (const option of keyOptions) {
    const value = option(event);
    if (validKeys.has(value)) {
      return value;
    }
  }

  return "";
}

// biome-ignore lint/suspicious/noExplicitAny: <explanation>
function getShortcutForEvent(e: any) {
  let modifierMap: {
    MacCtrl?: boolean;
    Ctrl?: boolean;
    Alt: boolean;
    Command?: boolean;
    Shift: boolean;
  };

  if (AppConstants.platform === "macosx") {
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
    .filter(([, isDown]) => isDown)
    .map(([key]) => key)
    .concat(getStringForEvent(e))
    .join("+");
}

const { ShortcutUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/ShortcutUtils.sys.mjs",
);

export function checkIsSystemShortcut(ev: Event): boolean {
  const shortcutString = getShortcutForEvent(ev);
  //@ts-ignore
  const chromeWindow = window.windowRoot.ownerGlobal;
  return ShortcutUtils.isSystem(chromeWindow, shortcutString) ?? false;
}
