import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XError_js_to_rust, ICU4XError_rust_to_js } from "./ICU4XError.js"

const ICU4XTime_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XTime_destroy(underlying);
});

export class ICU4XTime {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XTime_box_destroy_registry.register(this, underlying);
    }
  }

  static create(arg_hour, arg_minute, arg_second, arg_nanosecond) {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XTime_create(diplomat_receive_buffer, arg_hour, arg_minute, arg_second, arg_nanosecond);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = new ICU4XTime(diplomatRuntime.ptrRead(wasm, diplomat_receive_buffer), true, []);
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        return ok_value;
      } else {
        const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
  }

  hour() {
    return wasm.ICU4XTime_hour(this.underlying);
  }

  minute() {
    return wasm.ICU4XTime_minute(this.underlying);
  }

  second() {
    return wasm.ICU4XTime_second(this.underlying);
  }

  nanosecond() {
    return wasm.ICU4XTime_nanosecond(this.underlying);
  }
}
