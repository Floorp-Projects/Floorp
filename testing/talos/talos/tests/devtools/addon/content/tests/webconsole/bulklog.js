/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  openToolbox,
  closeToolbox,
  getBrowserWindow,
  runTest,
  testSetup,
  testTeardown,
  SIMPLE_URL,
} = require("../head");
const { waitForConsoleOutputChildListChange } = require("./webconsole-helpers");

module.exports = async function() {
  let TOTAL_MESSAGES = 10;
  let tab = await testSetup(SIMPLE_URL);
  let messageManager = tab.linkedBrowser.messageManager;
  let toolbox = await openToolbox("webconsole");
  let { hud } = toolbox.getPanel("webconsole");

  // Load a frame script using a data URI so we can do logs
  // from the page.  So this is running in content.
  messageManager.loadFrameScript(
    "data:,(" +
      encodeURIComponent(
        `function () {
      addMessageListener("do-logs", function () {
        for (var i = 0; i < ${TOTAL_MESSAGES}; i++) {
          content.console.log('damp', i+1, content);
        }
      });
    }`
      ) +
      ")()",
    true
  );

  let test = runTest("console.bulklog");

  const allMessagesreceived = waitForConsoleOutputChildListChange(
    hud,
    consoleOutput =>
      consoleOutput.textContent.includes("damp " + TOTAL_MESSAGES)
  );

  // Kick off the logging
  messageManager.sendAsyncMessage("do-logs");

  await allMessagesreceived;
  // Wait for the console to redraw
  await new Promise(resolve =>
    getBrowserWindow().requestAnimationFrame(resolve)
  );

  test.done();

  await closeToolbox();
  await testTeardown();
};
