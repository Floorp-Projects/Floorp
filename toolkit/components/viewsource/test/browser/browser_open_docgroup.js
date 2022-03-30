"use strict";

/**
 * Very basic smoketests for the View Source feature, which also
 * forces on the DocGroup mismatch check that was added in
 * bug 1340719.
 */

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.throw_on_docgroup_mismatch.enabled", true]],
  });
});

/**
 * Tests that we can open View Source in a tab.
 */
add_task(async function test_view_source_in_tab() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "http://example.com",
    },
    async function(browser) {
      let sourceTab = await openViewSourceForBrowser(browser);
      let sourceBrowser = sourceTab.linkedBrowser;

      await SpecialPowers.spawn(sourceBrowser, [], async function() {
        Assert.equal(
          content.document.body.id,
          "viewsource",
          "View source mode enabled"
        );
      });

      BrowserTestUtils.removeTab(sourceTab);
    }
  );

  await SpecialPowers.popPrefEnv();
});
