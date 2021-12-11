/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that we emit network markers accordingly.
 * In this file we'll test a caching service worker. This service worker will
 * fetch and store requests at install time, and serve them when the page
 * requests them.
 */

const serviceWorkerFileName = "serviceworker_cache_first.js";
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
  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler is not currently active"
  );

  startProfilerForMarkerTests();

  const url = `${BASE_URL_HTTPS}serviceworkers/serviceworker_register.html`;
  await BrowserTestUtils.withNewTab(url, async contentBrowser => {
    const contentPid = await SpecialPowers.spawn(
      contentBrowser,
      [],
      () => Services.appinfo.processID
    );

    await SpecialPowers.spawn(
      contentBrowser,
      [serviceWorkerFileName],
      async function(serviceWorkerFileName) {
        await content.wrappedJSObject.registerServiceWorkerAndWait(
          serviceWorkerFileName
        );
      }
    );

    const {
      parentThread,
      contentThread,
      profile,
    } = await stopProfilerNowAndGetThreads(contentPid);

    // The service worker work happens in a third "thread" or process, let's try
    // to find it.
    // Currently the fetches happen on the main thread for the content process,
    // this may change in the future and we may have to adapt this function.
    // Also please note this isn't necessarily the same content process as the
    // ones for the tab.
    const { serviceWorkerParentThread } = findServiceWorkerThreads(profile);

    // Here are a few sanity checks.
    ok(
      serviceWorkerParentThread,
      "We should find a thread for the service worker."
    );

    Assert.notEqual(
      serviceWorkerParentThread.pid,
      parentThread.pid,
      "We should have a different pid than the parent thread."
    );
    Assert.notEqual(
      serviceWorkerParentThread.tid,
      parentThread.tid,
      "We should have a different tid than the parent thread."
    );

    // Let's make sure we actually have a registered service workers.
    const workers = await SpecialPowers.registeredServiceWorkers();
    Assert.equal(
      workers.length,
      1,
      "One service worker should be properly registered."
    );

    // By logging a few information about the threads we make debugging easier.
    logInformationForThread("parentThread information", parentThread);
    logInformationForThread("contentThread information", contentThread);
    logInformationForThread(
      "serviceWorkerParentThread information",
      serviceWorkerParentThread
    );

    // Now let's check the marker payloads.
    const parentNetworkMarkers = getInflatedNetworkMarkers(parentThread)
      // When we load a page, Firefox will check the service worker freshness
      // after a few seconds. So when the test lasts a long time (with some test
      // environments) we might see spurious markers about that that we're not
      // interesting in in this part of the test. They're only present in the
      // parent process.
      .filter(marker => !marker.data.URI.includes(serviceWorkerFileName));
    const contentNetworkMarkers = getInflatedNetworkMarkers(contentThread);
    const serviceWorkerNetworkMarkers = getInflatedNetworkMarkers(
      serviceWorkerParentThread
    );

    // Some more logs for debugging purposes.
    info(
      "Parent network markers: " + JSON.stringify(parentNetworkMarkers, null, 2)
    );
    info(
      "Content network markers: " +
        JSON.stringify(contentNetworkMarkers, null, 2)
    );
    info(
      "Serviceworker network markers: " +
        JSON.stringify(serviceWorkerNetworkMarkers, null, 2)
    );

    const parentPairs = getPairsOfNetworkMarkers(parentNetworkMarkers);
    const contentPairs = getPairsOfNetworkMarkers(contentNetworkMarkers);
    const serviceWorkerPairs = getPairsOfNetworkMarkers(
      serviceWorkerNetworkMarkers
    );

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
    serviceWorkerPairs.forEach(pair =>
      Assert.equal(
        pair.length,
        2,
        `For the URL ${pair[0].data.URI} we should get 2 markers in the service worker process.`
      )
    );

    // Let's look at all pairs and make sure we requested all expected files.
    const parentStopMarkers = parentPairs.map(([_, stopMarker]) => stopMarker);
    const serviceWorkerStopMarkers = serviceWorkerPairs.map(
      ([_, stopMarker]) => stopMarker
    );

    // These are the files cached by the service worker. We should see markers
    // for both the parent thread and the service worker thread.
    const expectedFiles = [
      "serviceworker_page.html",
      "firefox-logo-nightly.svg",
    ].map(filename => `${BASE_URL_HTTPS}serviceworkers/${filename}`);

    for (const expectedFile of expectedFiles) {
      info(
        `Checking if "${expectedFile}" is present in the network markers in both processes.`
      );
      const parentMarker = parentStopMarkers.find(
        marker => marker.data.URI === expectedFile
      );
      const serviceWorkerMarker = serviceWorkerStopMarkers.find(
        marker => marker.data.URI === expectedFile
      );

      const expectedProperties = {
        name: Expect.stringMatches(
          `Load \\d+:.*${escapeStringRegexp(expectedFile)}`
        ),
        data: Expect.objectContains({
          status: "STATUS_STOP",
          URI: expectedFile,
          requestMethod: "GET",
          contentType: Expect.stringMatches(/^(text\/html|image\/svg\+xml)$/),
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
        }),
      };

      Assert.objectContains(parentMarker, expectedProperties);
      Assert.objectContains(serviceWorkerMarker, expectedProperties);
    }
  });
});

