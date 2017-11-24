"use strict";

/**
 * Very basic smoketests for the View Source feature, which also
 * forces on the DocGroup mismatch check that was added in
 * bug 1340719.
 */

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.throw_on_docgroup_mismatch.enabled", true],
    ],
  });
});

/**
 * Tests that we can open View Source in a tab.
 */
add_task(async function test_view_source_in_tab() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["view_source.tab", true],
    ],
  });

  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.com",
  }, async function(browser) {
    let sourceTab = await openViewSource(browser);
    let sourceBrowser = sourceTab.linkedBrowser;
    await waitForSourceLoaded(sourceBrowser);

    await ContentTask.spawn(sourceBrowser, null, async function() {
      Assert.equal(content.document.body.id, "viewsource",
                   "View source mode enabled");
    });

    await BrowserTestUtils.removeTab(sourceTab);
  });

  await SpecialPowers.popPrefEnv();
});

/**
 * Tests that we can open View Source in a window.
 */
add_task(async function test_view_source_in_window() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["view_source.tab", false],
    ],
  });

  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.com",
  }, async function(browser) {
    let sourceWin = await openViewSource(browser);
    await waitForSourceLoaded(sourceWin);
    await ContentTask.spawn(sourceWin.gBrowser, null, async function() {
      Assert.equal(content.document.body.id, "viewsource",
                   "View source mode enabled");
    });

    await closeViewSourceWindow(sourceWin);
  });

  await SpecialPowers.popPrefEnv();
});
