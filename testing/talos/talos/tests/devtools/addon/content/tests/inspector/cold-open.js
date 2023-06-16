/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  openToolboxAndLog,
  closeToolbox,
  testSetup,
  testTeardown,
  SIMPLE_URL,
} = require("damp-test/tests/head");

// This simple test is only called once using the flag coldRun
module.exports = async function () {
  await testSetup(SIMPLE_URL);
  await openToolboxAndLog("cold.inspector", "inspector");
  await closeToolbox();
  await testTeardown();
};
