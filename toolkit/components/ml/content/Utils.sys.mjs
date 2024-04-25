/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/**
 * Converts an ArrayBuffer to a Blob URL.
 *
 * @param {ArrayBuffer} buffer - The ArrayBuffer to convert.
 * @returns {string} The Blob URL.
 */
export function arrayBufferToBlobURL(buffer) {
  let blob = new Blob([buffer], { type: "application/wasm" });
  return URL.createObjectURL(blob);
}

/**
 * Validate some simple Wasm that uses a SIMD operation.
 */
function detectSimdSupport() {
  return WebAssembly.validate(
    new Uint8Array(
      // ```
      // ;; Detect SIMD support.
      // ;; Compile by running: wat2wasm --enable-all simd-detect.wat
      //
      // (module
      //   (func (result v128)
      //     i32.const 0
      //     i8x16.splat
      //     i8x16.popcnt
      //   )
      // )
      // ```

      // prettier-ignore
      [
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x05, 0x01, 0x60, 0x00,
        0x01, 0x7b, 0x03, 0x02, 0x01, 0x00, 0x0a, 0x0a, 0x01, 0x08, 0x00, 0x41, 0x00,
        0xfd, 0x0f, 0xfd, 0x62, 0x0b
      ]
    )
  );
}

let cachedRuntimeWasmFilename = null;

/**
 * Determines the appropriate WebAssembly (Wasm) filename based on the runtime capabilities of the browser.
 * This function considers both SIMD and multi-threading support.
 * It returns a filename that matches the browser's capabilities, ensuring the most optimized version of the Wasm file is used.
 *
 * The result is cached to avoid re-computation.
 *
 * @param {Window|null} browsingContext - The browsing context to use for feature detection.
 * @returns {string} The filename of the Wasm file best suited for the current browser's capabilities.
 */
export function getRuntimeWasmFilename(browsingContext = null) {
  if (cachedRuntimeWasmFilename != null) {
    return cachedRuntimeWasmFilename;
  }

  // The cross-origin isolation flag is used to determine if we have multi-threading support.
  const hasMultiThreadSupport = browsingContext
    ? browsingContext.crossOriginIsolated
    : false;

  let res;
  if (detectSimdSupport()) {
    res = hasMultiThreadSupport
      ? "ort-wasm-simd-threaded.wasm"
      : "ort-wasm-simd.wasm";
  } else {
    res = hasMultiThreadSupport ? "ort-wasm-threaded.wasm" : "ort-wasm.wasm";
  }
  cachedRuntimeWasmFilename = res;
  return res;
}
