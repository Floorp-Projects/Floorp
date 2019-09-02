/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["ProfilerGetSymbols"];

ChromeUtils.defineModuleGetter(
  this,
  "setTimeout",
  "resource://gre/modules/Timer.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "clearTimeout",
  "resource://gre/modules/Timer.jsm"
);

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

// Generated from https://github.com/mstange/profiler-get-symbols/commit/bff0bc7b6dfbbc83b084d080dd977adea6d3f661
const WASM_MODULE_URL =
  "https://zealous-rosalind-a98ce8.netlify.com/wasm/3365b75b606d0fd60ca98eea6b3057ac5bdcd4be2f24d1ad1c6452ddf2cc622ddeae28665261d7e0f13913ed5e14960b.wasm";
const WASM_MODULE_INTEGRITY =
  "sha384-M2W3W2BtD9YMqY7qazBXrFvc1L4vJNGtHGRS3fLMYi3erihmUmHX4PE5E+1eFJYL";

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
  gCachedWASMModuleExpiryTimer = setTimeout(
    clearCachedWASMModule,
    EXPIRY_TIME_IN_MS
  );

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
      const worker = new ChromeWorker(
        "resource://gre/modules/ProfilerGetSymbols-worker.js"
      );
      worker.onmessage = e => {
        if (e.data.error) {
          reject(e.data.error);
          return;
        }
        resolve(e.data.result);
      };
      worker.postMessage({ binaryPath, debugPath, breakpadId, module });
    });
  },
};
