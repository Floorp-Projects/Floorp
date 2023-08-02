import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"

const ICU4XReorderedIndexMap_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XReorderedIndexMap_destroy(underlying);
});

export class ICU4XReorderedIndexMap {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XReorderedIndexMap_box_destroy_registry.register(this, underlying);
    }
  }

  as_slice() {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(8, 4);
      wasm.ICU4XReorderedIndexMap_as_slice(diplomat_receive_buffer, this.underlying);
      const [ptr, size] = new Uint32Array(wasm.memory.buffer, diplomat_receive_buffer, 2);
      wasm.diplomat_free(diplomat_receive_buffer, 8, 4);
      return new Uint32Array(wasm.memory.buffer, ptr, size);
    })();
  }

  len() {
    return wasm.ICU4XReorderedIndexMap_len(this.underlying);
  }

  get(arg_index) {
    return wasm.ICU4XReorderedIndexMap_get(this.underlying, arg_index);
  }
}
