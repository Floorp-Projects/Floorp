/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  closeToolbox,
  testSetup,
  testTeardown,
  SIMPLE_URL,
} = require("../head");
const { openDebuggerAndLog } = require("./debugger-helpers");

const EXPECTED = {
  sources: 1,
  file: "simple.html",
  sourceURL: SIMPLE_URL,
  text: "This is a simple page",
};

module.exports = async function() {
  await testSetup(SIMPLE_URL);
  await openDebuggerAndLog("cold", EXPECTED);
  await closeToolbox();
  await testTeardown();
};
