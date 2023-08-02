import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XIsoTimeZoneFormat_js_to_rust, ICU4XIsoTimeZoneFormat_rust_to_js } from "./ICU4XIsoTimeZoneFormat.js"
import { ICU4XIsoTimeZoneMinuteDisplay_js_to_rust, ICU4XIsoTimeZoneMinuteDisplay_rust_to_js } from "./ICU4XIsoTimeZoneMinuteDisplay.js"
import { ICU4XIsoTimeZoneSecondDisplay_js_to_rust, ICU4XIsoTimeZoneSecondDisplay_rust_to_js } from "./ICU4XIsoTimeZoneSecondDisplay.js"

export class ICU4XIsoTimeZoneOptions {
  constructor(underlying) {
    this.format = ICU4XIsoTimeZoneFormat_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, underlying)];
    this.minutes = ICU4XIsoTimeZoneMinuteDisplay_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, underlying + 4)];
    this.seconds = ICU4XIsoTimeZoneSecondDisplay_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, underlying + 8)];
  }
}
