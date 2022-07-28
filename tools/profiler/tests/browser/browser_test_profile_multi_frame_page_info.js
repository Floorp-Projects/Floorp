/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

if (SpecialPowers.useRemoteSubframes) {
  // Bug 1586105: these tests could time out in some extremely slow conditions,
  // when fission is enabled.
  // Requesting a longer timeout should make it pass.
  requestLongerTimeout(2);
}

add_task(async function test_profile_multi_frame_page_info() {
  // Requesting the complete log to be able to debug Bug 1586105.
  SimpleTest.requestCompleteLog();
  Assert.ok(!Services.profiler.IsActive());
  info("Clear the previous pages just in case we still have some open tabs.");
  await Services.profiler.ClearAllPages();

  info(
    "Start the profiler to test the page information with multi frame page."
  );
  await startProfiler();

  info("Open a tab with multi_frame.html in it.");
  // multi_frame.html embeds single_frame.html inside an iframe.
  const url = BASE_URL + "multi_frame.html";
  await BrowserTestUtils.withNewTab(url, async function(contentBrowser) {
    const contentPid = await SpecialPowers.spawn(contentBrowser, [], () => {
      return Services.appinfo.processID;
    });

    // Getting the active Browser ID to assert the page info tabID later.
    const win = Services.wm.getMostRecentWindow("navigator:browser");
    const activeTabID = win.gBrowser.selectedBrowser.browsingContext.browserId;

    info("Capture the profile data.");
    const { contentProcess } = await stopProfilerNowAndGetThreads(contentPid);

    info(
      "Check if the captured pages are the ones with correct values we created."
    );

    let parentPage;
    let foundPage = 0;
    for (const page of contentProcess.pages) {
      // Parent page
      if (page.url == url) {
        Assert.equal(page.url, url);
        Assert.equal(typeof page.tabID, "number");
        Assert.equal(page.tabID, activeTabID);
        Assert.equal(typeof page.innerWindowID, "number");
        // Top level document will have no embedder.
        Assert.equal(page.embedderInnerWindowID, 0);
        Assert.equal(typeof page.isPrivateBrowsing, "boolean");
        Assert.equal(page.isPrivateBrowsing, false);
        parentPage = page;
        foundPage++;
        break;
      }
    }

    Assert.notEqual(typeof parentPage, "undefined");

    for (const page of contentProcess.pages) {
      // Child page (iframe)
      if (page.url == BASE_URL + "single_frame.html") {
        Assert.equal(page.url, BASE_URL + "single_frame.html");
        Assert.equal(typeof page.tabID, "number");
        Assert.equal(page.tabID, activeTabID);
        Assert.equal(typeof page.innerWindowID, "number");
        Assert.equal(typeof page.embedderInnerWindowID, "number");
        Assert.notEqual(typeof parentPage, "undefined");
        Assert.equal(page.embedderInnerWindowID, parentPage.innerWindowID);
        Assert.equal(typeof page.isPrivateBrowsing, "boolean");
        Assert.equal(page.isPrivateBrowsing, false);
        foundPage++;
        break;
      }
    }

    Assert.equal(foundPage, 2);
  });
});
