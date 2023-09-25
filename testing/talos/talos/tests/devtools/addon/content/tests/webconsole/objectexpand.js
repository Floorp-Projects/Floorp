/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  openToolbox,
  closeToolboxAndLog,
  runTest,
  testSetup,
  testTeardown,
  SIMPLE_URL,
  waitForDOMPredicate,
} = require("damp-test/tests/head");

module.exports = async function () {
  let tab = await testSetup(SIMPLE_URL);

  let messageManager = tab.linkedBrowser.messageManager;
  let toolbox = await openToolbox("webconsole");
  let webconsole = toolbox.getPanel("webconsole");
  const WARMUP_INFO_COUNT = 1000;

  // Load a frame script using a data URI so we can do logs
  // from the page.
  messageManager.loadFrameScript(
    "data:,(" +
      encodeURIComponent(
        `function() {
          const obj = Array.from({
            length: 333
          }).reduce((res, _, i) => {
            res["item_" + i] = "alphanum-indexed-" + i;
            res[i] = "num-indexed-" + i;
            res[Symbol(i)] = "symbol-indexed-" + i;
            return res;
          }, {});

          addMessageListener("do-dir", function() {
            content.console.dir(obj);
          });

          addMessageListener("clear-and-do-info", function() {
              // clear the output first so we don't have previous messages.
              content.console.clear();
              content.console.info(...new Array(${WARMUP_INFO_COUNT}).fill(obj));
          });
        }`
      ) +
      ")()",
    true
  );

  // Expand an object when there's a single objectInspector instance in the console
  const objectExpandTest = runTest("console.objectexpand");
  await logAndWaitForExpandedObjectDirMessage(
    webconsole,
    messageManager,
    WARMUP_INFO_COUNT + 1
  );
  objectExpandTest.done();

  // Expand an object when there are many objectInspector instances in the console (See Bug 1599317)
  // First print a lot of objects and wait for them to be rendered
  const waitForInfoMessage = async () => {
    const infoMessage =
      webconsole.hud.ui.outputNode.querySelector(".info.message");
    if (
      infoMessage &&
      infoMessage.querySelectorAll(".tree").length === WARMUP_INFO_COUNT
    ) {
      return infoMessage;
    }
    await new Promise(res => setTimeout(res, 50));
    return waitForInfoMessage();
  };
  messageManager.sendAsyncMessage("clear-and-do-info");
  await waitForInfoMessage();

  const objectExpandWhenManyInstancesTest = runTest(
    "console.objectexpand-many-instances"
  );
  await logAndWaitForExpandedObjectDirMessage(
    webconsole,
    messageManager,
    WARMUP_INFO_COUNT + 1
  );
  objectExpandWhenManyInstancesTest.done();

  await closeToolboxAndLog("console.objectexpanded", toolbox);

  await testTeardown();
};

async function logAndWaitForExpandedObjectDirMessage(
  webconsole,
  messageManager,
  expectedTreeItemCount
) {
  const waitForDirMessage = async () => {
    const dirMessage =
      webconsole.hud.ui.outputNode.querySelector(".dir.message");
    if (dirMessage) {
      return dirMessage;
    }
    await new Promise(res => setTimeout(res, 50));
    return waitForDirMessage();
  };

  // Kick off the logging
  messageManager.sendAsyncMessage("do-dir");
  const message = await waitForDirMessage();
  const tree = message.querySelector(".tree");

  // The tree can be collapsed since the properties are fetched asynchronously.
  if (tree.querySelectorAll(".node").length === 1) {
    // If this is the case, we wait for the properties to be fetched and displayed.
    await waitForDOMPredicate(
      tree,
      () => tree.childElementCount === expectedTreeItemCount,
      { childList: true }
    );
  }
}
