/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  openToolboxAndLog,
  reloadPageAndLog,
  closeToolboxAndLog,
  testSetup,
  testTeardown,
  PAGES_BASE_URL,
} = require("damp-test/tests/head");

const {
  exportHar,
  waitForNetworkRequests,
} = require("damp-test/tests/netmonitor/netmonitor-helpers");

// These numbers controls the number of requests we do in the test
const bigFileRequests = 20,
  postDataRequests = 20,
  xhrRequests = 50;
// These other numbers only state how many requests the test do,
// we have to keep them in sync with netmonitor.html static content
const expectedSyncCssRequests = 10,
  expectedSyncJSRequests = 10;
// There is 10 request for the html document and 10 for the js files
// But the css file is loaded only once, the others are cached
const expectedSyncIframeRequests = 2 * 10 + 1;
const expectedRequests =
  1 + // This is for the top level index.html document
  expectedSyncCssRequests +
  expectedSyncJSRequests +
  expectedSyncIframeRequests +
  bigFileRequests +
  postDataRequests +
  xhrRequests;

const CUSTOM_URL = PAGES_BASE_URL + "custom/netmonitor/index.html";

module.exports = async function () {
  const url =
    CUSTOM_URL +
    `?bigFileRequests=${bigFileRequests}&postDataRequests=${postDataRequests}&xhrRequests=${xhrRequests}`;
  let tab = await testSetup(url);
  let { messageManager } = tab.linkedBrowser;
  let onReady = new Promise(done => {
    messageManager.addMessageListener("ready", done);
  });
  messageManager.loadFrameScript(
    "data:,(" +
      encodeURIComponent(
        `function () {
      if (content.wrappedJSObject.isReady) {
        sendAsyncMessage("ready");
      } else {
        content.addEventListener("message", function () {
          sendAsyncMessage("ready");
        });
      }
    }`
      ) +
      ")()",
    true
  );

  // We wait for a custom "ready" event in order to ensure all the requests
  // done during document load are finished before opening the netmonitor.
  // Otherwise some still pending requests will be displayed on toolbox open
  // and the number of requests being displayed will be random and introduce noise
  // in custom.netmonitor.open
  dump("Waiting for document to be ready and have sent all its requests\n");
  await onReady;

  const toolbox = await openToolboxAndLog("custom.netmonitor", "netmonitor");

  // Waterfall will only work after an idle event. Its width is only set after idle.
  // Before that, it doesn't render.
  dump("Waiting for idle in order to ensure running reload with a waterfall\n");
  let window = toolbox.getCurrentPanel().panelWin;
  await new Promise(done => {
    window.requestIdleCallback(done);
  });

  const requestsDone = waitForNetworkRequests(
    "custom.netmonitor",
    toolbox,
    expectedRequests,
    expectedRequests
  );
  await reloadPageAndLog("custom.netmonitor", toolbox);
  await requestsDone;

  await exportHar("custom.netmonitor", toolbox);

  await closeToolboxAndLog("custom.netmonitor", toolbox);

  // Bug 1503822, wait one second on test end to prevent a crash during firefox shutdown.
  await new Promise(r => setTimeout(r, 1000));

  await testTeardown();
};
