import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XBidiDirection_js_to_rust, ICU4XBidiDirection_rust_to_js } from "./ICU4XBidiDirection.js"
import { ICU4XError_js_to_rust, ICU4XError_rust_to_js } from "./ICU4XError.js"

const ICU4XBidiParagraph_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XBidiParagraph_destroy(underlying);
});

export class ICU4XBidiParagraph {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XBidiParagraph_box_destroy_registry.register(this, underlying);
    }
  }

  set_paragraph_in_text(arg_n) {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XBidiParagraph_set_paragraph_in_text(diplomat_receive_buffer, this.underlying, arg_n);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = {};
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        return ok_value;
      } else {
        const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
  }

  direction() {
    return ICU4XBidiDirection_rust_to_js[wasm.ICU4XBidiParagraph_direction(this.underlying)];
  }

  size() {
    return wasm.ICU4XBidiParagraph_size(this.underlying);
  }

  range_start() {
    return wasm.ICU4XBidiParagraph_range_start(this.underlying);
  }

  range_end() {
    return wasm.ICU4XBidiParagraph_range_end(this.underlying);
  }

  reorder_line(arg_range_start, arg_range_end) {
    return diplomatRuntime.withWriteable(wasm, (writeable) => {
      return (() => {
        const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
        wasm.ICU4XBidiParagraph_reorder_line(diplomat_receive_buffer, this.underlying, arg_range_start, arg_range_end, writeable);
        const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
        if (is_ok) {
          const ok_value = {};
          wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
          return ok_value;
        } else {
          const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
          wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
          throw new diplomatRuntime.FFIError(throw_value);
        }
      })();
    });
  }

  level_at(arg_pos) {
    return wasm.ICU4XBidiParagraph_level_at(this.underlying, arg_pos);
  }
}
