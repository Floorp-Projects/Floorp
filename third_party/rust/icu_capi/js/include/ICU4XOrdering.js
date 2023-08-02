import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"

export const ICU4XOrdering_js_to_rust = {
  "Less": -1,
  "Equal": 0,
  "Greater": 1,
};

export const ICU4XOrdering_rust_to_js = {
  [-1]: "Less",
  [0]: "Equal",
  [1]: "Greater",
};

export const ICU4XOrdering = {
  "Less": "Less",
  "Equal": "Equal",
  "Greater": "Greater",
};
