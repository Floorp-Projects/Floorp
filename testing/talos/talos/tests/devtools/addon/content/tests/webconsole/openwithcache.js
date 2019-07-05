/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  openToolboxAndLog,
  closeToolbox,
  testSetup,
  testTeardown,
  SIMPLE_URL,
} = require("../head");

const TOTAL_MESSAGES = 100;

module.exports = async function() {
  let tab = await testSetup(SIMPLE_URL);

  // Load a frame script using a data URI so we can do logs
  // from the page.  So this is running in content.
  tab.linkedBrowser.messageManager.loadFrameScript(
    "data:,(" +
      encodeURIComponent(`
    function () {
      for (var i = 0; i < ${TOTAL_MESSAGES}; i++) {
        content.console.log('damp', i+1, content);
      }
    }`) +
      ")()",
    true
  );

  await openToolboxAndLog("console.openwithcache", "webconsole");
  await closeToolbox();

  await testTeardown();
};
