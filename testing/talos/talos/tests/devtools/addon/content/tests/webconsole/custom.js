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
} = require("damp-test/tests/head");
const {
  reloadConsoleAndLog,
} = require("damp-test/tests/webconsole/webconsole-helpers");

const TEST_URL = PAGES_BASE_URL + "custom/console/index.html";

module.exports = async function() {
  // These numbers controls the number of console api calls we do in the test
  const sync = 500,
    stream = 250,
    batch = 500,
    simple = 5000;

  const params = `?sync=${sync}&stream=${stream}&batch=${batch}&simple=${simple}`;
  const url = TEST_URL + params;
  await testSetup(url, { disableCache: true });

  const toolbox = await openToolboxAndLog("custom.webconsole", "webconsole");
  // With virtualization, we won't have all the messages rendered in the DOM, so we only
  // wait for the last message to be displayed ("simple log 4999").
  await reloadConsoleAndLog("custom", toolbox, [
    {
      text: "simple log " + (simple - 1),
    },
  ]);
  await closeToolboxAndLog("custom.webconsole", toolbox);

  await testTeardown();
};
