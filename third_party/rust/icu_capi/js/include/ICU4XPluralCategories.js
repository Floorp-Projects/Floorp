import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"

export class ICU4XPluralCategories {
  constructor(underlying) {
    this.zero = (new Uint8Array(wasm.memory.buffer, underlying, 1))[0] == 1;
    this.one = (new Uint8Array(wasm.memory.buffer, underlying + 1, 1))[0] == 1;
    this.two = (new Uint8Array(wasm.memory.buffer, underlying + 2, 1))[0] == 1;
    this.few = (new Uint8Array(wasm.memory.buffer, underlying + 3, 1))[0] == 1;
    this.many = (new Uint8Array(wasm.memory.buffer, underlying + 4, 1))[0] == 1;
    this.other = (new Uint8Array(wasm.memory.buffer, underlying + 5, 1))[0] == 1;
  }
}
