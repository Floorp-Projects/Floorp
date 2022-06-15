/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  openToolboxAndLog,
  closeToolboxAndLog,
  runTest,
  testSetup,
  testTeardown,
  PAGES_BASE_URL,
} = require("damp-test/tests/head");
const {
  reloadConsoleAndLog,
  waitForConsoleOutputChildListChange,
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

  dump("Clear console\n");
  const { hud } = toolbox.getPanel("webconsole");
  const onMessageCleared = waitForConsoleOutputChildListChange(
    hud,
    consoleOutput => consoleOutput.querySelector(".message-body") == null
  );
  hud.ui.clearOutput(true);
  await onMessageCleared;
  dump("Console cleared\n");

  const outOfOrderTest = runTest("custom.webconsole.out-of-order");
  // The evaluation query doesn't follow the same path as the message resources, which mean
  // we can get the result before the console messages are received in the store. When those
  // console messages are handled in the store, we need to insert them before the result
  // message, which can be a performance bottleneck.
  const TOTAL_MESSAGES = 10000;
  hud.ui.wrapper.dispatchEvaluateExpression(
    `for(let i =0;i<${TOTAL_MESSAGES};i++)console.info("expression log",i)`
  );

  await waitForConsoleOutputChildListChange(hud, consoleOutput => {
    const infoMessages = consoleOutput.querySelectorAll(
      ".message.info .message-body"
    );
    return infoMessages[infoMessages.length - 1].textContent.includes(
      "expression log " + (TOTAL_MESSAGES - 1)
    );
  });

  outOfOrderTest.done();

  await closeToolboxAndLog("custom.webconsole", toolbox);

  await testTeardown();
};
