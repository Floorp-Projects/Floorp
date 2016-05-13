add_task(function* () {
  yield BrowserTestUtils.withNewTab({gBrowser, url: "about:config"}, function*(browser) {
    let newTabURL = "http://www.example.com/";
    yield ContentTask.spawn(browser, newTabURL, function*(newTabURL) {
      let doc = content.document;
      let label = doc.createElement("label");
      label.href = newTabURL;
      label.id = "textlink-test";
      label.className = "text-link";
      label.textContent = "click me";
      doc.documentElement.append(label);
    });

    // Test that click will open tab in foreground.
    let awaitNewTab = BrowserTestUtils.waitForNewTab(gBrowser, newTabURL);
    yield BrowserTestUtils.synthesizeMouseAtCenter("#textlink-test", {}, browser);
    let newTab = yield awaitNewTab;
    is(newTab.linkedBrowser, gBrowser.selectedBrowser, "selected tab should be example page");
    yield BrowserTestUtils.removeTab(gBrowser.selectedTab);

    // Test that ctrl+shift+click/meta+shift+click will open tab in background.
    awaitNewTab = BrowserTestUtils.waitForNewTab(gBrowser, newTabURL);
    yield BrowserTestUtils.synthesizeMouseAtCenter("#textlink-test",
      {ctrlKey: true, metaKey: true, shiftKey: true},
      browser);
    yield awaitNewTab;
    is(gBrowser.selectedBrowser, browser, "selected tab should be original tab");
    yield BrowserTestUtils.removeTab(gBrowser.tabs[gBrowser.tabs.length - 1]);

    // Middle-clicking should open tab in foreground.
    awaitNewTab = BrowserTestUtils.waitForNewTab(gBrowser, newTabURL);
    yield BrowserTestUtils.synthesizeMouseAtCenter("#textlink-test",
      {button: 1}, browser);
    newTab = yield awaitNewTab;
    is(newTab.linkedBrowser, gBrowser.selectedBrowser, "selected tab should be example page");
    yield BrowserTestUtils.removeTab(gBrowser.tabs[gBrowser.tabs.length - 1]);
  });
});
