"use strict";

/**
 * Tests that content can open windows at requested dimensions
 * of height and width.
 */

/**
 * This utility function does most of the actual testing. We
 * construct a feature string suitable for the passed in width
 * and height, and then run that script in content to open the
 * new window. When the new window comes up, this function tests
 * to ensure that the content area of the initial browser is the
 * requested dimensions. Finally, we also ensure that we're not
 * persisting the position, size or sizemode of the new browser
 * window.
 */
function test_dimensions({ width, height }) {
  let features = [];
  if (width) {
    features.push(`width=${width}`);
  }
  if (height) {
    features.push(`height=${height}`);
  }
  const FEATURE_STR = features.join(",");
  const SCRIPT_PAGE = `data:text/html,<script>window.open("about:blank", "_blank", "${FEATURE_STR}");</script>`;

  let newWinPromise = BrowserTestUtils.waitForNewWindow();

  return BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: SCRIPT_PAGE,
    },
    async function(browser) {
      let win = await newWinPromise;
      let rect = win.gBrowser.selectedBrowser.getBoundingClientRect();

      if (width) {
        Assert.equal(rect.width, width, "Should have the requested width");
      }

      if (height) {
        Assert.equal(rect.height, height, "Should have the requested height");
      }

      let treeOwner = win.docShell.treeOwner;
      let persistPosition = {};
      let persistSize = {};
      let persistSizeMode = {};
      treeOwner.getPersistence(persistPosition, persistSize, persistSizeMode);

      Assert.ok(!persistPosition.value, "Should not persist position");
      Assert.ok(!persistSize.value, "Should not persist size");
      Assert.ok(!persistSizeMode.value, "Should not persist size mode");

      await BrowserTestUtils.closeWindow(win);
    }
  );
}

add_task(async function test_new_sized_window() {
  await test_dimensions({ width: 100 });
  await test_dimensions({ height: 150 });
  await test_dimensions({ width: 300, height: 200 });
});
