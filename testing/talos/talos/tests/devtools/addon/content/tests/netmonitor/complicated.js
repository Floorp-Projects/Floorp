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
  COMPLICATED_URL,
} = require("damp-test/tests/head");
const {
  exportHar,
  waitForNetworkRequests,
  openResponseDetailsPanel,
} = require("damp-test/tests/netmonitor/netmonitor-helpers");

const EXPECTED_REQUESTS = {
  min: 230,
  max: 280,
};

module.exports = async function () {
  await testSetup(COMPLICATED_URL);
  const toolbox = await openToolboxAndLog(
    "complicated.netmonitor",
    "netmonitor"
  );

  const requestsDone = waitForNetworkRequests(
    "complicated.netmonitor",
    toolbox,
    EXPECTED_REQUESTS.min,
    EXPECTED_REQUESTS.max
  );
  await reloadPageAndLog("complicated.netmonitor", toolbox);
  await requestsDone;

  await exportHar("complicated.netmonitor", toolbox);

  await openResponseDetailsPanel("complicated.netmonitor", toolbox);

  await closeToolboxAndLog("complicated.netmonitor", toolbox);

  // Bug 1503822, wait one second on test end to prevent a crash during firefox shutdown.
  await new Promise(r => setTimeout(r, 1000));

  await testTeardown();
};
