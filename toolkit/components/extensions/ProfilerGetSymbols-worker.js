/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* eslint-env mozilla/chrome-worker */

"use strict";

importScripts("resource://gre/modules/osfile.jsm",
              "resource://gre/modules/profiler_get_symbols.js");

// This worker uses the wasm module that was generated from https://github.com/mstange/profiler-get-symbols.
// See ProfilerGetSymbols.jsm for more information.
//
// The worker instantiates the module, reads the binary into wasm memory, runs
// the wasm code, and returns the symbol table or an error. Then it shuts down
// itself.

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
    const chunkData = file.read({bytes: chunkSize});
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
    const {binaryPath, debugPath, breakpadId, module} = e.data;

    if (!(module instanceof WebAssembly.Module)) {
      throw new Error("invalid WebAssembly module");
    }

    // Instantiate the WASM module.
    await wasm_bindgen(module);

    const {CompactSymbolTable, wasm} = wasm_bindgen;

    const binaryFile = OS.File.open(binaryPath, {read: true});
    const binaryDataBufLen = binaryFile.stat().size;

    // Read the binary file into WASM memory.
    const binaryDataBufPtr = wasm.__wbindgen_malloc(binaryDataBufLen);
    const binaryDataBuf = new Uint8Array(wasm.memory.buffer, binaryDataBufPtr, binaryDataBufLen);
    readFileInto(binaryFile, binaryDataBuf);
    binaryFile.close();

    // Do the same for the debug file, if it is supplied and different from the
    // binary file. This is only the case on Windows.
    let debugDataBufLen = binaryDataBufLen;
    let debugDataBufPtr = binaryDataBufPtr;
    if (debugPath && debugPath !== binaryPath) {
      const debugFile = OS.File.open(debugPath, {read: true});
      debugDataBufLen = debugFile.stat().size;
      debugDataBufPtr = wasm.__wbindgen_malloc(debugDataBufLen);
      const debugDataBuf = new Uint8Array(wasm.memory.buffer, debugDataBufPtr, debugDataBufLen);
      readFileInto(debugFile, debugDataBuf);
      debugFile.close();
    }

    // Call get_compact_symbol_table. We're calling the raw wasm function
    // instead of the binding function that wasm-bindgen generated for us,
    // because the generated function doesn't let us pass binaryDataBufPtr
    // or debugDataBufPtr and would force another copy.
    //
    // The rust definition of get_compact_symbol_table is:
    //
    // #[wasm_bindgen]
    // pub fn get_compact_symbol_table(binary_data: &[u8], debug_data: &[u8], breakpad_id: &str, dest: &mut CompactSymbolTable) -> bool
    //
    // It gets exposed as a wasm function with the following signature:
    //
    // pub fn get_compact_symbol_table(binaryDataBufPtr: u32, binaryDataBufLen: u32, debugDataBufPtr: u32, debugDataBufLen: u32, breakpadIdPtr: u32, breakpadIdLen: u32, destPtr: u32) -> u32
    //
    // We're relying on implementation details of wasm-bindgen here. The above
    // is true as of wasm-bindgen 0.2.32.

    // Convert the breakpadId string into bytes in wasm memory.
    const breakpadIdBuf = new TextEncoder("utf-8").encode(breakpadId);
    const breakpadIdLen = breakpadIdBuf.length;
    const breakpadIdPtr = wasm.__wbindgen_malloc(breakpadIdLen);
    new Uint8Array(wasm.memory.buffer).set(breakpadIdBuf, breakpadIdPtr);

    const output = new CompactSymbolTable();
    let succeeded;
    try {
      succeeded =
        wasm.get_compact_symbol_table(binaryDataBufPtr, binaryDataBufLen,
                                      debugDataBufPtr, debugDataBufLen,
                                      breakpadIdPtr, breakpadIdLen,
                                      output.ptr) !== 0;
    } catch (e) {
      succeeded = false;
    }

    wasm.__wbindgen_free(breakpadIdPtr, breakpadIdLen);
    wasm.__wbindgen_free(binaryDataBufPtr, binaryDataBufLen);
    if (debugDataBufPtr != binaryDataBufPtr) {
      wasm.__wbindgen_free(debugDataBufPtr, debugDataBufLen);
    }

    if (!succeeded) {
      output.free();
      throw new Error("get_compact_symbol_table returned false");
    }

    const result = [output.take_addr(), output.take_index(), output.take_buffer()];
    output.free();

    postMessage({result}, result.map(r => r.buffer));
  } catch (error) {
    postMessage({error: error.toString()});
  }
  close();
};
