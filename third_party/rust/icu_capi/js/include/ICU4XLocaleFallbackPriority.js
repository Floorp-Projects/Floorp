import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"

export const ICU4XLocaleFallbackPriority_js_to_rust = {
  "Language": 0,
  "Region": 1,
  "Collation": 2,
};

export const ICU4XLocaleFallbackPriority_rust_to_js = {
  [0]: "Language",
  [1]: "Region",
  [2]: "Collation",
};

export const ICU4XLocaleFallbackPriority = {
  "Language": "Language",
  "Region": "Region",
  "Collation": "Collation",
};
