/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const EXPORTED_SYMBOLS = ["ProfilerGetSymbols"];

ChromeUtils.defineModuleGetter(this, "setTimeout",
                               "resource://gre/modules/Timer.jsm");
ChromeUtils.defineModuleGetter(this, "clearTimeout",
                               "resource://gre/modules/Timer.jsm");

Cu.importGlobalProperties(["fetch"]);

// This module obtains symbol tables for binaries.
// It does so with the help of a WASM module which gets pulled in from the
// internet on demand. We're doing this purely for the purposes of saving on
// code size. The contents of the WASM module are expected to be static, they
// are checked against the hash specified below.
// The WASM code is run on a ChromeWorker thread. It takes the raw byte
// contents of the to-be-dumped binary (and of an additional optional pdb file
// on Windows) as its input, and returns a set of typed arrays which make up
// the symbol table.

// Don't let the strange looking URLs and strings below scare you.
// The hash check ensures that the contents of the wasm module are what we
// expect them to be.
// The source code is at https://github.com/mstange/profiler-get-symbols/ .

// Generated from https://github.com/mstange/profiler-get-symbols/commit/0a0aadc68d6196823a5f102feacb2f04424cd681
const WASM_MODULE_URL =
  "https://zealous-rosalind-a98ce8.netlify.com/wasm/4af7553b4848038c5a1e33a15ec107094bd0572e16fe2a367235bcb50a630b148ac4fe1d165859d7a9bb4ca4e2572c0e.wasm";
const WASM_MODULE_INTEGRITY =
  "sha384-SvdVO0hIA4xaHjOhXsEHCUvQVy4W/io2cjW8tQpjCxSKxP4dFlhZ16m7TKTiVywO";

const EXPIRY_TIME_IN_MS = 5 * 60 * 1000; // 5 minutes

let gCachedWASMModulePromise = null;
let gCachedWASMModuleExpiryTimer = 0;

function clearCachedWASMModule() {
  gCachedWASMModulePromise = null;
  gCachedWASMModuleExpiryTimer = 0;
}

function getWASMProfilerGetSymbolsModule() {
  if (!gCachedWASMModulePromise) {
    gCachedWASMModulePromise = (async function() {
      const request = new Request(WASM_MODULE_URL, {
        integrity: WASM_MODULE_INTEGRITY,
        credentials: "omit",
      });
      return WebAssembly.compileStreaming(fetch(request));
    })();
  }

  // Reset expiry timer.
  clearTimeout(gCachedWASMModuleExpiryTimer);
  gCachedWASMModuleExpiryTimer = setTimeout(clearCachedWASMModule, EXPIRY_TIME_IN_MS);

  return gCachedWASMModulePromise;
}

this.ProfilerGetSymbols = {
  /**
   * Obtain symbols for the binary at the specified location.
   *
   * @param {string} binaryPath The absolute path to the binary on the local
   *   file system.
   * @param {string} debugPath The absolute path to the binary's pdb file on the
   *   local file system if on Windows, otherwise the same as binaryPath.
   * @param {string} breakpadId The breakpadId for the binary whose symbols
   *   should be obtained. This is used for two purposes: 1) to locate the
   *   correct single-arch binary in "FatArch" files, and 2) to make sure the
   *   binary at the given path is actually the one that we want. If no ID match
   *   is found, this function throws (rejects the promise).
   * @returns {Promise} The symbol table in SymbolTableAsTuple format, see the
   *   documentation for nsIProfiler.getSymbolTable.
   */
  async getSymbolTable(binaryPath, debugPath, breakpadId) {
    const module = await getWASMProfilerGetSymbolsModule();

    return new Promise((resolve, reject) => {
      const worker =
        new ChromeWorker("resource://gre/modules/ProfilerGetSymbols-worker.js");
      worker.onmessage = (e) => {
        if (e.data.error) {
          reject(e.data.error);
          return;
        }
        resolve(e.data.result);
      };
      worker.postMessage({binaryPath, debugPath, breakpadId, module});
    });
  },
};
