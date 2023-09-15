/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

if (SpecialPowers.useRemoteSubframes) {
  // Bug 1586105: these tests could time out in some extremely slow conditions,
  // when fission is enabled.
  // Requesting a longer timeout should make it pass.
  requestLongerTimeout(2);
}

add_task(async function test_profile_fission_no_private_browsing() {
  // Requesting the complete log to be able to debug Bug 1586105.
  SimpleTest.requestCompleteLog();
  Assert.ok(!Services.profiler.IsActive());
  info("Clear the previous pages just in case we still have some open tabs.");
  await Services.profiler.ClearAllPages();

  info(
    "Start the profiler to test the page information with single frame page."
  );
  await startProfiler();

  info("Open a private window with single_frame.html in it.");
  const win = await BrowserTestUtils.openNewBrowserWindow({
    fission: true,
  });

  try {
    const url = BASE_URL_HTTPS + "single_frame.html";
    const contentBrowser = win.gBrowser.selectedBrowser;
    BrowserTestUtils.startLoadingURIString(contentBrowser, url);
    await BrowserTestUtils.browserLoaded(contentBrowser, false, url);

    const parentPid = Services.appinfo.processID;
    const contentPid = await SpecialPowers.spawn(contentBrowser, [], () => {
      return Services.appinfo.processID;
    });

    // Getting the active Browser ID to assert the page info tabID later.
    const activeTabID = contentBrowser.browsingContext.browserId;

    info("Capture the profile data.");
    const { profile, contentProcess, contentThread } =
      await stopProfilerNowAndGetThreads(contentPid);

    Assert.equal(
      contentThread.isPrivateBrowsing,
      false,
      "The content process has the private browsing flag set to false."
    );

    Assert.equal(
      contentThread.userContextId,
      0,
      "The content process has the information about the container used for this process"
    );

    info(
      "Check if the captured page is the one with correct values we created."
    );

    let pageFound = false;
    for (const page of contentProcess.pages) {
      if (page.url == url) {
        Assert.equal(page.url, url);
        Assert.equal(typeof page.tabID, "number");
        Assert.equal(page.tabID, activeTabID);
        Assert.equal(typeof page.innerWindowID, "number");
        // Top level document will have no embedder.
        Assert.equal(page.embedderInnerWindowID, 0);
        Assert.equal(typeof page.isPrivateBrowsing, "boolean");
        Assert.equal(page.isPrivateBrowsing, false);
        pageFound = true;
        break;
      }
    }
    Assert.equal(pageFound, true);

    info("Check that the profiling logs exist with the expected properties.");
    Assert.equal(typeof profile.profilingLog, "object");
    Assert.equal(typeof profile.profilingLog[parentPid], "object");
    const parentLog = profile.profilingLog[parentPid];
    Assert.equal(typeof parentLog.profilingLogBegin_TSms, "number");
    Assert.equal(typeof parentLog.profilingLogEnd_TSms, "number");
    Assert.equal(typeof parentLog.bufferGlobalController, "object");
    Assert.equal(
      typeof parentLog.bufferGlobalController.controllerCreationTime_TSms,
      "number"
    );

    Assert.equal(typeof profile.profileGatheringLog, "object");
    Assert.equal(typeof profile.profileGatheringLog[parentPid], "object");
    Assert.equal(
      typeof profile.profileGatheringLog[parentPid]
        .profileGatheringLogBegin_TSms,
      "number"
    );
    Assert.equal(
      typeof profile.profileGatheringLog[parentPid].profileGatheringLogEnd_TSms,
      "number"
    );

    Assert.equal(typeof contentProcess.profilingLog, "object");
    Assert.equal(typeof contentProcess.profilingLog[contentPid], "object");
    Assert.equal(
      typeof contentProcess.profilingLog[contentPid].profilingLogBegin_TSms,
      "number"
    );
    Assert.equal(
      typeof contentProcess.profilingLog[contentPid].profilingLogEnd_TSms,
      "number"
    );

    Assert.equal(typeof contentProcess.profileGatheringLog, "undefined");
  } finally {
    await BrowserTestUtils.closeWindow(win);
  }
});

add_task(async function test_profile_fission_private_browsing() {
  // Requesting the complete log to be able to debug Bug 1586105.
  SimpleTest.requestCompleteLog();
  Assert.ok(!Services.profiler.IsActive());
  info("Clear the previous pages just in case we still have some open tabs.");
  await Services.profiler.ClearAllPages();

  info(
    "Start the profiler to test the page information with single frame page."
  );
  await startProfiler();

  info("Open a private window with single_frame.html in it.");
  const win = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
    fission: true,
  });

  try {
    const url = BASE_URL_HTTPS + "single_frame.html";
    const contentBrowser = win.gBrowser.selectedBrowser;
    BrowserTestUtils.startLoadingURIString(contentBrowser, url);
    await BrowserTestUtils.browserLoaded(contentBrowser, false, url);

    const contentPid = await SpecialPowers.spawn(contentBrowser, [], () => {
      return Services.appinfo.processID;
    });

    // Getting the active Browser ID to assert the page info tabID later.
    const activeTabID = contentBrowser.browsingContext.browserId;

    info("Capture the profile data.");
    const { contentProcess, contentThread } =
      await stopProfilerNowAndGetThreads(contentPid);

    Assert.equal(
      contentThread.isPrivateBrowsing,
      true,
      "The content process has the private browsing flag set to true."
    );

    Assert.equal(
      contentThread.userContextId,
      0,
      "The content process has the information about the container used for this process"
    );

    info(
      "Check if the captured page is the one with correct values we created."
    );

    let pageFound = false;
    for (const page of contentProcess.pages) {
      if (page.url == url) {
        Assert.equal(page.url, url);
        Assert.equal(typeof page.tabID, "number");
        Assert.equal(page.tabID, activeTabID);
        Assert.equal(typeof page.innerWindowID, "number");
        // Top level document will have no embedder.
        Assert.equal(page.embedderInnerWindowID, 0);
        Assert.equal(typeof page.isPrivateBrowsing, "boolean");
        Assert.equal(page.isPrivateBrowsing, true);
        pageFound = true;
        break;
      }
    }
    Assert.equal(pageFound, true);
  } finally {
    await BrowserTestUtils.closeWindow(win);
  }
});
