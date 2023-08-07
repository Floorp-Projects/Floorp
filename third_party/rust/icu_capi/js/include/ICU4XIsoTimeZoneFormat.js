import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"

export const ICU4XIsoTimeZoneFormat_js_to_rust = {
  "Basic": 0,
  "Extended": 1,
  "UtcBasic": 2,
  "UtcExtended": 3,
};

export const ICU4XIsoTimeZoneFormat_rust_to_js = {
  [0]: "Basic",
  [1]: "Extended",
  [2]: "UtcBasic",
  [3]: "UtcExtended",
};

export const ICU4XIsoTimeZoneFormat = {
  "Basic": "Basic",
  "Extended": "Extended",
  "UtcBasic": "UtcBasic",
  "UtcExtended": "UtcExtended",
};
