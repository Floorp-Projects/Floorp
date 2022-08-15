/**
 * @fileoverview Defines the environment for xpcshell test files.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

var { getScriptGlobals } = require("./utils");

const extraGlobals = [
  // Defined in XPCShellImpl.cpp
  "print",
  "readline",
  "load",
  "quit",
  "dumpXPC",
  "dump",
  "gc",
  "gczeal",
  "options",
  "sendCommand",
  "atob",
  "btoa",
  "setInterruptCallback",
  "simulateNoScriptActivity",
  "registerXPCTestComponents",

  // Assert.jsm globals.
  "setReporter",
  "report",
  "ok",
  "equal",
  "notEqual",
  "deepEqual",
  "notDeepEqual",
  "strictEqual",
  "notStrictEqual",
  "throws",
  "rejects",
  "greater",
  "greaterOrEqual",
  "less",
  "lessOrEqual",
  // TestingFunctions.cpp globals
  "allocationMarker",
  "byteSize",
  "saveStack",
];

module.exports = getScriptGlobals(
  "xpcshell",
  ["testing/xpcshell/head.js"],
  extraGlobals.map(g => {
    return { name: g, writable: false };
  })
);
