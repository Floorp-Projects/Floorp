/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { EVENTS } = require("devtools/client/netmonitor/src/constants");
const { openToolbox, closeToolbox, reloadPageAndLog, testSetup,
        testTeardown, PAGES_BASE_URL } = require("../head");

module.exports = async function() {
  await testSetup(PAGES_BASE_URL + "custom/panels-in-background/index.html");

  // Make sure the Console and Network panels are initialized
  let toolbox = await openToolbox("webconsole");
  let monitor = await toolbox.selectTool("netmonitor");

  // Select the options panel to make both the Console and Network
  // panel be in background.
  // Options panel should not do anything on page reload.
  await toolbox.selectTool("options");

  // Reload the page and wait for all HTTP requests
  // to finish (1 doc + 600 XHRs).
  let payloadReady = waitForPayload(601, monitor.panelWin);
  await reloadPageAndLog("panelsInBackground", toolbox);
  await payloadReady;

  await closeToolbox();
  await testTeardown();
};

function waitForPayload(count, panelWin) {
  return new Promise(resolve => {
    let payloadReady = 0;

    function onPayloadReady(_, id) {
      payloadReady++;
      maybeResolve();
    }

    function maybeResolve() {
      if (payloadReady >= count) {
        panelWin.api.off(EVENTS.PAYLOAD_READY, onPayloadReady);
        resolve();
      }
    }

    panelWin.api.on(EVENTS.PAYLOAD_READY, onPayloadReady);
  });
}
