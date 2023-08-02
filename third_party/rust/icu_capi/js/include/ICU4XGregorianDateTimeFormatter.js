import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XDateLength_js_to_rust, ICU4XDateLength_rust_to_js } from "./ICU4XDateLength.js"
import { ICU4XError_js_to_rust, ICU4XError_rust_to_js } from "./ICU4XError.js"
import { ICU4XTimeLength_js_to_rust, ICU4XTimeLength_rust_to_js } from "./ICU4XTimeLength.js"

const ICU4XGregorianDateTimeFormatter_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XGregorianDateTimeFormatter_destroy(underlying);
});

export class ICU4XGregorianDateTimeFormatter {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XGregorianDateTimeFormatter_box_destroy_registry.register(this, underlying);
    }
  }

  static create_with_lengths(arg_provider, arg_locale, arg_date_length, arg_time_length) {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XGregorianDateTimeFormatter_create_with_lengths(diplomat_receive_buffer, arg_provider.underlying, arg_locale.underlying, ICU4XDateLength_js_to_rust[arg_date_length], ICU4XTimeLength_js_to_rust[arg_time_length]);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = new ICU4XGregorianDateTimeFormatter(diplomatRuntime.ptrRead(wasm, diplomat_receive_buffer), true, []);
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        return ok_value;
      } else {
        const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
  }

  format_iso_datetime(arg_value) {
    return diplomatRuntime.withWriteable(wasm, (writeable) => {
      return (() => {
        const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
        wasm.ICU4XGregorianDateTimeFormatter_format_iso_datetime(diplomat_receive_buffer, this.underlying, arg_value.underlying, writeable);
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
}
