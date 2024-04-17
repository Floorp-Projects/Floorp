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

/**
 * Detects if the browser supports multi-threading by checking the availability of `SharedArrayBuffer` and the ability to use WebAssembly threads.
 * This function first checks if `SharedArrayBuffer` is defined, indicating that some form of thread-like behavior might be supported.
 * Then, it attempts to post a message with a `SharedArrayBuffer` object to a `MessageChannel`, testing transferability (important for some browsers like Firefox).
 * Finally, it checks if the current environment can validate a WebAssembly module that includes threading instructions.
 *
 * Taken from https://github.com/microsoft/onnxruntime/blob/262b6bd3b7531503f40f2cb6059d22d7d9d84f27/js/web/lib/wasm/wasm-factory.ts#L31
 *
 * @returns {boolean} True if multi-threading support is detected, otherwise false.
 */
function detectMultiThreadSupport() {
  if (typeof SharedArrayBuffer === "undefined") {
    return false;
  }
  try {
    // Test for transferability of SABs (for browsers. needed for Firefox)
    // https://groups.google.com/forum/#!msg/mozilla.dev.platform/IHkBZlHETpA/dwsMNchWEQAJ
    if (typeof MessageChannel !== "undefined") {
      new MessageChannel().port1.postMessage(new SharedArrayBuffer(1));
    }

    // Test for WebAssembly threads capability (for both browsers and Node.js)
    // This typed array is a WebAssembly program containing threaded instructions.
    return WebAssembly.validate(
      new Uint8Array([
        // ```
        // (module
        //  (type (;0;) (func))
        //  (func (;0;) (type 0)
        //    i32.const 0
        //    i32.atomic.load
        //    drop)
        //  (memory (;0;) 1 1 shared)
        // )
        // ```

        // prettier-ignore
        0, 97, 115, 109, 1, 0, 0, 0, 1, 4, 1, 96, 0, 0, 3, 2, 1, 0, 5, 4, 1, 3,
        1, 1, 10, 11, 1, 9, 0, 65, 0, 254, 16, 2, 0, 26, 11,
      ])
    );
  } catch (e) {
    return false;
  }
}

let cachedRuntimeWasmFilename = null;

/**
 * Determines the appropriate WebAssembly (Wasm) filename based on the runtime capabilities of the browser.
 * This function considers both SIMD and multi-threading support.
 * It returns a filename that matches the browser's capabilities, ensuring the most optimized version of the Wasm file is used.
 *
 * The result is cached to avoid re-computation.
 *
 * @returns {string} The filename of the Wasm file best suited for the current browser's capabilities.
 */
export function getRuntimeWasmFilename() {
  if (cachedRuntimeWasmFilename != null) {
    return cachedRuntimeWasmFilename;
  }
  let res;
  if (detectSimdSupport()) {
    res = detectMultiThreadSupport()
      ? "ort-wasm-simd-threaded.wasm"
      : "ort-wasm-simd.wasm";
  } else {
    res = detectMultiThreadSupport()
      ? "ort-wasm-threaded.wasm"
      : "ort-wasm.wasm";
  }
  cachedRuntimeWasmFilename = res;
  return res;
}
