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
  // This is the number of iframes created in the test. Each iframe will create
  // a number of console messages equal to the sum of the numbers below. The
  // first iframe will use the same domain as the parent document. The remaining
  // iframes will use unique domains.
  const domains = 2;

  // These numbers controls the number of console api calls we do in the test
  const sync = 250,
    stream = 250,
    batch = 250,
    simple = 5000;

  const params = `?domains=${domains}&sync=${sync}&stream=${stream}&batch=${batch}&simple=${simple}`;
  const url = TEST_URL + params;
  await testSetup(url, { disableCache: true });

  const toolbox = await openToolboxAndLog("custom.webconsole", "webconsole");
  await reloadConsoleAndLog("custom", toolbox, sync + stream + batch + simple);
  await closeToolboxAndLog("custom.webconsole", toolbox);

  await testTeardown();
};
