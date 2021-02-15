/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that we emit network markers accordingly.
 * In this file we'll test a few service worker cases.
 */

registerCleanupFunction(() => SpecialPowers.removeAllServiceWorkerData());

add_task(async function test_network_markers_service_worker_register() {
  // In this first step, we request an HTML page that will register a service
  // worker. We'll wait until the service worker is fully installed before
  // checking various things.
  if (!AppConstants.MOZ_GECKO_PROFILER) {
    return;
  }
  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler is not currently active"
  );

  startProfilerForMarkerTests();

  const url =
    BASE_URL_HTTPS +
    "serviceworkers/serviceworker_register.html?cacheBust=" +
    Math.random();
  await BrowserTestUtils.withNewTab(url, async contentBrowser => {
    const contentPid = await SpecialPowers.spawn(
      contentBrowser,
      [],
      () => Services.appinfo.processID
    );

    await SpecialPowers.spawn(contentBrowser, [], async function() {
      await content.wrappedJSObject.registerServiceWorkerAndWait();
    });

    const { parentThread, contentThread } = await stopProfilerNowAndGetThreads(
      contentPid
    );

    const parentNetworkMarkers = getInflatedNetworkMarkers(parentThread);
    const contentNetworkMarkers = getInflatedNetworkMarkers(contentThread);
    info(JSON.stringify(parentNetworkMarkers, null, 2));
    info(JSON.stringify(contentNetworkMarkers, null, 2));

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

    // Let's make sure we actually have a registered service workers, to prevent
    // stupid mistakes.
    const workers = await SpecialPowers.registeredServiceWorkers();
    Assert.equal(
      workers.length,
      1,
      "One service worker should be properly registered."
    );
  });
});

add_task(async function test_network_markers_service_worker_use() {
  // In this test we request an HTML file that itself contains resources that
  // are redirected.
  if (!AppConstants.MOZ_GECKO_PROFILER) {
    return;
  }
  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler is not currently active"
  );

  startProfilerForMarkerTests();

  const url =
    BASE_URL_HTTPS +
    "serviceworkers/serviceworker_page.html?cacheBust=" +
    Math.random();
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

    // const parentPairs = getPairsOfNetworkMarkers(parentNetworkMarkers);
    const contentPairs = getPairsOfNetworkMarkers(contentNetworkMarkers);

    // First, make sure we properly matched all start with stop markers. This
    // means that both arrays should contain only arrays of 2 elements.
    /* This test fails for now, see Bug 1567222.
    parentPairs.forEach(pair =>
      Assert.equal(
        pair.length,
        2,
        `For the URL ${pair[0].data.URI} we should get 2 markers in the parent process.`
      )
    );
    */
    contentPairs.forEach(pair =>
      Assert.equal(
        pair.length,
        2,
        `For the URL ${pair[0].data.URI} we should get 2 markers in the content process.`
      )
    );
  });
});
