/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  openToolbox,
  closeToolbox,
  runTest,
  testSetup,
  testTeardown,
  SIMPLE_URL,
} = require("damp-test/tests/head");

/**
 * Measure the time necessary to perform successive childList mutations in the content
 * page and update the markup-view accordingly.
 */
module.exports = async function () {
  let tab = await testSetup(SIMPLE_URL);
  let messageManager = tab.linkedBrowser.messageManager;

  let toolbox = await openToolbox("inspector");
  let inspector = toolbox.getPanel("inspector");

  // Test with n=LIMIT mutations, with t=DELAY ms between each one.
  const LIMIT = 100;
  const DELAY = 5;

  messageManager.loadFrameScript(
    "data:,(" +
      encodeURIComponent(
        `function () {
      const LIMIT = ${LIMIT};
      addMessageListener("start-mutations-test", function () {
        let addElement = function(index) {
          if (index == LIMIT) {
            // LIMIT was reached, stop adding elements.
            return;
          }
          let div = content.document.createElement("div");
          content.document.body.appendChild(div);
          content.setTimeout(() => addElement(index + 1), ${DELAY});
        };
        addElement(0);
      });
    }`
      ) +
      ")()",
    false
  );

  let test = runTest("inspector.mutations");

  await new Promise(resolve => {
    let childListMutationsCounter = 0;
    inspector.on("markupmutation", mutations => {
      let childListMutations = mutations.filter(m => m.type === "childList");
      childListMutationsCounter += childListMutations.length;
      if (childListMutationsCounter === LIMIT) {
        // Wait until we received exactly n=LIMIT mutations in the markup view.
        resolve();
      }
    });

    messageManager.sendAsyncMessage("start-mutations-test");
  });

  test.done();
  await closeToolbox();

  await testTeardown();
};
