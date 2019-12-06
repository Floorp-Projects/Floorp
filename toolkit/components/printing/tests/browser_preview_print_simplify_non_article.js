/**
 * Verify if we recover from parsing errors of Reader Mode when
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
  let url = TEST_PATH + "simplifyNonArticleSample.html";

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

  // Trick browser to think loaded tab has isArticle property set as true
  tab.linkedBrowser.isArticle = true;

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
    is(
      content.document.title,
      "Non article title",
      "Should have initial content."
    );
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

  // Assert that we are showing recovery content on simplified print preview browser
  await SpecialPowers.spawn(simplifiedPPBrowser, [], async function() {
    await ContentTaskUtils.waitForCondition(
      () => content.document.title === "Failed to load article from page",
      "Simplified document title should be updated with recovery title."
    );
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

  PrintUtils.exitPrintPreview();

  await BrowserTestUtils.removeTab(tab);
});
