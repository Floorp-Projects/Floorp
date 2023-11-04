/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let isSimdSupported = false;

/**
 * WebAssembly counts as unsafe eval in privileged contexts, so we have to execute this
 * code in a ChromeWorker. The code feature detects SIMD support. The comment above
 * the binary code is the .wat version of the .wasm binary.
 */

try {
  new WebAssembly.Module(
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
  isSimdSupported = true;
} catch (error) {
  console.error(`Translations: SIMD not supported`, error);
}

postMessage({ isSimdSupported });
