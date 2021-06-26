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
  info("Clear the previous pages just in case we still some open tabs.");
  await Services.profiler.ClearAllPages();

  info(
    "Start the profiler to test the page information with single frame page."
  );
  startProfiler();

  info("Open a tab with single_frame.html in it.");
  const url = BASE_URL + "single_frame.html";
  await BrowserTestUtils.withNewTab(url, async function(contentBrowser) {
    const contentPid = await SpecialPowers.spawn(contentBrowser, [], () => {
      return Services.appinfo.processID;
    });

    // Getting the active Browser ID to assert the page info tabID later.
    const win = Services.wm.getMostRecentWindow("navigator:browser");
    const activeTabID = win.gBrowser.selectedBrowser.browsingContext.browserId;

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
        pageFound = true;
        break;
      }
    }
    Assert.equal(pageFound, true);
  });
});
