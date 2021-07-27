/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that we emit network markers accordingly.
 * In this file we'll test that we behave properly with STS redirections.
 */

add_task(async function test_network_markers_service_worker_setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Disabling cache makes the result more predictable especially in verify mode.
      ["browser.cache.disk.enable", false],
      ["browser.cache.memory.enable", false],
      // We want to test upgrading requests
      ["dom.security.https_only_mode", true],
    ],
  });
});

add_task(async function test_network_markers_redirect_to_https() {
  // In this test, we request an HTML page with http that gets redirected to https.
  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler is not currently active"
  );

  startProfilerForMarkerTests();

  const url = BASE_URL + "simple.html";
  const targetUrl = BASE_URL_HTTPS + "simple.html";

  await BrowserTestUtils.withNewTab(url, async contentBrowser => {
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
      4,
      `We should get 2 pairs of network markers in the parent thread.`
    );

    /* It looks like that for a redirection for the top level navigation, the
     * content thread sees the markers for the second request only.
     * See Bug 1692879. */
    Assert.equal(
      contentNetworkMarkers.length,
      2,
      `We should get one pair of network markers in the content thread.`
    );

    const parentRedirectMarker = parentNetworkMarkers[1];
    const parentStopMarker = parentNetworkMarkers[3];
    // There's no content redirect marker for the reason outlined above.
    const contentStopMarker = contentNetworkMarkers[1];

    Assert.objectContains(parentRedirectMarker, {
      name: Expect.stringMatches(`Load \\d+:.*${escapeStringRegexp(url)}`),
      data: Expect.objectContainsOnly({
        type: "Network",
        status: "STATUS_REDIRECT",
        URI: url,
        RedirectURI: targetUrl,
        requestMethod: "GET",
        contentType: null,
        startTime: Expect.number(),
        endTime: Expect.number(),
        id: Expect.number(),
        redirectId: parentStopMarker.data.id,
        pri: Expect.number(),
        cache: "Unresolved",
        redirectType: "Permanent",
        isHttpToHttpsRedirect: true,
      }),
    });

    const expectedProperties = {
      name: Expect.stringMatches(
        `Load \\d+:.*${escapeStringRegexp(targetUrl)}`
      ),
    };
    const expectedDataProperties = {
      type: "Network",
      status: "STATUS_STOP",
      URI: targetUrl,
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
    };

    Assert.objectContains(parentStopMarker, expectedProperties);
    Assert.objectContains(contentStopMarker, expectedProperties);

    // The cache information is missing from the content marker, it's only part
    // of the parent marker. See Bug 1544821.
    Assert.objectContainsOnly(parentStopMarker.data, {
      ...expectedDataProperties,
      // Because the request races with the cache, these 2 values are valid:
      // "Missed" when the cache answered before we get a result from the network.
      // "Unresolved" when we got a response from the network before the cache subsystem.
      cache: Expect.stringMatches(/^(Missed|Unresolved)$/),
    });
    Assert.objectContainsOnly(contentStopMarker.data, expectedDataProperties);
  });
});
