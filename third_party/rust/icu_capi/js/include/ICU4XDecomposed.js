import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"

export class ICU4XDecomposed {
  constructor(underlying) {
    this.first = String.fromCharCode((new Uint32Array(wasm.memory.buffer, underlying, 1))[0]);
    this.second = String.fromCharCode((new Uint32Array(wasm.memory.buffer, underlying + 4, 1))[0]);
  }
}
