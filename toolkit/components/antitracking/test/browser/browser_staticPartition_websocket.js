/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const FIRST_PARTY_A = "http://example.com";
const FIRST_PARTY_B = "http://example.org";
const THIRD_PARTY = "http://example.net";
const WS_ENDPOINT_HOST = "mochi.test:8888";

function getWSTestUrlForHost(host) {
  return (
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      `ws://${host}`
    ) + `file_ws_handshake_delay`
  );
}

function connect(browsingContext, host, protocol) {
  let url = getWSTestUrlForHost(host);
  info("Creating websocket with endpoint " + url);

  // Create websocket connection in third party iframe.
  let createPromise = SpecialPowers.spawn(
    browsingContext.children[0],
    [url, protocol],
    (url, protocol) => {
      let ws = new content.WebSocket(url, [protocol]);
      ws.addEventListener("error", () => {
        ws._testError = true;
      });
      if (!content.ws) {
        content.ws = {};
      }
      content.ws[protocol] = ws;
    }
  );

  let openPromise = createPromise.then(() =>
    SpecialPowers.spawn(
      browsingContext.children[0],
      [protocol],
      async protocol => {
        let ws = content.ws[protocol];
        if (ws.readyState != 0) {
          return !ws._testError;
        }
        // Still connecting.
        let result = await Promise.race([
          ContentTaskUtils.waitForEvent(ws, "open"),
          ContentTaskUtils.waitForEvent(ws, "error"),
        ]);
        return result.type != "error";
      }
    )
  );

  let result = { createPromise, openPromise };
  return result;
}

// Open 3 websockets which target the same ip/port combination, but have
// different principals. We send a protocol identifier to the server to signal
// how long the request should be delayed.
//
// When partitioning is disabled A blocks B and B blocks C. The timeline will
// look like this:
// A________
//          B____
//               C_
//
// When partitioning is enabled, connection handshakes for A and B will run
// (semi-) parallel since they have different origin attributes. B and C share
// origin attributes and therefore still run serially.
// A________
// B____
//      C_
//
// By observing the order of the handshakes we can ensure that the queue
// partitioning is working correctly.
async function runTest(partitioned) {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.partition.network_state", partitioned]],
  });

  let tabA = BrowserTestUtils.addTab(gBrowser, FIRST_PARTY_A);
  await BrowserTestUtils.browserLoaded(tabA.linkedBrowser);
  let tabB = BrowserTestUtils.addTab(gBrowser, FIRST_PARTY_B);
  await BrowserTestUtils.browserLoaded(tabB.linkedBrowser);

  for (let tab of [tabA, tabB]) {
    await SpecialPowers.spawn(tab.linkedBrowser, [THIRD_PARTY], async src => {
      let frame = content.document.createElement("iframe");
      frame.src = src;
      let loadPromise = ContentTaskUtils.waitForEvent(frame, "load");
      content.document.body.appendChild(frame);
      await loadPromise;
    });
  }

  // First ensure that we can open websocket connections to the test endpoint.
  let { openPromise, createPromise } = await connect(
    tabA.linkedBrowser.browsingContext,
    WS_ENDPOINT_HOST,
    false
  );
  await createPromise;
  let openPromiseResult = await openPromise;
  ok(openPromiseResult, "Websocket endpoint accepts connections.");

  let openedA;
  let openedB;
  let openedC;

  let { createPromise: createPromiseA, openPromise: openPromiseA } = connect(
    tabA.linkedBrowser.browsingContext,
    WS_ENDPOINT_HOST,
    "test-6"
  );

  openPromiseA = openPromiseA.then(opened => {
    openedA = opened;
    info("Completed WS connection A");
    if (partitioned) {
      ok(openedA, "Should have opened A");
      ok(openedB, "Should have opened B");
    } else {
      ok(openedA, "Should have opened A");
      ok(openedB == null, "B should be pending");
    }
  });
  await createPromiseA;

  // The frame of connection B is embedded in a different first party as A.
  let { createPromise: createPromiseB, openPromise: openPromiseB } = connect(
    tabB.linkedBrowser.browsingContext,
    WS_ENDPOINT_HOST,
    "test-3"
  );
  openPromiseB = openPromiseB.then(opened => {
    openedB = opened;
    info("Completed WS connection B");
    if (partitioned) {
      ok(openedA == null, "A should be pending");
      ok(openedB, "Should have opened B");
      ok(openedC == null, "C should be pending");
    } else {
      ok(openedA, "Should have opened A");
      ok(openedB, "Should have opened B");
      ok(openedC == null, "C should be pending");
    }
  });
  await createPromiseB;

  // The frame of connection C is embedded in the same first party as B.
  let { createPromise: createPromiseC, openPromise: openPromiseC } = connect(
    tabB.linkedBrowser.browsingContext,
    WS_ENDPOINT_HOST,
    "test-0"
  );
  openPromiseC = openPromiseC.then(opened => {
    openedC = opened;
    info("Completed WS connection C");
    if (partitioned) {
      ok(openedB, "Should have opened B");
      ok(openedC, "Should have opened C");
    } else {
      ok(opened, "Should have opened B");
      ok(opened, "Should have opened C");
    }
  });
  await createPromiseC;

  // Wait for all connections to complete before closing the tabs.
  await Promise.all([openPromiseA, openPromiseB, openPromiseC]);

  BrowserTestUtils.removeTab(tabA);
  BrowserTestUtils.removeTab(tabB);

  await SpecialPowers.popPrefEnv();
}

add_setup(async function () {
  // This test relies on a WS connection timeout > 6 seconds.
  await SpecialPowers.pushPrefEnv({
    set: [["network.websocket.timeout.open", 20]],
  });
});

add_task(async function test_non_partitioned() {
  await runTest(false);
});

add_task(async function test_partitioned() {
  await runTest(true);
});
