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
  SIMPLE_URL,
} = require("damp-test/tests/head");
const {
  exportHar,
  waitForNetworkRequests,
} = require("damp-test/tests/netmonitor/netmonitor-helpers");

const EXPECTED_REQUESTS = 1;

module.exports = async function () {
  await testSetup(SIMPLE_URL);
  const toolbox = await openToolboxAndLog("simple.netmonitor", "netmonitor");

  const requestsDone = waitForNetworkRequests(
    "simple.netmonitor",
    toolbox,
    EXPECTED_REQUESTS
  );
  await reloadPageAndLog("simple.netmonitor", toolbox);
  await requestsDone;

  await exportHar("simple.netmonitor", toolbox);

  await closeToolboxAndLog("simple.netmonitor", toolbox);
  await testTeardown();
};
