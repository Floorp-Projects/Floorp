import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XLineBreakStrictness_js_to_rust, ICU4XLineBreakStrictness_rust_to_js } from "./ICU4XLineBreakStrictness.js"
import { ICU4XLineBreakWordOption_js_to_rust, ICU4XLineBreakWordOption_rust_to_js } from "./ICU4XLineBreakWordOption.js"

export class ICU4XLineBreakOptionsV1 {
  constructor(underlying) {
    this.strictness = ICU4XLineBreakStrictness_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, underlying)];
    this.word_option = ICU4XLineBreakWordOption_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, underlying + 4)];
    this.ja_zh = (new Uint8Array(wasm.memory.buffer, underlying + 8, 1))[0] == 1;
  }
}
