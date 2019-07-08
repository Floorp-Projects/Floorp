/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  openToolbox,
  closeToolbox,
  logTestResult,
  testSetup,
  testTeardown,
  SIMPLE_URL,
} = require("../head");

// Log a stream of console messages, 1 per rAF.  Then record the average
// time per rAF.  The idea is that the console being slow can slow down
// content (i.e. Bug 1237368).
module.exports = async function() {
  let TOTAL_MESSAGES = 100;
  let tab = await testSetup(SIMPLE_URL);
  let messageManager = tab.linkedBrowser.messageManager;

  await openToolbox("webconsole");

  // Load a frame script using a data URI so we can do logs
  // from the page.  So this is running in content.
  messageManager.loadFrameScript(
    "data:,(" +
      encodeURIComponent(
        `function () {
      let count = 0;
      let startTime = content.performance.now();
      function log() {
        if (++count < ${TOTAL_MESSAGES}) {
          content.document.querySelector("h1").textContent += count + "\\n";
          content.console.log('damp', count,
                              content,
                              content.document,
                              content.document.body,
                              content.document.documentElement,
                              new Array(100).join(" DAMP? DAMP! "));
          content.requestAnimationFrame(log);
        } else {
          let avgTime = (content.performance.now() - startTime) / ${TOTAL_MESSAGES};
          sendSyncMessage("done", Math.round(avgTime));
        }
      }
      log();
    }`
      ) +
      ")()",
    true
  );

  let avgTime = await new Promise(resolve => {
    messageManager.addMessageListener("done", e => {
      resolve(e.data);
    });
  });

  logTestResult("console.streamlog", avgTime);

  await closeToolbox();
  await testTeardown();
};
