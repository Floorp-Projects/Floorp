/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  openToolboxAndLog,
  closeToolboxAndLog,
  testSetup,
  testTeardown,
  PAGES_BASE_URL,
} = require("../head");
const { reloadConsoleAndLog } = require("./webconsole-helpers");

const TEST_URL = PAGES_BASE_URL + "custom/console/index.html";

module.exports = async function() {
  // These numbers controls the number of console api calls we do in the test
  const sync = 250,
    stream = 250,
    batch = 250;
  const params = `?sync=${sync}&stream=${stream}&batch=${batch}`;
  const url = TEST_URL + params;
  await testSetup(url, { disableCache: true });

  const toolbox = await openToolboxAndLog("custom.webconsole", "webconsole");
  await reloadConsoleAndLog("custom", toolbox, sync + stream + batch);
  await closeToolboxAndLog("custom.webconsole", toolbox);

  await testTeardown();
};
