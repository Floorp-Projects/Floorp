/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  openToolbox,
  closeToolbox,
  getBrowserWindow,
  logTestResult,
  runTest,
  testSetup,
  testTeardown,
  SIMPLE_URL,
} = require("damp-test/tests/head");
const {
  waitForConsoleOutputChildListChange,
} = require("damp-test/tests/webconsole/webconsole-helpers");

module.exports = async function () {
  let TOTAL_MESSAGES = 1000;
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
      const obj = {};
      for (let i = 0; i < 1000; i++) {
        obj["item-" + i] = {index: i, ...obj};
      }
      addMessageListener("do-logs", function () {
        const start = Cu.now();
        for (var i = 0; i < ${TOTAL_MESSAGES}; i++) {
          content.console.log('damp', i+1, content, obj);
        }
        sendAsyncMessage('logs-done',  Cu.now() - start);
      });
    }`
      ) +
      ")()",
    true
  );

  let test = runTest("console.bulklog");

  const allMessagesreceived = waitForConsoleOutputChildListChange(
    hud,
    consoleOutput => {
      const messages = Array.from(
        consoleOutput.querySelectorAll(".message-body")
      );
      return (
        messages.find(message => message.textContent.includes("damp 1")) &&
        messages.find(message =>
          message.textContent.includes("damp " + TOTAL_MESSAGES)
        )
      );
    }
  );

  // Kick off the logging
  const onContentProcessLogsDone = new Promise(resolve => {
    messageManager.addMessageListener("logs-done", function onLogsDone(msg) {
      messageManager.removeMessageListener("logs-done", onLogsDone);
      resolve(msg.data);
    });
  });

  messageManager.sendAsyncMessage("do-logs");
  const contentProcessConsoleAPIDuration = await onContentProcessLogsDone;
  logTestResult(
    "console.content-process-bulklog",
    contentProcessConsoleAPIDuration
  );

  await allMessagesreceived;
  // Wait for the console to redraw
  await new Promise(resolve =>
    getBrowserWindow().requestAnimationFrame(resolve)
  );

  test.done();

  await closeToolbox();
  await testTeardown();
};
