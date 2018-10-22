/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { closeToolboxAndLog, testSetup, testTeardown, COMPLICATED_URL } = require("../head");
const { openDebuggerAndLog, reloadDebuggerAndLog } = require("./debugger-helpers");

const EXPECTED = {
  sources: 14,
  file: "ga.js",
  sourceURL: COMPLICATED_URL,
  text: "Math;function ga(a,b){return a.name=b}",
};

module.exports = async function() {
  await testSetup(COMPLICATED_URL);

  let toolbox = await openDebuggerAndLog("complicated", EXPECTED);
  await reloadDebuggerAndLog("complicated", toolbox, EXPECTED);
  await closeToolboxAndLog("complicated.jsdebugger", toolbox);

  await testTeardown();
};
