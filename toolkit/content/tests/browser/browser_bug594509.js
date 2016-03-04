add_task(function* () {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:rights");

  yield ContentTask.spawn(tab.linkedBrowser, null, function* () {
    Assert.ok(content.document.getElementById("your-rights"), "about:rights content loaded");
  });

  yield BrowserTestUtils.removeTab(tab);
});
