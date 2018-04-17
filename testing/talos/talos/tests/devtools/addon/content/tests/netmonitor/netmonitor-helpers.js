/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { EVENTS } = require("devtools/client/netmonitor/src/constants");
const { getToolbox, runTest } = require("../head");

/**
 * Start monitoring all incoming update events about network requests and wait until
 * a complete info about all requests is received. (We wait for the timings info
 * explicitly, because that's always the last piece of information that is received.)
 *
 * This method is designed to wait for network requests that are issued during a page
 * load, when retrieving page resources (scripts, styles, images). It has certain
 * assumptions that can make it unsuitable for other types of network communication:
 * - it waits for at least one network request to start and finish before returning
 * - it waits only for request that were issued after it was called. Requests that are
 *   already in mid-flight will be ignored.
 * - the request start and end times are overlapping. If a new request starts a moment
 *   after the previous one was finished, the wait will be ended in the "interim"
 *   period.
 * @returns a promise that resolves when the wait is done.
 */
function waitForAllRequestsFinished(expectedRequests) {
  let toolbox = getToolbox();
  let window = toolbox.getCurrentPanel().panelWin;

  return new Promise(resolve => {
    // Explicitly waiting for specific number of requests arrived
    let payloadReady = 0;
    let timingsUpdated = 0;

    function onPayloadReady(_, id) {
      payloadReady++;
      maybeResolve();
    }

    function onTimingsUpdated(_, id) {
      timingsUpdated++;
      maybeResolve();
    }

    function maybeResolve() {
      // Have all the requests finished yet?
      if (payloadReady >= expectedRequests && timingsUpdated >= expectedRequests) {
        // All requests are done - unsubscribe from events and resolve!
        window.api.off(EVENTS.PAYLOAD_READY, onPayloadReady);
        window.api.off(EVENTS.RECEIVED_EVENT_TIMINGS, onTimingsUpdated);
        resolve();
      }
    }

    window.api.on(EVENTS.PAYLOAD_READY, onPayloadReady);
    window.api.on(EVENTS.RECEIVED_EVENT_TIMINGS, onTimingsUpdated);
  });
}

exports.waitForNetworkRequests = async function(label, toolbox, expectedRequests) {
  let test = runTest(label + ".requestsFinished.DAMP");
  await waitForAllRequestsFinished(expectedRequests);
  test.done();
};

exports.exportHar = async function(label, toolbox) {
  let test = runTest(label + ".exportHar");

  // Export HAR from the Network panel.
  await toolbox.getHARFromNetMonitor();

  test.done();
};
