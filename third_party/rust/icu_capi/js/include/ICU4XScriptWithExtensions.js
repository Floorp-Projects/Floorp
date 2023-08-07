import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { CodePointRangeIterator } from "./CodePointRangeIterator.js"
import { ICU4XError_js_to_rust, ICU4XError_rust_to_js } from "./ICU4XError.js"
import { ICU4XScriptWithExtensionsBorrowed } from "./ICU4XScriptWithExtensionsBorrowed.js"

const ICU4XScriptWithExtensions_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XScriptWithExtensions_destroy(underlying);
});

export class ICU4XScriptWithExtensions {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XScriptWithExtensions_box_destroy_registry.register(this, underlying);
    }
  }

  static create(arg_provider) {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XScriptWithExtensions_create(diplomat_receive_buffer, arg_provider.underlying);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = new ICU4XScriptWithExtensions(diplomatRuntime.ptrRead(wasm, diplomat_receive_buffer), true, []);
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        return ok_value;
      } else {
        const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
  }

  get_script_val(arg_code_point) {
    return wasm.ICU4XScriptWithExtensions_get_script_val(this.underlying, arg_code_point);
  }

  has_script(arg_code_point, arg_script) {
    return wasm.ICU4XScriptWithExtensions_has_script(this.underlying, arg_code_point, arg_script);
  }

  as_borrowed() {
    return new ICU4XScriptWithExtensionsBorrowed(wasm.ICU4XScriptWithExtensions_as_borrowed(this.underlying), true, [this]);
  }

  iter_ranges_for_script(arg_script) {
    return new CodePointRangeIterator(wasm.ICU4XScriptWithExtensions_iter_ranges_for_script(this.underlying, arg_script), true, [this]);
  }
}
