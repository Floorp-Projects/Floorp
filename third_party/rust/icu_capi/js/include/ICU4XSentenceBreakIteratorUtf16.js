import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"

const ICU4XSentenceBreakIteratorUtf16_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XSentenceBreakIteratorUtf16_destroy(underlying);
});

export class ICU4XSentenceBreakIteratorUtf16 {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XSentenceBreakIteratorUtf16_box_destroy_registry.register(this, underlying);
    }
  }

  next() {
    return wasm.ICU4XSentenceBreakIteratorUtf16_next(this.underlying);
  }
}
