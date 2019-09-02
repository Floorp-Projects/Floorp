/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/chrome-worker */

"use strict";

importScripts(
  "resource://gre/modules/osfile.jsm",
  "resource://gre/modules/profiler_get_symbols.js"
);

// This worker uses the wasm module that was generated from https://github.com/mstange/profiler-get-symbols.
// See ProfilerGetSymbols.jsm for more information.
//
// The worker instantiates the module, reads the binary into wasm memory, runs
// the wasm code, and returns the symbol table or an error. Then it shuts down
// itself.

const {
  CompactSymbolTable,
  WasmMemBuffer,
  get_compact_symbol_table,
} = wasm_bindgen;

// Read an open OS.File instance into the Uint8Array dataBuf.
function readFileInto(file, dataBuf) {
  // Ideally we'd be able to call file.readTo(dataBuf) here, but readTo no
  // longer exists.
  // So instead, we copy the file over into wasm memory in 4MB chunks. This
  // will take 425 invocations for a a 1.7GB file (such as libxul.so for a
  // Firefox for Android build) and not take up too much memory per call.
  const dataBufLen = dataBuf.byteLength;
  const chunkSize = 4 * 1024 * 1024;
  let pos = 0;
  while (pos < dataBufLen) {
    const chunkData = file.read({ bytes: chunkSize });
    const chunkBytes = chunkData.byteLength;
    if (chunkBytes === 0) {
      break;
    }

    dataBuf.set(chunkData, pos);
    pos += chunkBytes;
  }
}

onmessage = async e => {
  try {
    const { binaryPath, debugPath, breakpadId, module } = e.data;

    if (!(module instanceof WebAssembly.Module)) {
      throw new Error("invalid WebAssembly module");
    }

    // Instantiate the WASM module.
    await wasm_bindgen(module);

    // Read the binary file into WASM memory.
    const binaryFile = OS.File.open(binaryPath, { read: true });
    const binaryData = new WasmMemBuffer(binaryFile.stat().size, array => {
      readFileInto(binaryFile, array);
    });
    binaryFile.close();

    // Do the same for the debug file, if it is supplied and different from the
    // binary file. This is only the case on Windows.
    let debugData = binaryData;
    if (debugPath && debugPath !== binaryPath) {
      const debugFile = OS.File.open(debugPath, { read: true });
      debugData = new WasmMemBuffer(debugFile.stat().size, array => {
        readFileInto(debugFile, array);
      });
      debugFile.close();
    }

    const output = new CompactSymbolTable();
    let succeeded;
    try {
      succeeded = get_compact_symbol_table(
        binaryData,
        debugData,
        breakpadId,
        output
      );
    } catch (e) {
      succeeded = false;
    }

    binaryData.free();
    if (debugData != binaryData) {
      debugData.free();
    }

    if (!succeeded) {
      output.free();
      throw new Error("get_compact_symbol_table returned false");
    }

    const result = [
      output.take_addr(),
      output.take_index(),
      output.take_buffer(),
    ];
    output.free();

    postMessage({ result }, result.map(r => r.buffer));
  } catch (error) {
    postMessage({ error: error.toString() });
  }
  close();
};
