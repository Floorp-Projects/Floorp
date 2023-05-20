/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  openAccessibilityAndLog,
  reloadAccessibilityAndLog,
  shutdownAccessibilityService,
} = require("damp-test/tests/accessibility/accessibility-helpers");
const {
  closeToolboxAndLog,
  testSetup,
  testTeardown,
  SIMPLE_URL,
} = require("damp-test/tests/head");

module.exports = async function () {
  await testSetup(SIMPLE_URL);

  const toolbox = await openAccessibilityAndLog("simple");
  await reloadAccessibilityAndLog("simple", toolbox);
  await closeToolboxAndLog("simple.accessibility", toolbox);
  shutdownAccessibilityService();
  await testTeardown();
};