add_task(async function test_network_markers_service_worker_use() {
  // In this test we request an HTML file that itself contains resources that
  // are redirected.
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
      "Parent network markers: " + JSON.stringify(parentNetworkMarkers, null, 2)
    );
    info(
      "Content network markers: " +
        JSON.stringify(contentNetworkMarkers, null, 2)
    );

    const parentPairs = getPairsOfNetworkMarkers(parentNetworkMarkers);
    const contentPairs = getPairsOfNetworkMarkers(contentNetworkMarkers);

    // These are the files cached by the service worker. We should see markers
    // for the parent thread and the content thread.
    const expectedFiles = [
      "serviceworker_page.html",
      "firefox-logo-nightly.svg",
    ].map(filename => `${BASE_URL_HTTPS}serviceworkers/${filename}`);

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
    const parentEndMarkers = parentPairs.map(([_, endMarker]) => endMarker);
    const contentStopMarkers = contentPairs.map(
      ([_, stopMarker]) => stopMarker
    );

    Assert.equal(
      parentEndMarkers.length,
      expectedFiles.length * 2, // one redirect + one stop
      "There should be twice as many end markers in the parent process as requested files."
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
      const [parentRedirectMarker, parentStopMarker] = parentEndMarkers.filter(
        marker => marker.data.URI === expectedFile
      );
      const contentMarker = contentStopMarkers.find(
        marker => marker.data.URI === expectedFile
      );

      const commonDataProperties = {
        type: "Network",
        URI: expectedFile,
        requestMethod: "GET",
        contentType: Expect.stringMatches(/^(text\/html|image\/svg\+xml)$/),
        startTime: Expect.number(),
        endTime: Expect.number(),
        id: Expect.number(),
        pri: Expect.number(),
      };

      const expectedProperties = {
        name: Expect.stringMatches(
          `Load \\d+:.*${escapeStringRegexp(expectedFile)}`
        ),
      };

      Assert.objectContains(parentRedirectMarker, expectedProperties);
      Assert.objectContains(parentStopMarker, expectedProperties);
      Assert.objectContains(contentMarker, expectedProperties);
      if (i === 0) {
        // This is the top level navigation, the HTML file.
        Assert.objectContainsOnly(parentRedirectMarker.data, {
          ...commonDataProperties,
          status: "STATUS_REDIRECT",
          contentType: null,
          cache: "Unresolved",
          RedirectURI: expectedFile,
          redirectType: "Internal",
          redirectId: parentStopMarker.data.id,
          isHttpToHttpsRedirect: false,
        });

        Assert.objectContainsOnly(parentStopMarker.data, {
          ...commonDataProperties,
          status: "STATUS_STOP",
        });

        Assert.objectContainsOnly(contentMarker.data, {
          ...commonDataProperties,
          status: "STATUS_STOP",
        });
      } else {
        Assert.objectContainsOnly(parentRedirectMarker.data, {
          ...commonDataProperties,
          status: "STATUS_REDIRECT",
          contentType: null,
          cache: "Unresolved",
          innerWindowID: Expect.number(),
          RedirectURI: expectedFile,
          redirectType: "Internal",
          redirectId: parentStopMarker.data.id,
          isHttpToHttpsRedirect: false,
        });

        Assert.objectContainsOnly(
          parentStopMarker.data,
          // Note: in the future we may have more properties. We're using the
          // "Only" flavor of the matcher so that we don't forget to update this
          // test when this changes.
          {
            ...commonDataProperties,
            innerWindowID: Expect.number(),
            status: "STATUS_STOP",
          }
        );

        Assert.objectContainsOnly(contentMarker.data, {
          ...commonDataProperties,
          innerWindowID: Expect.number(),
          status: "STATUS_STOP",
        });
      }
    }
  });
});
