/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  openToolboxAndLog,
  closeToolboxAndLog,
  testSetup,
  testTeardown,
  COMPLICATED_URL,
} = require("../head");
const { reloadConsoleAndLog } = require("./webconsole-helpers");

const EXPECTED_MESSAGES = 7;

module.exports = async function() {
  await testSetup(COMPLICATED_URL);

  let toolbox = await openToolboxAndLog("complicated.webconsole", "webconsole");
  await reloadConsoleAndLog("complicated", toolbox, EXPECTED_MESSAGES);
  await closeToolboxAndLog("complicated.webconsole", toolbox);

  await testTeardown();
};
