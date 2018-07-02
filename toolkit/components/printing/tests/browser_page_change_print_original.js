// This file spawns content tasks.
/* global content */

const TEST_PATH = getRootDirectory(gTestPath)
                    .replace("chrome://mochitests/content", "http://example.com");

/**
 * Verify that if the page contents change after print preview is initialized,
 * and we re-initialize print preview (e.g. by changing page orientation),
 * we still show (and will therefore print) the original contents.
 */
add_task(async function pp_after_orientation_change() {
  const URI = TEST_PATH + "file_page_change_print_original_1.html";
  // Can only do something if we have a print preview UI:
  if (AppConstants.platform != "win" && AppConstants.platform != "linux") {
    ok(true, "Can't test if there's no print preview.");
    return;
  }

  // Ensure we get a browserStopped for this browser
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URI, false, true);
  let browserToPrint = tab.linkedBrowser;
  let ppBrowser = PrintPreviewListener.getPrintPreviewBrowser();

  // Get a promise now that resolves when the original tab's location changes.
  let originalTabNavigated = BrowserTestUtils.browserStopped(browserToPrint);

  // Enter print preview:
  let printPreviewEntered = BrowserTestUtils.waitForMessage(ppBrowser.messageManager, "Printing:Preview:Entered");
  document.getElementById("cmd_printPreview").doCommand();
  await printPreviewEntered;

  // Assert that we are showing the original page
  await ContentTask.spawn(ppBrowser, null, async function() {
    is(content.document.body.textContent.trim(), "INITIAL PAGE", "Should have initial page print previewed.");
  });

  await originalTabNavigated;

  // Change orientation and wait for print preview to re-enter:
  let orient = PrintUtils.getPrintSettings().orientation;
  let orientToSwitchTo = orient != Ci.nsIPrintSettings.kPortraitOrientation ?
    "portrait" : "landscape";
  let printPreviewToolbar = document.querySelector("toolbar[is=printpreview-toolbar]");

  printPreviewEntered = BrowserTestUtils.waitForMessage(ppBrowser.messageManager, "Printing:Preview:Entered");
  printPreviewToolbar.orient(orientToSwitchTo);
  await printPreviewEntered;

  // Check that we're still showing the original page.
  await ContentTask.spawn(ppBrowser, null, async function() {
    is(content.document.body.textContent.trim(), "INITIAL PAGE", "Should still have initial page print previewed.");
  });

  // Check that the other tab is definitely showing the new page:
  await ContentTask.spawn(browserToPrint, null, async function() {
    is(content.document.body.textContent.trim(), "REPLACED PAGE!", "Original page should have changed.");
  });

  PrintUtils.exitPrintPreview();

  BrowserTestUtils.removeTab(tab);
});
