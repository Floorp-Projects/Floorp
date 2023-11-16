/**
 * @fileoverview Defines the environment for sjs files.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  globals: {
    // All these variables are hard-coded to be available for sjs scopes only.
    // https://searchfox.org/mozilla-central/rev/26a1b0fce12e6dd495a954c542bb1e7bd6e0d548/netwerk/test/httpserver/httpd.js#2879
    atob: false,
    btoa: false,
    Cc: false,
    ChromeUtils: false,
    Ci: false,
    Components: false,
    Cr: false,
    Cu: false,
    dump: false,
    IOUtils: false,
    PathUtils: false,
    TextDecoder: false,
    TextEncoder: false,
    URLSearchParams: false,
    URL: false,
    getState: false,
    setState: false,
    getSharedState: false,
    setSharedState: false,
    getObjectState: false,
    setObjectState: false,
    registerPathHandler: false,
    Services: false,
    // importScripts is also available.
    importScripts: false,
  },
};
