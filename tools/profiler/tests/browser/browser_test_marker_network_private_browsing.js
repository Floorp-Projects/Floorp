/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that we emit network markers accordingly
 */
add_task(async function test_network_markers() {
  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler is not currently active"
  );

  startProfilerForMarkerTests();

  const win = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
    fission: true,
  });
  try {
    const url = BASE_URL_HTTPS + "simple.html?cacheBust=" + Math.random();
    const contentBrowser = win.gBrowser.selectedBrowser;
    BrowserTestUtils.startLoadingURIString(contentBrowser, url);
    await BrowserTestUtils.browserLoaded(contentBrowser, false, url);
    const contentPid = await SpecialPowers.spawn(
      contentBrowser,
      [],
      () => Services.appinfo.processID
    );

    const { parentThread, contentThread } = await stopProfilerNowAndGetThreads(
      contentPid
    );

    const parentNetworkMarkers = getInflatedNetworkMarkers(parentThread);
    const contentNetworkMarkers = getInflatedNetworkMarkers(contentThread);
    info(JSON.stringify(parentNetworkMarkers, null, 2));
    info(JSON.stringify(contentNetworkMarkers, null, 2));

    Assert.equal(
      parentNetworkMarkers.length,
      2,
      `We should get a pair of network markers in the parent thread.`
    );
    Assert.equal(
      contentNetworkMarkers.length,
      2,
      `We should get a pair of network markers in the content thread.`
    );

    const parentStopMarker = parentNetworkMarkers[1];
    const contentStopMarker = contentNetworkMarkers[1];

    const expectedProperties = {
      name: Expect.stringMatches(`Load \\d+:.*${escapeStringRegexp(url)}`),
      data: Expect.objectContains({
        status: "STATUS_STOP",
        URI: url,
        requestMethod: "GET",
        contentType: "text/html",
        startTime: Expect.number(),
        endTime: Expect.number(),
        domainLookupStart: Expect.number(),
        domainLookupEnd: Expect.number(),
        connectStart: Expect.number(),
        tcpConnectEnd: Expect.number(),
        connectEnd: Expect.number(),
        requestStart: Expect.number(),
        responseStart: Expect.number(),
        responseEnd: Expect.number(),
        id: Expect.number(),
        count: Expect.number(),
        pri: Expect.number(),
        isPrivateBrowsing: true,
      }),
    };

    Assert.objectContains(parentStopMarker, expectedProperties);
    // The cache information is missing from the content marker, it's only part
    // of the parent marker. See Bug 1544821.
    Assert.objectContains(parentStopMarker.data, {
      // Because the request races with the cache, these 2 values are valid:
      // "Missed" when the cache answered before we get a result from the network.
      // "Unresolved" when we got a response from the network before the cache subsystem.
      cache: Expect.stringMatches(/^(Missed|Unresolved)$/),
    });
    Assert.objectContains(contentStopMarker, expectedProperties);
  } finally {
    await BrowserTestUtils.closeWindow(win);
  }
});
