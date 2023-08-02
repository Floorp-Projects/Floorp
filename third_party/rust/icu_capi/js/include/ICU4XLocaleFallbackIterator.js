import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XLocale } from "./ICU4XLocale.js"

const ICU4XLocaleFallbackIterator_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XLocaleFallbackIterator_destroy(underlying);
});

export class ICU4XLocaleFallbackIterator {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XLocaleFallbackIterator_box_destroy_registry.register(this, underlying);
    }
  }

  get() {
    return new ICU4XLocale(wasm.ICU4XLocaleFallbackIterator_get(this.underlying), true, []);
  }

  step() {
    wasm.ICU4XLocaleFallbackIterator_step(this.underlying);
  }
}
