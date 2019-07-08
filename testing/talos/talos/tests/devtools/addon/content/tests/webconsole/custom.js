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

module.exports = async function() {
  // These numbers controls the number of console api calls we do in the test
  let sync = 250,
    stream = 250,
    async = 250;
  let params = `?sync=${sync}&stream=${stream}&async=${async}`;
  let url = PAGES_BASE_URL + "custom/console/index.html" + params;

  await testSetup(url);

  let toolbox = await openToolboxAndLog("custom.webconsole", "webconsole");
  await reloadConsoleAndLog("custom", toolbox, sync + stream + async);
  await closeToolboxAndLog("custom.webconsole", toolbox);

  await testTeardown();
};
