/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const KEY_DATA = {
  " ": { code: "Space" },
  "!": { code: "Digit1", shifted: true },
  "#": { code: "Digit3", shifted: true },
  $: { code: "Digit4", shifted: true },
  "%": { code: "Digit5", shifted: true },
  "&": { code: "Digit7", shifted: true },
  "'": { code: "Quote" },
  "(": { code: "Digit9", shifted: true },
  ")": { code: "Digit0", shifted: true },
  "*": { code: "Digit8", shifted: true },
  "+": { code: "Equal", shifted: true },
  ",": { code: "Comma" },
  "-": { code: "Minus" },
  ".": { code: "Period" },
  "/": { code: "Slash" },
  0: { code: "Digit0" },
  1: { code: "Digit1" },
  2: { code: "Digit2" },
  3: { code: "Digit3" },
  4: { code: "Digit4" },
  5: { code: "Digit5" },
  6: { code: "Digit6" },
  7: { code: "Digit7" },
  8: { code: "Digit8" },
  9: { code: "Digit9" },
  ":": { code: "Semicolon", shifted: true },
  ";": { code: "Semicolon" },
  "<": { code: "Comma", shifted: true },
  "=": { code: "Equal" },
  ">": { code: "Period", shifted: true },
  "?": { code: "Slash", shifted: true },
  "@": { code: "Digit2", shifted: true },
  A: { code: "KeyA", shifted: true },
  B: { code: "KeyB", shifted: true },
  C: { code: "KeyC", shifted: true },
  D: { code: "KeyD", shifted: true },
  E: { code: "KeyE", shifted: true },
  F: { code: "KeyF", shifted: true },
  G: { code: "KeyG", shifted: true },
  H: { code: "KeyH", shifted: true },
  I: { code: "KeyI", shifted: true },
  J: { code: "KeyJ", shifted: true },
  K: { code: "KeyK", shifted: true },
  L: { code: "KeyL", shifted: true },
  M: { code: "KeyM", shifted: true },
  N: { code: "KeyN", shifted: true },
  O: { code: "KeyO", shifted: true },
  P: { code: "KeyP", shifted: true },
  Q: { code: "KeyQ", shifted: true },
  R: { code: "KeyR", shifted: true },
  S: { code: "KeyS", shifted: true },
  T: { code: "KeyT", shifted: true },
  U: { code: "KeyU", shifted: true },
  V: { code: "KeyV", shifted: true },
  W: { code: "KeyW", shifted: true },
  X: { code: "KeyX", shifted: true },
  Y: { code: "KeyY", shifted: true },
  Z: { code: "KeyZ", shifted: true },
  "[": { code: "BracketLeft" },
  '"': { code: "Quote", shifted: true },
  "\\": { code: "Backslash" },
  "]": { code: "BracketRight" },
  "^": { code: "Digit6", shifted: true },
  _: { code: "Minus", shifted: true },
  "`": { code: "Backquote" },
  a: { code: "KeyA" },
  b: { code: "KeyB" },
  c: { code: "KeyC" },
  d: { code: "KeyD" },
  e: { code: "KeyE" },
  f: { code: "KeyF" },
  g: { code: "KeyG" },
  h: { code: "KeyH" },
  i: { code: "KeyI" },
  j: { code: "KeyJ" },
  k: { code: "KeyK" },
  l: { code: "KeyL" },
  m: { code: "KeyM" },
  n: { code: "KeyN" },
  o: { code: "KeyO" },
  p: { code: "KeyP" },
  q: { code: "KeyQ" },
  r: { code: "KeyR" },
  s: { code: "KeyS" },
  t: { code: "KeyT" },
  u: { code: "KeyU" },
  v: { code: "KeyV" },
  w: { code: "KeyW" },
  x: { code: "KeyX" },
  y: { code: "KeyY" },
  z: { code: "KeyZ" },
  "{": { code: "BracketLeft", shifted: true },
  "|": { code: "Backslash", shifted: true },
  "}": { code: "BracketRight", shifted: true },
  "~": { code: "Backquote", shifted: true },
  "\uE000": { key: "Unidentified", printable: false },
  "\uE001": { key: "Cancel", printable: false },
  "\uE002": { code: "Help", key: "Help", printable: false },
  "\uE003": { code: "Backspace", key: "Backspace", printable: false },
  "\uE004": { code: "Tab", key: "Tab", printable: false },
  "\uE005": { code: "", key: "Clear", printable: false },
  "\uE006": { code: "Enter", key: "Enter", printable: false },
  "\uE007": {
    code: "NumpadEnter",
    key: "Enter",
    location: 1,
    printable: false,
  },
  "\uE008": {
    code: "ShiftLeft",
    key: "Shift",
    location: 1,
    modifier: "shiftKey",
    printable: false,
  },
  "\uE009": {
    code: "ControlLeft",
    key: "Control",
    location: 1,
    modifier: "ctrlKey",
    printable: false,
  },
  "\uE00A": {
    code: "AltLeft",
    key: "Alt",
    location: 1,
    modifier: "altKey",
    printable: false,
  },
  "\uE00B": { code: "", key: "Pause", printable: false },
  "\uE00C": { code: "Escape", key: "Escape", printable: false },
  "\uE00D": { code: "Space", key: " ", shifted: true },
  "\uE00E": { code: "PageUp", key: "PageUp", printable: false },
  "\uE00F": { code: "PageDown", key: "PageDown", printable: false },
  "\uE010": { code: "End", key: "End", printable: false },
  "\uE011": { code: "Home", key: "Home", printable: false },
  "\uE012": { code: "ArrowLeft", key: "ArrowLeft", printable: false },
  "\uE013": { code: "ArrowUp", key: "ArrowUp", printable: false },
  "\uE014": { code: "ArrowRight", key: "ArrowRight", printable: false },
  "\uE015": { code: "ArrowDown", key: "ArrowDown", printable: false },
  "\uE016": { code: "Insert", key: "Insert", printable: false },
  "\uE017": { code: "Delete", key: "Delete", printable: false },
  "\uE018": { code: "", key: ";" },
  "\uE019": { code: "", key: "=" },
  "\uE01A": { code: "Numpad0", key: "0", location: 3 },
  "\uE01B": { code: "Numpad1", key: "1", location: 3 },
  "\uE01C": { code: "Numpad2", key: "2", location: 3 },
  "\uE01D": { code: "Numpad3", key: "3", location: 3 },
  "\uE01E": { code: "Numpad4", key: "4", location: 3 },
  "\uE01F": { code: "Numpad5", key: "5", location: 3 },
  "\uE020": { code: "Numpad6", key: "6", location: 3 },
  "\uE021": { code: "Numpad7", key: "7", location: 3 },
  "\uE022": { code: "Numpad8", key: "8", location: 3 },
  "\uE023": { code: "Numpad9", key: "9", location: 3 },
  "\uE024": { code: "NumpadMultiply", key: "*", location: 3 },
  "\uE025": { code: "NumpadAdd", key: "+", location: 3 },
  "\uE026": { code: "NumpadComma", key: ",", location: 3 },
  "\uE027": { code: "NumpadSubtract", key: "-", location: 3 },
  "\uE028": { code: "NumpadDecimal", key: ".", location: 3 },
  "\uE029": { code: "NumpadDivide", key: "/", location: 3 },
  "\uE031": { code: "F1", key: "F1", printable: false },
  "\uE032": { code: "F2", key: "F2", printable: false },
  "\uE033": { code: "F3", key: "F3", printable: false },
  "\uE034": { code: "F4", key: "F4", printable: false },
  "\uE035": { code: "F5", key: "F5", printable: false },
  "\uE036": { code: "F6", key: "F6", printable: false },
  "\uE037": { code: "F7", key: "F7", printable: false },
  "\uE038": { code: "F8", key: "F8", printable: false },
  "\uE039": { code: "F9", key: "F9", printable: false },
  "\uE03A": { code: "F10", key: "F10", printable: false },
  "\uE03B": { code: "F11", key: "F11", printable: false },
  "\uE03C": { code: "F12", key: "F12", printable: false },
  "\uE03D": {
    code: "MetaLeft",
    key: "Meta",
    location: 1,
    modifier: "metaKey",
    printable: false,
  },
  "\uE040": { code: "", key: "ZenkakuHankaku", printable: false },
  "\uE050": {
    code: "ShiftRight",
    key: "Shift",
    location: 2,
    modifier: "shiftKey",
    printable: false,
  },
  "\uE051": {
    code: "ControlRight",
    key: "Control",
    location: 2,
    modifier: "ctrlKey",
    printable: false,
  },
  "\uE052": {
    code: "AltRight",
    key: "Alt",
    location: 2,
    modifier: "altKey",
    printable: false,
  },
  "\uE053": {
    code: "MetaRight",
    key: "Meta",
    location: 2,
    modifier: "metaKey",
    printable: false,
  },
  "\uE054": {
    code: "Numpad9",
    key: "PageUp",
    location: 3,
    printable: false,
    shifted: true,
  },
  "\uE055": {
    code: "Numpad3",
    key: "PageDown",
    location: 3,
    printable: false,
    shifted: true,
  },
  "\uE056": {
    code: "Numpad1",
    key: "End",
    location: 3,
    printable: false,
    shifted: true,
  },
  "\uE057": {
    code: "Numpad7",
    key: "Home",
    location: 3,
    printable: false,
    shifted: true,
  },
  "\uE058": {
    code: "Numpad4",
    key: "ArrowLeft",
    location: 3,
    printable: false,
    shifted: true,
  },
  "\uE059": {
    code: "Numpad8",
    key: "ArrowUp",
    location: 3,
    printable: false,
    shifted: true,
  },
  "\uE05A": {
    code: "Numpad6",
    key: "ArrowRight",
    location: 3,
    printable: false,
    shifted: true,
  },
  "\uE05B": {
    code: "Numpad2",
    key: "ArrowDown",
    location: 3,
    printable: false,
    shifted: true,
  },
  "\uE05C": {
    code: "Numpad0",
    key: "Insert",
    location: 3,
    printable: false,
    shifted: true,
  },
  "\uE05D": {
    code: "NumpadDecimal",
    key: "Delete",
    location: 3,
    printable: false,
    shifted: true,
  },
};

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "SHIFT_DATA", () => {
  // Initalize the shift mapping
  const shiftData = new Map();
  const byCode = new Map();
  for (let [key, props] of Object.entries(KEY_DATA)) {
    if (props.code) {
      if (!byCode.has(props.code)) {
        byCode.set(props.code, [null, null]);
      }
      byCode.get(props.code)[props.shifted ? 1 : 0] = key;
    }
  }
  for (let [unshifted, shifted] of byCode.values()) {
    if (unshifted !== null && shifted !== null) {
      shiftData.set(unshifted, shifted);
    }
  }
  return shiftData;
});

export const keyData = {
  /**
   * Get key event data for a given key character.
   *
   * @param {string} rawKey
   *     Key for which to get data. This can either be the key codepoint
   *     itself or one of the codepoints in the range U+E000-U+E05D that
   *     WebDriver uses to represent keys not corresponding directly to
   *     a codepoint.
   * @returns {object} Key event data object.
   */
  getData(rawKey) {
    let keyData = { key: rawKey, location: 0, printable: true, shifted: false };
    if (KEY_DATA.hasOwnProperty(rawKey)) {
      keyData = { ...keyData, ...KEY_DATA[rawKey] };
    }
    return keyData;
  },

  /**
   * Get shifted key character for a given key character.
   *
   * For characters unaffected by the shift key, this returns the input.
   *
   * @param {string} rawKey Key for which to get shifted key.
   * @returns {string} Key string to use when the shift modifier is set.
   */
  getShiftedKey(rawKey) {
    return lazy.SHIFT_DATA.get(rawKey) ?? rawKey;
  },
};
