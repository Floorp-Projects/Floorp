/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { openToolbox, closeToolboxAndLog, getBrowserWindow, runTest, testSetup,
        testTeardown, SIMPLE_URL } = require("chrome://damp/content/tests/head");

module.exports = async function() {
  let tab = await testSetup(SIMPLE_URL);

  let messageManager = tab.linkedBrowser.messageManager;
  let toolbox = await openToolbox("webconsole");
  let webconsole = toolbox.getPanel("webconsole");

  // Resolve once the first message is received.
  let onMessageReceived = new Promise(resolve => {
    function receiveMessages(messages) {
      for (let m of messages) {
        resolve(m);
      }
    }
    webconsole.hud.ui.once("new-messages", receiveMessages);
  });

  // Load a frame script using a data URI so we can do logs
  // from the page.
  messageManager.loadFrameScript("data:,(" + encodeURIComponent(
    `function () {
      addMessageListener("do-dir", function () {
        content.console.dir(Array.from({length:1000}).reduce((res, _, i)=> {
          res["item_" + i] = i;
          return res;
        }, {}));
      });
    }`
  ) + ")()", true);

  let test = runTest("console.objectexpand");
  // Kick off the logging
  messageManager.sendAsyncMessage("do-dir");

  await onMessageReceived;
  const tree = webconsole.hud.ui.outputNode.querySelector(".dir.message .tree");

  // The tree can be collapsed since the properties are fetched asynchronously.
  if (tree.querySelectorAll(".node").length === 1) {
    // If this is the case, we wait for the properties to be fetched and displayed.
    await new Promise(resolve => {
      const observer = new (getBrowserWindow().MutationObserver)(mutations => {
        resolve(mutations);
        observer.disconnect();
      });
      observer.observe(tree, {
        childList: true
      });
    });
  }

  test.done();

  await closeToolboxAndLog("console.objectexpanded", toolbox);

  await testTeardown();
};


