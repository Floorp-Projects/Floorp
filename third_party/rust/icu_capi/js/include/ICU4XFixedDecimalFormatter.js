import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XError_js_to_rust, ICU4XError_rust_to_js } from "./ICU4XError.js"
import { ICU4XFixedDecimalGroupingStrategy_js_to_rust, ICU4XFixedDecimalGroupingStrategy_rust_to_js } from "./ICU4XFixedDecimalGroupingStrategy.js"

const ICU4XFixedDecimalFormatter_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XFixedDecimalFormatter_destroy(underlying);
});

export class ICU4XFixedDecimalFormatter {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XFixedDecimalFormatter_box_destroy_registry.register(this, underlying);
    }
  }

  static create_with_grouping_strategy(arg_provider, arg_locale, arg_grouping_strategy) {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XFixedDecimalFormatter_create_with_grouping_strategy(diplomat_receive_buffer, arg_provider.underlying, arg_locale.underlying, ICU4XFixedDecimalGroupingStrategy_js_to_rust[arg_grouping_strategy]);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = new ICU4XFixedDecimalFormatter(diplomatRuntime.ptrRead(wasm, diplomat_receive_buffer), true, []);
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        return ok_value;
      } else {
        const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
  }

  static create_with_decimal_symbols_v1(arg_data_struct, arg_grouping_strategy) {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XFixedDecimalFormatter_create_with_decimal_symbols_v1(diplomat_receive_buffer, arg_data_struct.underlying, ICU4XFixedDecimalGroupingStrategy_js_to_rust[arg_grouping_strategy]);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = new ICU4XFixedDecimalFormatter(diplomatRuntime.ptrRead(wasm, diplomat_receive_buffer), true, []);
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        return ok_value;
      } else {
        const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
  }

  format(arg_value) {
    return diplomatRuntime.withWriteable(wasm, (writeable) => {
      return (() => {
        const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
        wasm.ICU4XFixedDecimalFormatter_format(diplomat_receive_buffer, this.underlying, arg_value.underlying, writeable);
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
