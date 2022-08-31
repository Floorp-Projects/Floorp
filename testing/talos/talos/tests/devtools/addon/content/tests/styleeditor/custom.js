/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  openToolboxAndLog,
  closeToolboxAndLog,
  reloadPageAndLog,
  testSetup,
  testTeardown,
  PAGES_BASE_URL,
} = require("damp-test/tests/head");

const TEST_URL = PAGES_BASE_URL + "custom/styleeditor/index.html";

module.exports = async function() {
  await testSetup(TEST_URL);
  const toolbox = await openToolboxAndLog("custom.styleeditor", "styleeditor");
  await reloadPageAndLog("custom.styleeditor", toolbox);

  await closeToolboxAndLog("custom.styleeditor", toolbox);
  await testTeardown();
};
