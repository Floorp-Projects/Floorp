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

const global = this;

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

// Generated from https://github.com/mstange/profiler-get-symbols/commit/90ee39f1d18d2727f07dc57bd93cff6bc73ce8a0
const WASM_MODULE_URL =
  "https://zealous-rosalind-a98ce8.netlify.com/wasm/8f7ca2f70e1cd21b5a2dbe96545672752887bfbd4e7b3b9437e9fc7c3da0a3bedae4584ff734f0c9f08c642e6b66ffab.wasm";
const WASM_MODULE_INTEGRITY =
  "sha384-j3yi9w4c0htaLb6WVFZydSiHv71OezuUN+n8fD2go77a5FhP9zTwyfCMZC5rZv+r";

const EXPIRY_TIME_IN_MS = 5 * 60 * 1000; // 5 minutes

let gCachedWASMModulePromise = null;
let gCachedWASMModuleExpiryTimer = 0;

// Keep active workers alive (see bug 1592227).
let gActiveWorkers = new Set();

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
      gActiveWorkers.add(worker);

      worker.onmessage = msg => {
        gActiveWorkers.delete(worker);
        if (msg.data.error) {
          const error = msg.data.error;
          if (error.name) {
            // Turn the JSON error object into a real Error object.
            const { name, message, fileName, lineNumber } = error;
            const ErrorObjConstructor =
              name in global && Error.isPrototypeOf(global[name])
                ? global[name]
                : Error;
            const e = new ErrorObjConstructor(message, fileName, lineNumber);
            e.name = name;
            reject(e);
          } else {
            reject(error);
          }
          return;
        }
        resolve(msg.data.result);
      };

      // Handle uncaught errors from the worker script. onerror is called if
      // there's a syntax error in the worker script, for example, or when an
      // unhandled exception is thrown, but not for unhandled promise
      // rejections. Without this handler, mistakes during development such as
      // syntax errors can be hard to track down.
      worker.onerror = errorEvent => {
        gActiveWorkers.delete(worker);
        worker.terminate();
        const { message, filename, lineno } = errorEvent;
        const error = new Error(message, filename, lineno);
        error.name = "WorkerError";
        reject(error);
      };

      // Handle errors from messages that cannot be deserialized. I'm not sure
      // how to get into such a state, but having this handler seems like a good
      // idea.
      worker.onmessageerror = errorEvent => {
        gActiveWorkers.delete(worker);
        worker.terminate();
        const { message, filename, lineno } = errorEvent;
        const error = new Error(message, filename, lineno);
        error.name = "WorkerMessageError";
        reject(error);
      };

      worker.postMessage({ binaryPath, debugPath, breakpadId, module });
    });
  },
};
