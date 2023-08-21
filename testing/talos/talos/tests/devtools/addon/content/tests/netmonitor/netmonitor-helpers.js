/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { EVENTS } = require("devtools/client/netmonitor/src/constants");
const Actions = require("devtools/client/netmonitor/src/actions/index");
const {
  getToolbox,
  runTest,
  waitForDOMElement,
} = require("damp-test/tests/head");

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
 *
 * We might need to allow a range of requests because even though we run with
 * cache disabled, different loads can still be coalesced, and whether they're
 * coalesced or not depends on timing.
 *
 * @returns a promise that resolves when the wait is done.
 */
async function waitForAllRequestsFinished(
  minExpectedRequests,
  maxExpectedRequests
) {
  let toolbox = getToolbox();
  let window = toolbox.getCurrentPanel().panelWin;

  return new Promise(resolve => {
    // Explicitly waiting for specific number of requests arrived
    let payloadReady = 0;
    let resolveWithLessThanMaxRequestsTimer = null;

    function onPayloadReady(_, id) {
      payloadReady++;
      dump(`Waiting for ${maxExpectedRequests - payloadReady} requests\n`);
      maybeResolve();
    }

    function doResolve() {
      // All requests are done - unsubscribe from events and resolve!
      window.api.off(EVENTS.PAYLOAD_READY, onPayloadReady);
      // Resolve after current frame
      setTimeout(resolve, 1);
    }

    function maybeResolve() {
      if (resolveWithLessThanMaxRequestsTimer) {
        clearTimeout(resolveWithLessThanMaxRequestsTimer);
        resolveWithLessThanMaxRequestsTimer = null;
      }

      // Have all the requests finished yet?
      if (payloadReady >= maxExpectedRequests) {
        doResolve();
        return;
      }

      // If we're past the minimum threshold, wait to see if more requests come
      // up, but resolve otherwise.
      if (payloadReady >= minExpectedRequests) {
        resolveWithLessThanMaxRequestsTimer = setTimeout(doResolve, 1000);
      }
    }

    window.api.on(EVENTS.PAYLOAD_READY, onPayloadReady);
  });
}

function waitForLoad(iframe) {
  return new Promise(resolve => iframe.addEventListener("load", resolve));
}

function clickElement(el, win) {
  const clickEvent = new win.MouseEvent("click", {
    bubbles: true,
    cancelable: true,
    view: win,
  });
  el.dispatchEvent(clickEvent);
}

function mouseDownElement(el, win) {
  const mouseEvent = new win.MouseEvent("mousedown", {
    bubbles: true,
    cancelable: true,
    view: win,
  });
  el.dispatchEvent(mouseEvent);
}

exports.waitForNetworkRequests = async function (
  label,
  toolbox,
  minExpectedRequests,
  maxExpectedRequests = minExpectedRequests
) {
  let test = runTest(label + ".requestsFinished.DAMP");
  await waitForAllRequestsFinished(minExpectedRequests, maxExpectedRequests);
  test.done();
};

exports.exportHar = async function (label, toolbox) {
  let test = runTest(label + ".exportHar");

  // Export HAR from the Network panel.
  await toolbox.getHARFromNetMonitor();

  test.done();
};

exports.openResponseDetailsPanel = async function (label, toolbox) {
  const win = toolbox.getCurrentPanel().panelWin;
  const { document, store } = win;
  const monitor = document.querySelector(".monitor-panel");

  // html test
  const testHtml = runTest(label + ".responsePanel.html");

  store.dispatch(Actions.batchEnable(false));

  const waitForDetailsBar = waitForDOMElement(monitor, ".network-details-bar");
  store.dispatch(Actions.toggleNetworkDetails());
  await waitForDetailsBar;

  const sideBar = document.querySelector(".network-details-bar");
  const iframeSelector = "#response-panel .html-preview iframe";
  const waitForIframe = waitForDOMElement(sideBar, iframeSelector);

  clickElement(document.querySelector("#response-tab"), win);

  await waitForIframe;
  await waitForLoad(document.querySelector(iframeSelector));

  testHtml.done();

  // close the sidebar
  store.dispatch(Actions.toggleNetworkDetails());

  // Sort the request list on the size column (in descending order)
  // to make sure we use the largest response.
  const sizeColumnHeader = document.querySelector(
    "#requests-list-contentSize-button"
  );
  const waitForDesc = waitForDOMElement(
    sizeColumnHeader.parentNode,
    "#requests-list-contentSize-button[data-sorted='descending']"
  );
  // Click the size header twice to make sure the requests
  // are sorted in descending order.
  clickElement(sizeColumnHeader, win);
  clickElement(sizeColumnHeader, win);
  await waitForDesc;

  // editor test
  const testEditor = runTest(label + ".responsePanel.editor");
  const request = document.querySelectorAll(".request-list-item")[0];
  const waitForEditor = waitForDOMElement(
    monitor,
    "#response-panel .CodeMirror.cm-s-mozilla"
  );
  mouseDownElement(request, win);
  await waitForEditor;

  testEditor.done();
  store.dispatch(Actions.toggleNetworkDetails());
};
