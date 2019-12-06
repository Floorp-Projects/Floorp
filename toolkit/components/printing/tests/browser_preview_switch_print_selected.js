/**
 * Verify if we correctly switch print preview browsers based on whether
 * Simplify Page checkbox is checked.
 */

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

add_task(async function set_simplify_and_reader_pref() {
  // Ensure we have the simplify page preference set
  await SpecialPowers.pushPrefEnv({
    set: [
      ["print.use_simplify_page", true],
      ["reader.parse-on-load.enabled", true],
    ],
  });
});

add_task(async function switch_print_preview_browsers() {
  let url = TEST_PATH + "simplifyArticleSample.html";

  // Can only do something if we have a print preview UI:
  if (AppConstants.platform != "win" && AppConstants.platform != "linux") {
    ok(false, "Can't test if there's no print preview.");
    return;
  }

  // Ensure we get a browserStopped for this browser
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    url,
    false,
    true
  );

  // Wait for Reader Mode to parse and set property of loaded tab
  await BrowserTestUtils.waitForCondition(() => {
    return tab.linkedBrowser.isArticle;
  });

  // Enter print preview
  let defaultPPBrowser = PrintPreviewListener.getPrintPreviewBrowser();
  let defaultPPEntered = BrowserTestUtils.waitForMessage(
    defaultPPBrowser.messageManager,
    "Printing:Preview:Entered"
  );
  document.getElementById("cmd_printPreview").doCommand();
  await defaultPPEntered;

  // Assert that we are showing the initial content on default print preview browser
  await SpecialPowers.spawn(defaultPPBrowser, [], async function() {
    is(content.document.title, "Article title", "Should have initial content.");
  });

  // Here we call simplified mode
  let simplifiedPPBrowser = PrintPreviewListener.getSimplifiedPrintPreviewBrowser();
  let simplifiedPPEntered = BrowserTestUtils.waitForMessage(
    simplifiedPPBrowser.messageManager,
    "Printing:Preview:Entered"
  );
  let printPreviewToolbar = document.getElementById("print-preview-toolbar");

  // Wait for simplify page option enablement
  await BrowserTestUtils.waitForCondition(() => {
    return !printPreviewToolbar.mSimplifyPageCheckbox.disabled;
  });

  printPreviewToolbar.mSimplifyPageCheckbox.click();
  await simplifiedPPEntered;

  // Assert that simplify page option is checked
  is(
    printPreviewToolbar.mSimplifyPageCheckbox.checked,
    true,
    "Should have simplify page option checked"
  );

  // Assert that we are showing custom content on simplified print preview browser
  await SpecialPowers.spawn(simplifiedPPBrowser, [], async function() {
    is(content.document.title, "Article title", "Should have custom content.");
  });

  // Assert that we are selecting simplified print preview browser, and not default one
  is(
    gBrowser.selectedTab.linkedBrowser,
    simplifiedPPBrowser,
    "Should have simplified print preview browser selected"
  );
  isnot(
    gBrowser.selectedTab.linkedBrowser,
    defaultPPBrowser,
    "Should not have default print preview browser selected"
  );

  // Switch back to default print preview content
  defaultPPEntered = BrowserTestUtils.waitForMessage(
    defaultPPBrowser.messageManager,
    "Printing:Preview:Entered"
  );
  printPreviewToolbar.mSimplifyPageCheckbox.click();
  await defaultPPEntered;

  // Assert that simplify page option is not checked
  isnot(
    printPreviewToolbar.mSimplifyPageCheckbox.checked,
    true,
    "Should not have simplify page option checked"
  );

  // Assert that we are showing the initial content on default print preview browser
  await SpecialPowers.spawn(defaultPPBrowser, [], async function() {
    is(content.document.title, "Article title", "Should have initial content.");
  });

  // Assert that we are selecting default print preview browser, and not simplified one
  is(
    gBrowser.selectedTab.linkedBrowser,
    defaultPPBrowser,
    "Should have default print preview browser selected"
  );
  isnot(
    gBrowser.selectedTab.linkedBrowser,
    simplifiedPPBrowser,
    "Should not have simplified print preview browser selected"
  );

  PrintUtils.exitPrintPreview();

  BrowserTestUtils.removeTab(tab);
});
