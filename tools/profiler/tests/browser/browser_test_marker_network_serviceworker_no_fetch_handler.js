/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that we emit network markers accordingly.
 * In this file we'll test the case of a service worker that has no fetch
 * handlers. In this case, a fetch is done to the network. There may be
 * shortcuts in our code in this case, that's why it's important to test it
 * separately.
 */

const serviceWorkerFileName = "serviceworker_no_fetch_handler.js";
registerCleanupFunction(() => SpecialPowers.removeAllServiceWorkerData());

add_task(async function test_network_markers_service_worker_setup() {
  // Disabling cache makes the result more predictable. Also this makes things
  // simpler when dealing with service workers.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.cache.disk.enable", false],
      ["browser.cache.memory.enable", false],
    ],
  });
});

add_task(async function test_network_markers_service_worker_register() {
  // In this first step, we request an HTML page that will register a service
  // worker. We'll wait until the service worker is fully installed before
  // checking various things.
  const url = `${BASE_URL_HTTPS}serviceworkers/serviceworker_register.html`;
  await BrowserTestUtils.withNewTab(url, async contentBrowser => {
    await SpecialPowers.spawn(
      contentBrowser,
      [serviceWorkerFileName],
      async function (serviceWorkerFileName) {
        await content.wrappedJSObject.registerServiceWorkerAndWait(
          serviceWorkerFileName
        );
      }
    );

    // Let's make sure we actually have a registered service workers.
    const workers = await SpecialPowers.registeredServiceWorkers();
    Assert.equal(
      workers.length,
      1,
      "One service worker should be properly registered."
    );
  });
});

add_task(async function test_network_markers_service_worker_use() {
  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler is not currently active"
  );

  startProfilerForMarkerTests();

  const url = `${BASE_URL_HTTPS}serviceworkers/serviceworker_page.html`;
  await BrowserTestUtils.withNewTab(url, async contentBrowser => {
    const contentPid = await SpecialPowers.spawn(
      contentBrowser,
      [],
      () => Services.appinfo.processID
    );

    const { parentThread, contentThread } = await stopProfilerNowAndGetThreads(
      contentPid
    );

    // By logging a few information about the threads we make debugging easier.
    logInformationForThread("parentThread information", parentThread);
    logInformationForThread("contentThread information", contentThread);

    const parentNetworkMarkers = getInflatedNetworkMarkers(parentThread)
      // When we load a page, Firefox will check the service worker freshness
      // after a few seconds. So when the test lasts a long time (with some test
      // environments) we might see spurious markers about that that we're not
      // interesting in in this part of the test. They're only present in the
      // parent process.
      .filter(marker => !marker.data.URI.includes(serviceWorkerFileName));
    const contentNetworkMarkers = getInflatedNetworkMarkers(contentThread);

    // Here are some logs to ease debugging.
    info(
      "Parent network markers:" + JSON.stringify(parentNetworkMarkers, null, 2)
    );
    info(
      "Content network markers:" +
        JSON.stringify(contentNetworkMarkers, null, 2)
    );

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

    // Let's look at all pairs and make sure we requested all expected files.
    const parentStopMarkers = parentPairs.map(([_, stopMarker]) => stopMarker);
    const contentStopMarkers = contentPairs.map(
      ([_, stopMarker]) => stopMarker
    );

    // These are the files requested by the page.
    // We should see markers for the parent thread and the content thread.
    const expectedFiles = [
      // Please take care that the first element is the top level navigation, as
      // this is special-cased below.
      "serviceworker_page.html",
      "firefox-logo-nightly.svg",
    ].map(filename => `${BASE_URL_HTTPS}serviceworkers/${filename}`);

    Assert.equal(
      parentStopMarkers.length,
      expectedFiles.length,
      "There should be as many stop markers in the parent process as requested files."
    );
    Assert.equal(
      contentStopMarkers.length,
      expectedFiles.length,
      "There should be as many stop markers in the content process as requested files."
    );

    for (const [i, expectedFile] of expectedFiles.entries()) {
      info(
        `Checking if "${expectedFile}" if present in the network markers in both processes.`
      );
      const parentMarker = parentStopMarkers.find(
        marker => marker.data.URI === expectedFile
      );
      const contentMarker = contentStopMarkers.find(
        marker => marker.data.URI === expectedFile
      );

      const commonProperties = {
        name: Expect.stringMatches(
          `Load \\d+:.*${escapeStringRegexp(expectedFile)}`
        ),
      };
      Assert.objectContains(parentMarker, commonProperties);
      Assert.objectContains(contentMarker, commonProperties);

      // We get the full set of properties in this case, because we do an actual
      // fetch to the network.
      const commonDataProperties = {
        type: "Network",
        status: "STATUS_STOP",
        URI: expectedFile,
        requestMethod: "GET",
        contentType: Expect.stringMatches(/^(text\/html|image\/svg\+xml)$/),
        startTime: Expect.number(),
        endTime: Expect.number(),
        id: Expect.number(),
        pri: Expect.number(),
        count: Expect.number(),
        domainLookupStart: Expect.number(),
        domainLookupEnd: Expect.number(),
        connectStart: Expect.number(),
        tcpConnectEnd: Expect.number(),
        connectEnd: Expect.number(),
        requestStart: Expect.number(),
        responseStart: Expect.number(),
        responseEnd: Expect.number(),
      };

      if (i === 0) {
        // The first marker is special cased: this is the top level navigation
        // serviceworker_page.html,
        // and in this case we don't have all the same properties. Especially
        // the innerWindowID information is missing.
        Assert.objectContainsOnly(parentMarker.data, {
          ...commonDataProperties,
          // Note that the parent process has the "cache" information, but not the content
          // process. See Bug 1544821.
          // Also because the request races with the cache, these 2 values are valid:
          // "Missed" when the cache answered before we get a result from the network.
          // "Unresolved" when we got a response from the network before the cache subsystem.
          cache: Expect.stringMatches(/^(Missed|Unresolved)$/),
        });

        Assert.objectContainsOnly(contentMarker.data, commonDataProperties);
      } else {
        // This is the other file firefox-logo-nightly.svg.
        Assert.objectContainsOnly(parentMarker.data, {
          ...commonDataProperties,
          // Because the request races with the cache, these 2 values are valid:
          // "Missed" when the cache answered before we get a result from the network.
          // "Unresolved" when we got a response from the network before the cache subsystem.
          cache: Expect.stringMatches(/^(Missed|Unresolved)$/),
          innerWindowID: Expect.number(),
        });

        Assert.objectContainsOnly(contentMarker.data, {
          ...commonDataProperties,
          innerWindowID: Expect.number(),
        });
      }
    }
  });
});
