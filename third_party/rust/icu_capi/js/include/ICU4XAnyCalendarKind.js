import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XError_js_to_rust, ICU4XError_rust_to_js } from "./ICU4XError.js"

export const ICU4XAnyCalendarKind_js_to_rust = {
  "Iso": 0,
  "Gregorian": 1,
  "Buddhist": 2,
  "Japanese": 3,
  "JapaneseExtended": 4,
  "Ethiopian": 5,
  "EthiopianAmeteAlem": 6,
  "Indian": 7,
  "Coptic": 8,
};

export const ICU4XAnyCalendarKind_rust_to_js = {
  [0]: "Iso",
  [1]: "Gregorian",
  [2]: "Buddhist",
  [3]: "Japanese",
  [4]: "JapaneseExtended",
  [5]: "Ethiopian",
  [6]: "EthiopianAmeteAlem",
  [7]: "Indian",
  [8]: "Coptic",
};

export const ICU4XAnyCalendarKind = {
  "Iso": "Iso",
  "Gregorian": "Gregorian",
  "Buddhist": "Buddhist",
  "Japanese": "Japanese",
  "JapaneseExtended": "JapaneseExtended",
  "Ethiopian": "Ethiopian",
  "EthiopianAmeteAlem": "EthiopianAmeteAlem",
  "Indian": "Indian",
  "Coptic": "Coptic",
};
