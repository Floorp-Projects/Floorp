/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  openToolboxAndLog,
  closeToolboxAndLog,
  testSetup,
  testTeardown,
  SIMPLE_URL,
} = require("damp-test/tests/head");
const {
  reloadConsoleAndLog,
} = require("damp-test/tests/webconsole/webconsole-helpers");

const EXPECTED_MESSAGES = 1;

module.exports = async function () {
  await testSetup(SIMPLE_URL);

  let toolbox = await openToolboxAndLog("simple.webconsole", "webconsole");
  await reloadConsoleAndLog("simple", toolbox, EXPECTED_MESSAGES);
  await closeToolboxAndLog("simple.webconsole", toolbox);

  await testTeardown();
};
