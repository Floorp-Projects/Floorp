import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XError_js_to_rust, ICU4XError_rust_to_js } from "./ICU4XError.js"
import { ICU4XPluralCategories } from "./ICU4XPluralCategories.js"
import { ICU4XPluralCategory_js_to_rust, ICU4XPluralCategory_rust_to_js } from "./ICU4XPluralCategory.js"

const ICU4XPluralRules_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XPluralRules_destroy(underlying);
});

export class ICU4XPluralRules {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XPluralRules_box_destroy_registry.register(this, underlying);
    }
  }

  static create_cardinal(arg_provider, arg_locale) {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XPluralRules_create_cardinal(diplomat_receive_buffer, arg_provider.underlying, arg_locale.underlying);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = new ICU4XPluralRules(diplomatRuntime.ptrRead(wasm, diplomat_receive_buffer), true, []);
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        return ok_value;
      } else {
        const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
  }

  static create_ordinal(arg_provider, arg_locale) {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XPluralRules_create_ordinal(diplomat_receive_buffer, arg_provider.underlying, arg_locale.underlying);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = new ICU4XPluralRules(diplomatRuntime.ptrRead(wasm, diplomat_receive_buffer), true, []);
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        return ok_value;
      } else {
        const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
  }

  category_for(arg_op) {
    return ICU4XPluralCategory_rust_to_js[wasm.ICU4XPluralRules_category_for(this.underlying, arg_op.underlying)];
  }

  categories() {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(6, 1);
      wasm.ICU4XPluralRules_categories(diplomat_receive_buffer, this.underlying);
      const out = new ICU4XPluralCategories(diplomat_receive_buffer);
      wasm.diplomat_free(diplomat_receive_buffer, 6, 1);
      return out;
    })();
  }
}
