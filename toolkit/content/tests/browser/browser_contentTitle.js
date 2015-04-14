let url = "https://example.com/browser/toolkit/content/tests/browser/file_contentTitle.html";

add_task(function*() {
  let tab = gBrowser.selectedTab = gBrowser.addTab(url);
  let browser = tab.linkedBrowser;
  yield new Promise((resolve) => {
    addEventListener("TestLocationChange", function listener() {
      removeEventListener("TestLocationChange", listener);
      resolve();
    }, true, true);
  });

  is(gBrowser.contentTitle, "Test Page", "Should have the right title.");

  gBrowser.removeTab(tab);
});
