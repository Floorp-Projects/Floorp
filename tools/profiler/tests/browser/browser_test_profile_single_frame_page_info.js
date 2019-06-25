/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_profile_multi_frame_page_info() {
  if (!AppConstants.MOZ_GECKO_PROFILER) {
    return;
  }
  Assert.ok(!Services.profiler.IsActive());
  // Clear all pages in case we have some pages registered before.
  await Services.profiler.ClearAllPages();
  startProfiler();

  // multi_frame.html embeds single_frame.html inside an iframe.
  const url = BASE_URL + "multi_frame.html";
  let contentPid;
  await BrowserTestUtils.withNewTab(url, async function(contentBrowser) {
    contentPid = await ContentTask.spawn(contentBrowser, null, () => {
      return Services.appinfo.processID;
    });
  });

  const profile = await Services.profiler.getProfileDataAsync();
  Services.profiler.StopProfiler();

  let foundPage = 0;
  // We need to find the correct content process for that tab.
  let contentProcess = profile.processes.find(p => p.threads[0].pid == contentPid);
  for (const page of contentProcess.pages) {
    // Parent page
    if (page.url == url) {
      Assert.equal(page.url, url);
      Assert.equal(typeof page.docshellId, "string");
      Assert.equal(typeof page.historyId, "number");
      Assert.equal(page.isSubFrame, false);
      foundPage++;
    }

    // Child page (iframe)
    if (page.url == BASE_URL + "single_frame.html") {
      Assert.equal(page.url, BASE_URL + "single_frame.html");
      Assert.equal(typeof page.docshellId, "string");
      Assert.equal(typeof page.historyId, "number");
      Assert.equal(page.isSubFrame, true);
      foundPage++;
    }
  }
  Assert.equal(foundPage, 2);
});
