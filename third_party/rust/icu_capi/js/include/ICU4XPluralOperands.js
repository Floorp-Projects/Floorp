import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XError_js_to_rust, ICU4XError_rust_to_js } from "./ICU4XError.js"

const ICU4XPluralOperands_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XPluralOperands_destroy(underlying);
});

export class ICU4XPluralOperands {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XPluralOperands_box_destroy_registry.register(this, underlying);
    }
  }

  static create_from_string(arg_s) {
    const buf_arg_s = diplomatRuntime.DiplomatBuf.str(wasm, arg_s);
    const diplomat_out = (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XPluralOperands_create_from_string(diplomat_receive_buffer, buf_arg_s.ptr, buf_arg_s.size);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = new ICU4XPluralOperands(diplomatRuntime.ptrRead(wasm, diplomat_receive_buffer), true, []);
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        return ok_value;
      } else {
        const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
    buf_arg_s.free();
    return diplomat_out;
  }
}
