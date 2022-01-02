var url =
  "https://example.com/browser/toolkit/content/tests/browser/file_contentTitle.html";

add_task(async function() {
  let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, url));
  await BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "TestLocationChange",
    true,
    null,
    true
  );

  is(gBrowser.contentTitle, "Test Page", "Should have the right title.");

  gBrowser.removeTab(tab);
});
