/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  openAccessibilityAndLog,
  shutdownAccessibilityService,
} = require("damp-test/tests/accessibility/accessibility-helpers");

const {
  closeToolbox,
  testSetup,
  testTeardown,
  SIMPLE_URL,
} = require("damp-test/tests/head");

module.exports = async function () {
  await testSetup(SIMPLE_URL);
  await openAccessibilityAndLog("cold");
  await closeToolbox();
  shutdownAccessibilityService();
  await testTeardown();
};
