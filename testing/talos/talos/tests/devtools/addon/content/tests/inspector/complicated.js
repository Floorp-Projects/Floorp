/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  reloadInspectorAndLog,
} = require("damp-test/tests/inspector/inspector-helpers");
const {
  openToolboxAndLog,
  closeToolboxAndLog,
  testSetup,
  testTeardown,
  COMPLICATED_URL,
} = require("damp-test/tests/head");

module.exports = async function () {
  await testSetup(COMPLICATED_URL);

  let toolbox = await openToolboxAndLog("complicated.inspector", "inspector");
  await reloadInspectorAndLog("complicated", toolbox);
  await closeToolboxAndLog("complicated.inspector", toolbox);

  await testTeardown();
};
