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

module.exports = async function() {
  let TOTAL_MESSAGES = 10;
  let tab = await testSetup(SIMPLE_URL);
  let messageManager = tab.linkedBrowser.messageManager;
  let toolbox = await openToolbox("webconsole");
  let webconsole = toolbox.getPanel("webconsole");

  // Resolve once the last message has been received.
  let allMessagesReceived = new Promise(resolve => {
    function receiveMessages(messages) {
      for (let m of messages) {
        if (m.node.textContent.includes("damp " + TOTAL_MESSAGES)) {
          webconsole.hud.ui.off("new-messages", receiveMessages);
          // Wait for the console to redraw
          getBrowserWindow().requestAnimationFrame(resolve);
        }
      }
    }
    webconsole.hud.ui.on("new-messages", receiveMessages);
  });

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
  // Kick off the logging
  messageManager.sendAsyncMessage("do-logs");

  await allMessagesReceived;
  test.done();

  await closeToolbox();
  await testTeardown();
};
