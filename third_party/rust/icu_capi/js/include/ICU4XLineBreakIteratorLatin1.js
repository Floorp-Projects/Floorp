import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"

const ICU4XLineBreakIteratorLatin1_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XLineBreakIteratorLatin1_destroy(underlying);
});

export class ICU4XLineBreakIteratorLatin1 {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XLineBreakIteratorLatin1_box_destroy_registry.register(this, underlying);
    }
  }

  next() {
    return wasm.ICU4XLineBreakIteratorLatin1_next(this.underlying);
  }
}
