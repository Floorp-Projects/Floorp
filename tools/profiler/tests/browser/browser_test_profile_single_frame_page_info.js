/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

if (SpecialPowers.useRemoteSubframes) {
  // Bug 1586105: these tests could time out in some extremely slow conditions,
  // when fission is enabled.
  // Requesting a longer timeout should make it pass.
  requestLongerTimeout(2);
}

add_task(async function test_profile_single_frame_page_info() {
  // Requesting the complete log to be able to debug Bug 1586105.
  SimpleTest.requestCompleteLog();
  Assert.ok(!Services.profiler.IsActive());
  info("Clear the previous pages just in case we still have some open tabs.");
  await Services.profiler.ClearAllPages();

  info(
    "Start the profiler to test the page information with single frame page."
  );
  await startProfiler();

  info("Open a tab with single_frame.html in it.");
  const url = BASE_URL + "single_frame.html";
  await BrowserTestUtils.withNewTab(url, async function (contentBrowser) {
    const contentPid = await SpecialPowers.spawn(contentBrowser, [], () => {
      return Services.appinfo.processID;
    });

    // Getting the active Browser ID to assert the page info tabID later.
    const win = Services.wm.getMostRecentWindow("navigator:browser");
    const activeTabID = win.gBrowser.selectedBrowser.browsingContext.browserId;

    info("Capture the profile data.");
    const { contentProcess } = await stopProfilerNowAndGetThreads(contentPid);

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
  });
});

add_task(async function test_profile_private_browsing() {
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
    fission: false,
    private: true,
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

    // This information is available with fission only.
    Assert.equal(
      contentThread.isPrivateBrowsing,
      undefined,
      "The content process has no private browsing flag."
    );

    Assert.equal(
      contentThread.userContextId,
      undefined,
      "The content process has no information about the container used for this process."
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
