"use strict";

/**
 * Very basic smoketests for the View Source feature, which also
 * forces on the DocGroup mismatch check that was added in
 * bug 1340719.
 */

add_task(function* setup() {
  yield SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.throw_on_docgroup_mismatch.enabled", true],
    ],
  });
});

/**
 * Tests that we can open View Source in a tab.
 */
add_task(function* test_view_source_in_tab() {
  yield SpecialPowers.pushPrefEnv({
    set: [
      ["view_source.tab", true],
    ],
  });

  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.com",
  }, function* (browser) {
    let sourceTab = yield openViewSource(browser);
    let sourceBrowser = sourceTab.linkedBrowser;
    yield waitForSourceLoaded(sourceBrowser);

    yield ContentTask.spawn(sourceBrowser, null, function*() {
      Assert.equal(content.document.body.id, "viewsource",
                   "View source mode enabled");
    });

    yield BrowserTestUtils.removeTab(sourceTab);
  });

  yield SpecialPowers.popPrefEnv();
});

/**
 * Tests that we can open View Source in a window.
 */
add_task(function* test_view_source_in_window() {
  yield SpecialPowers.pushPrefEnv({
    set: [
      ["view_source.tab", false],
    ],
  });

  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.com",
  }, function* (browser) {
    let sourceWin = yield openViewSource(browser);
    yield waitForSourceLoaded(sourceWin);
    yield ContentTask.spawn(sourceWin.gBrowser, null, function*() {
      Assert.equal(content.document.body.id, "viewsource",
                   "View source mode enabled");
    });

    yield closeViewSourceWindow(sourceWin);
  });

  yield SpecialPowers.popPrefEnv();
});
