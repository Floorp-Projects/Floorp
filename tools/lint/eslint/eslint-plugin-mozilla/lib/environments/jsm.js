/**
 * @fileoverview Defines the environment for jsm files.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  globals: {
    // These globals are hard-coded and available in .jsm scopes.
    // https://searchfox.org/mozilla-central/rev/dcb0cfb66e4ed3b9c7fbef1e80572426ff5f3c3a/js/xpconnect/loader/mozJSModuleLoader.cpp#222-223
    // Although `debug` is allowed for jsm files, this is non-standard and something
    // we don't want to allow in mjs files. Hence it is not included here.
    atob: false,
    btoa: false,
    dump: false,
    // The WebAssembly global is available in most (if not all) contexts where
    // JS can run. It's definitely available in JSMs. So even if this is not
    // the perfect place to add it, it's not wrong, and we can move it later.
    WebAssembly: false,
    // These are hard-coded and available in .jsm scopes.
    // See BackstagePass::Resolve.
    fetch: false,
    crypto: false,
    indexedDB: false,
    structuredClone: false,
  },
};
