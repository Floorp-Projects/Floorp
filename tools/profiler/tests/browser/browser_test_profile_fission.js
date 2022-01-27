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
  startProfiler();

  info("Open a private window with single_frame.html in it.");
  const win = await BrowserTestUtils.openNewBrowserWindow({
    fission: true,
  });

  try {
    const url = BASE_URL_HTTPS + "single_frame.html";
    const contentBrowser = win.gBrowser.selectedBrowser;
    BrowserTestUtils.loadURI(contentBrowser, url);
    await BrowserTestUtils.browserLoaded(contentBrowser, false, url);

    const contentPid = await SpecialPowers.spawn(contentBrowser, [], () => {
      return Services.appinfo.processID;
    });

    // Getting the active Browser ID to assert the page info tabID later.
    const activeTabID = contentBrowser.browsingContext.browserId;

    info("Capture the profile data.");
    const profile = await Services.profiler.getProfileDataAsync();
    Services.profiler.StopProfiler();

    let pageFound = false;
    // We need to find the correct content process for that tab.
    let contentProcess = profile.processes.find(
      p => p.threads[0].pid == contentPid
    );

    if (!contentProcess) {
      throw new Error(
        `Could not find the content process with given pid: ${contentPid}`
      );
    }

    Assert.equal(
      contentProcess.threads[0].isPrivateBrowsing,
      false,
      "The content process has the private browsing flag set to false."
    );

    Assert.equal(
      contentProcess.threads[0].userContextId,
      0,
      "The content process has the information about the container used for this process"
    );

    info(
      "Check if the captured page is the one with correct values we created."
    );

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
  startProfiler();

  info("Open a private window with single_frame.html in it.");
  const win = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
    fission: true,
  });

  try {
    const url = BASE_URL_HTTPS + "single_frame.html";
    const contentBrowser = win.gBrowser.selectedBrowser;
    BrowserTestUtils.loadURI(contentBrowser, url);
    await BrowserTestUtils.browserLoaded(contentBrowser, false, url);

    const contentPid = await SpecialPowers.spawn(contentBrowser, [], () => {
      return Services.appinfo.processID;
    });

    // Getting the active Browser ID to assert the page info tabID later.
    const activeTabID = contentBrowser.browsingContext.browserId;

    info("Capture the profile data.");
    const profile = await Services.profiler.getProfileDataAsync();
    Services.profiler.StopProfiler();

    let pageFound = false;
    // We need to find the correct content process for that tab.
    let contentProcess = profile.processes.find(
      p => p.threads[0].pid == contentPid
    );

    if (!contentProcess) {
      throw new Error(
        `Could not find the content process with given pid: ${contentPid}`
      );
    }

    Assert.equal(
      contentProcess.threads[0].isPrivateBrowsing,
      true,
      "The content process has the private browsing flag set to true."
    );

    Assert.equal(
      contentProcess.threads[0].userContextId,
      0,
      "The content process has the information about the container used for this process"
    );

    info(
      "Check if the captured page is the one with correct values we created."
    );

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
