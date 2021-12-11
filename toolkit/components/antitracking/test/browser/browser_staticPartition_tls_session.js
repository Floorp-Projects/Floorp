/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

/**
 * Tests that we correctly partition TLS sessions by inspecting the
 * socket's peerId. The peerId contains the OriginAttributes suffix which
 * includes the partitionKey.
 */

const TEST_ORIGIN_A = "https://example.com";
const TEST_ORIGIN_B = "https://example.org";
const TEST_ORIGIN_C = "https://w3c-test.org";

const TEST_ENDPOINT =
  "/browser/toolkit/components/antitracking/test/browser/empty.js";

const TEST_URL_C = TEST_ORIGIN_C + TEST_ENDPOINT;

/**
 * Waits for a load with the given URL to happen and returns the peerId.
 * @param {string} url - The URL expected to load.
 * @returns {Promise<string>} a promise which resolves on load with the
 * associated socket peerId.
 */
async function waitForLoad(url) {
  return new Promise(resolve => {
    const TOPIC = "http-on-examine-response";

    function observer(subject, topic, data) {
      if (topic != TOPIC) {
        return;
      }
      subject.QueryInterface(Ci.nsIChannel);
      if (subject.URI.spec != url) {
        return;
      }

      Services.obs.removeObserver(observer, TOPIC);

      resolve(
        subject.securityInfo.QueryInterface(Ci.nsISSLSocketControl).peerId
      );
    }
    Services.obs.addObserver(observer, TOPIC);
  });
}

/**
 * Loads a url in the given browser and returns the tls socket's peer id
 * associated with the load.
 * Note: Loads are identified by URI. If multiple loads with the same URI happen
 * concurrently, this method may not map them correctly.
 * @param {MozBrowser} browser
 * @param {string} url
 * @returns {Promise<string>} Resolves on load with the peer id associated with
 * the load.
 */
function loadURLInFrame(browser, url) {
  let loadPromise = waitForLoad(url);
  ContentTask.spawn(browser, [url], async testURL => {
    let frame = content.document.createElement("iframe");
    frame.src = testURL;
    content.document.body.appendChild(frame);
  });
  return loadPromise;
}

add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.cache.disk.enable", false],
      ["browser.cache.memory.enable", false],
      ["privacy.partition.network_state", true],
      // The test harness acts as a proxy, so we need to make sure to also
      // partition for proxies.
      ["privacy.partition.network_state.connection_with_proxy", true],
    ],
  });

  // C (first party)
  let loadPromiseC = waitForLoad(TEST_URL_C);
  await BrowserTestUtils.withNewTab(TEST_URL_C, async () => {});
  let peerIdC = await loadPromiseC;

  // C embedded in C (same origin)
  let peerIdCC;
  await BrowserTestUtils.withNewTab(TEST_ORIGIN_C, async browser => {
    peerIdCC = await loadURLInFrame(browser, TEST_URL_C);
  });

  // C embedded in A (third party)
  let peerIdAC;
  await BrowserTestUtils.withNewTab(TEST_ORIGIN_A, async browser => {
    peerIdAC = await loadURLInFrame(browser, TEST_URL_C);
  });

  // C embedded in B (third party)
  let peerIdBC;
  await BrowserTestUtils.withNewTab(TEST_ORIGIN_B, async browser => {
    peerIdBC = await loadURLInFrame(browser, TEST_URL_C);
  });

  info("Test that top level load and same origin frame have the same peerId.");
  is(peerIdC, peerIdCC, "Should have the same peerId");

  info("Test that all partitioned peer ids are distinct.");
  isnot(peerIdCC, peerIdAC, "Should have different peerId partitioned under A");
  isnot(peerIdCC, peerIdBC, "Should have different peerId partitioned under B");
  isnot(
    peerIdAC,
    peerIdBC,
    "Should have a different peerId under different first parties."
  );
});
