/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that we emit network markers accordingly.
 * In this file we'll test the redirect cases.
 */
add_task(async function test_network_markers_service_worker_setup() {
  // Disabling cache makes the result more predictable especially in verify mode.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.cache.disk.enable", false],
      ["browser.cache.memory.enable", false],
    ],
  });
});

add_task(async function test_network_markers_redirect_simple() {
  // In this test, we request an HTML page that gets redirected. This is a
  // top-level navigation.
  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler is not currently active"
  );

  startProfilerForMarkerTests();

  const targetFileNameWithCacheBust = "simple.html";
  const url =
    BASE_URL +
    "redirect.sjs?" +
    encodeURIComponent(targetFileNameWithCacheBust);
  const targetUrl = BASE_URL + targetFileNameWithCacheBust;

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
        domainLookupStart: Expect.number(),
        domainLookupEnd: Expect.number(),
        connectStart: Expect.number(),
        tcpConnectEnd: Expect.number(),
        connectEnd: Expect.number(),
        requestStart: Expect.number(),
        responseStart: Expect.number(),
        responseEnd: Expect.number(),
        id: Expect.number(),
        redirectId: parentStopMarker.data.id,
        pri: Expect.number(),
        cache: Expect.stringMatches(/Missed|Unresolved/),
        redirectType: "Permanent",
        isHttpToHttpsRedirect: false,
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

add_task(async function test_network_markers_redirect_resources() {
  // In this test we request an HTML file that itself contains resources that
  // are redirected.
  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler is not currently active"
  );

  startProfilerForMarkerTests();

  const url = BASE_URL + "page_with_resources.html?cacheBust=" + Math.random();
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
      8,
      `We should get 4 pairs of network markers in the parent thread.`
      // 1 - The main page
      // 2 - The SVG
      // 3 - The redirected request for the second SVG request.
      // 4 - The SVG, again
    );

    /* In this second test, the top level navigation request isn't redirected.
     * Contrary to Bug 1692879 we get all network markers for redirected
     * resources. */
    Assert.equal(
      contentNetworkMarkers.length,
      8,
      `We should get 4 pairs of network markers in the content thread.`
    );

    // The same resource firefox-logo-nightly.svg is requested twice, but the
    // second time it is redirected.
    // We're not interested in the main page, as we test that in other files.
    // In this page we're only interested in the marker for requested resources.

    const parentPairs = getPairsOfNetworkMarkers(parentNetworkMarkers);
    const contentPairs = getPairsOfNetworkMarkers(contentNetworkMarkers);

    // First, make sure we properly matched all start with stop markers. This
    // means that both arrays should contain only arrays of 2 elements.
    parentPairs.forEach(pair =>
      Assert.equal(
        pair.length,
        2,
        `For the URL ${pair[0].data.URI} we should get 2 markers in the parent process.`
      )
    );
    contentPairs.forEach(pair =>
      Assert.equal(
        pair.length,
        2,
        `For the URL ${pair[0].data.URI} we should get 2 markers in the content process.`
      )
    );

    const parentFirstStopMarker = parentPairs[1][1];
    const parentRedirectMarker = parentPairs[2][1];
    const parentSecondStopMarker = parentPairs[3][1];
    const contentFirstStopMarker = contentPairs[1][1];
    const contentRedirectMarker = contentPairs[2][1];
    const contentSecondStopMarker = contentPairs[3][1];

    const expectedCommonDataProperties = {
      type: "Network",
      requestMethod: "GET",
      startTime: Expect.number(),
      endTime: Expect.number(),
      id: Expect.number(),
      pri: Expect.number(),
      innerWindowID: Expect.number(),
    };

    // These properties are present when a connection is fully opened. This is
    // most often the case, unless we're in verify mode, because in that case
    // we run the same tests several times in the same Firefox and they might be
    // cached, or in chaos mode Firefox may make all requests sequentially on
    // the same connection.
    // In these cases, these properties won't always be present.
    const expectedConnectionProperties = {
      domainLookupStart: Expect.number(),
      domainLookupEnd: Expect.number(),
      connectStart: Expect.number(),
      tcpConnectEnd: Expect.number(),
      connectEnd: Expect.number(),
      requestStart: Expect.number(),
      responseStart: Expect.number(),
      responseEnd: Expect.number(),
    };

    const expectedPropertiesForStopMarker = {
      name: Expect.stringMatches(/Load \d+:.*\/firefox-logo-nightly\.svg/),
    };

    const expectedDataPropertiesForStopMarker = {
      ...expectedCommonDataProperties,
      ...expectedConnectionProperties,
      status: "STATUS_STOP",
      URI: Expect.stringContains("/firefox-logo-nightly.svg"),
      contentType: "image/svg+xml",
      count: Expect.number(),
    };

    const expectedPropertiesForRedirectMarker = {
      name: Expect.stringMatches(
        /Load \d+:.*\/redirect.sjs\?firefox-logo-nightly\.svg/
      ),
    };

    const expectedDataPropertiesForRedirectMarker = {
      ...expectedCommonDataProperties,
      ...expectedConnectionProperties,
      status: "STATUS_REDIRECT",
      URI: Expect.stringContains("/redirect.sjs?firefox-logo-nightly.svg"),
      RedirectURI: Expect.stringContains("/firefox-logo-nightly.svg"),
      contentType: null,
      redirectType: "Permanent",
      isHttpToHttpsRedirect: false,
    };

    Assert.objectContains(
      parentFirstStopMarker,
      expectedPropertiesForStopMarker
    );
    Assert.objectContainsOnly(parentFirstStopMarker.data, {
      ...expectedDataPropertiesForStopMarker,
      // The cache information is missing from the content marker, it's only part
      // of the parent marker. See Bug 1544821.
      // Also, because the request races with the cache, these 2 values are valid:
      // "Missed" when the cache answered before we get a result from the network.
      // "Unresolved" when we got a response from the network before the cache subsystem.
      cache: Expect.stringMatches(/^(Missed|Unresolved)$/),
    });

    Assert.objectContains(
      contentFirstStopMarker,
      expectedPropertiesForStopMarker
    );
    Assert.objectContainsOnly(
      contentFirstStopMarker.data,
      expectedDataPropertiesForStopMarker
    );

    Assert.objectContains(
      parentRedirectMarker,
      expectedPropertiesForRedirectMarker
    );
    Assert.objectContainsOnly(parentRedirectMarker.data, {
      ...expectedDataPropertiesForRedirectMarker,
      redirectId: parentSecondStopMarker.data.id,
      // See above for the full explanation about the cache property.
      cache: Expect.stringMatches(/^(Missed|Unresolved)$/),
    });

    Assert.objectContains(
      contentRedirectMarker,
      expectedPropertiesForRedirectMarker
    );
    Assert.objectContainsOnly(contentRedirectMarker.data, {
      ...expectedDataPropertiesForRedirectMarker,
      redirectId: contentSecondStopMarker.data.id,
    });

    Assert.objectContains(
      parentSecondStopMarker,
      expectedPropertiesForStopMarker
    );
    Assert.objectContainsOnly(parentSecondStopMarker.data, {
      ...expectedDataPropertiesForStopMarker,
      // The "count" property is absent from the content marker.
      count: Expect.number(),
      // See above for the full explanation about the cache property.
      cache: Expect.stringMatches(/^(Missed|Unresolved)$/),
    });

    Assert.objectContains(
      contentSecondStopMarker,
      expectedPropertiesForStopMarker
    );
    Assert.objectContainsOnly(
      contentSecondStopMarker.data,
      expectedDataPropertiesForStopMarker
    );
  });
});
