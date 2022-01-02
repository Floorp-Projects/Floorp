/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that we emit network markers with the cancel status.
 */
add_task(async function test_network_markers_early_cancel() {
  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler is not currently active"
  );

  startProfilerForMarkerTests();

  const url = BASE_URL + "simple.html?cacheBust=" + Math.random();
  const options = {
    gBrowser,
    url: "about:blank",
    waitForLoad: false,
  };

  const tab = await BrowserTestUtils.openNewForegroundTab(options);
  const loadPromise = BrowserTestUtils.waitForDocLoadAndStopIt(url, tab);
  BrowserTestUtils.loadURI(tab.linkedBrowser, url);
  const contentPid = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    () => Services.appinfo.processID
  );
  await loadPromise;
  const { parentThread, contentThread } = await stopProfilerNowAndGetThreads(
    contentPid
  );
  BrowserTestUtils.removeTab(tab);

  const parentNetworkMarkers = getInflatedNetworkMarkers(parentThread);
  const contentNetworkMarkers = getInflatedNetworkMarkers(contentThread);

  info("parent process: " + JSON.stringify(parentNetworkMarkers, null, 2));
  info("content process: " + JSON.stringify(contentNetworkMarkers, null, 2));

  Assert.equal(
    parentNetworkMarkers.length,
    2,
    `We should get a pair of network markers in the parent thread.`
  );

  // We don't test the markers in the content process, because depending on some
  // timing we can have 0 or 1 (and maybe even 2 (?)).

  const parentStopMarker = parentNetworkMarkers[1];

  const expectedProperties = {
    name: Expect.stringMatches(`Load \\d+:.*${escapeStringRegexp(url)}`),
    data: Expect.objectContainsOnly({
      type: "Network",
      status: "STATUS_CANCEL",
      URI: url,
      requestMethod: "GET",
      contentType: null,
      startTime: Expect.number(),
      endTime: Expect.number(),
      id: Expect.number(),
      pri: Expect.number(),
      cache: "Unresolved",
    }),
  };

  Assert.objectContains(parentStopMarker, expectedProperties);
});
