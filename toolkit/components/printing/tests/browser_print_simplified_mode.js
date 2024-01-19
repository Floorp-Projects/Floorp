/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function switch_print_preview_browsers() {
  await PrintHelper.withTestPage(async helper => {
    // Wait for the article state to be determined.
    await helper.waitForReaderModeReady();

    // Start print preview.
    await helper.startPrint();
    let sourcePreviewBrowser = helper.currentPrintPreviewBrowser;

    {
      // Assert that we are showing the initial content on default print preview browser
      let [headerText, headingText] = await SpecialPowers.spawn(
        sourcePreviewBrowser,
        [],
        async function () {
          return [
            content.document.querySelector("header").textContent,
            content.document.querySelector("h1").textContent,
          ];
        }
      );
      is(headerText, "Site header", "Should have initial content.");
      is(headingText, "Article title", "Should have initial title.");
    }

    // Here we call simplified mode
    await helper.openMoreSettings();
    let simplifyRadio = helper.get("source-version-simplified-radio");
    ok(!simplifyRadio.checked, "Simplify page is not checked");
    ok(BrowserTestUtils.isVisible(simplifyRadio), "Simplify is shown");

    await helper.waitForPreview(() => helper.click(simplifyRadio));
    let simplifiedPreviewBrowser = helper.currentPrintPreviewBrowser;
    is(
      simplifiedPreviewBrowser.getAttribute("previewtype"),
      "simplified",
      "Simplified browser was rendered"
    );
    is(
      simplifiedPreviewBrowser.closest("stack").getAttribute("previewtype"),
      "simplified",
      "Simplified browser is selected"
    );
    ok(
      BrowserTestUtils.isVisible(simplifiedPreviewBrowser),
      "Simplified browser is visible"
    );
    ok(simplifyRadio.checked, "Simplify page is checked");

    {
      // Assert that we are showing custom content on simplified print preview browser
      let [hasHeader, headingText] = await SpecialPowers.spawn(
        simplifiedPreviewBrowser,
        [],
        async function () {
          return [
            !!content.document.querySelector("header"),
            content.document.querySelector("h1").textContent,
          ];
        }
      );
      ok(!hasHeader, "The header was simplified out");
      is(headingText, "Article title", "The heading is still there");
    }

    // Switch back to default print preview content
    let sourceRadio = helper.get("source-version-source-radio");
    ok(!sourceRadio.checked, "Source is not checked");
    await helper.waitForPreview(() => helper.click(sourceRadio));
    is(
      helper.currentPrintPreviewBrowser,
      sourcePreviewBrowser,
      "Source browser was rendered"
    );
    is(
      sourcePreviewBrowser.getAttribute("previewtype"),
      "source",
      "Source browser was rendered"
    );
    is(
      sourcePreviewBrowser.closest("stack").getAttribute("previewtype"),
      "source",
      "Source browser is selected"
    );
    ok(
      BrowserTestUtils.isVisible(sourcePreviewBrowser),
      "Source browser is visible"
    );
    ok(sourceRadio.checked, "Source version is checked");

    {
      // Assert that we are showing the initial content on default print preview browser
      let headerText = await SpecialPowers.spawn(
        sourcePreviewBrowser,
        [],
        async function () {
          return content.document.querySelector("header").textContent;
        }
      );
      is(headerText, "Site header", "Should have initial content.");
    }

    await helper.closeDialog();
  }, "simplifyArticleSample.html");
});

add_task(async function testPrintBackgroundsDisabledSimplified() {
  await PrintHelper.withTestPage(async helper => {
    // Wait for the article state to be determined.
    await helper.waitForReaderModeReady();
    await helper.startPrint();

    helper.assertPreviewedWithSettings({
      printBGImages: false,
      printBGColors: false,
    });

    await helper.openMoreSettings();

    let printBackgrounds = helper.get("backgrounds-enabled");
    ok(!printBackgrounds.checked, "Print backgrounds is not checked");
    ok(!printBackgrounds.disabled, "Print backgrounds in not disabled");

    await helper.assertSettingsChanged(
      { printBGImages: false, printBGColors: false },
      { printBGImages: true, printBGColors: true },
      async () => {
        await helper.waitForPreview(() => helper.click(printBackgrounds));
      }
    );

    // Print backgrounds was enabled for preview.
    ok(printBackgrounds.checked, "Print backgrounds is checked");
    ok(!printBackgrounds.disabled, "Print backgrounds is not disabled");
    helper.assertPreviewedWithSettings({
      printBGImages: true,
      printBGColors: true,
    });

    let simplifyRadio = helper.get("source-version-simplified-radio");
    ok(!simplifyRadio.checked, "Simplify page is not checked");
    ok(BrowserTestUtils.isVisible(simplifyRadio), "Simplify is shown");

    // Switch to simplified mode.
    await helper.waitForPreview(() => helper.click(simplifyRadio));

    // Print backgrounds should be disabled, it's incompatible with simplified.
    ok(!printBackgrounds.checked, "Print backgrounds is now unchecked");
    ok(printBackgrounds.disabled, "Print backgrounds has been disabled");
    helper.assertPreviewedWithSettings({
      printBGImages: false,
      printBGColors: false,
    });

    // Switch back to source, printBackgrounds is remembered.
    let sourceRadio = helper.get("source-version-source-radio");
    ok(!sourceRadio.checked, "Source is not checked");
    ok(BrowserTestUtils.isVisible(sourceRadio), "Source is shown");

    await helper.waitForPreview(() => helper.click(sourceRadio));

    ok(printBackgrounds.checked, "Print backgrounds setting was remembered");
    ok(!printBackgrounds.disabled, "Print backgrounds can be changed again");
    helper.assertPreviewedWithSettings({
      printBGImages: true,
      printBGColors: true,
    });

    await helper.closeDialog();
  }, "simplifyArticleSample.html");
});

add_task(async function testSimplifyHiddenNonArticle() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();
    await helper.openMoreSettings();
    let sourceVersionSection = helper.get("source-version-section");
    ok(
      BrowserTestUtils.isHidden(sourceVersionSection),
      "Source version is hidden"
    );
    await helper.closeDialog();
  }, "simplifyNonArticleSample.html");
});

add_task(async function testSimplifyNonArticleTabModal() {
  await PrintHelper.withTestPage(async helper => {
    let tab = gBrowser.selectedTab;

    // Trick browser to think loaded tab has isArticle property set as true
    tab.linkedBrowser.isArticle = true;

    // Enter print preview
    await helper.startPrint();

    // Assert that we are showing the initial content on default print preview browser
    await SpecialPowers.spawn(
      helper.currentPrintPreviewBrowser,
      [],
      async () => {
        is(
          content.document.title,
          "Non article title",
          "Should have initial content."
        );
      }
    );

    await helper.openMoreSettings();

    // Simplify the page.
    let simplifyRadio = helper.get("source-version-simplified-radio");
    ok(!simplifyRadio.checked, "Simplify is off");
    await helper.waitForPreview(() => helper.click(simplifyRadio));
    let simplifiedPreviewBrowser = helper.currentPrintPreviewBrowser;
    is(
      simplifiedPreviewBrowser.getAttribute("previewtype"),
      "simplified",
      "The simplified browser is shown"
    );

    // Assert that simplify page option is checked
    ok(simplifyRadio.checked, "Should have simplify page option checked");

    // Assert that we are showing recovery content on simplified print preview browser
    await SpecialPowers.spawn(simplifiedPreviewBrowser, [], async () => {
      await ContentTaskUtils.waitForCondition(
        () => content.document.title === "Failed to load article from page",
        "Simplified document title should be updated with recovery title."
      );
    });

    await helper.closeDialog();
  }, "simplifyNonArticleSample.html");
});

add_task(async function testSimplifyHiddenReaderMode() {
  await PrintHelper.withTestPage(async helper => {
    let tab = gBrowser.selectedTab;

    // Trigger reader mode for the tab
    let readerButton = document.getElementById("reader-mode-button");
    await TestUtils.waitForCondition(() => !readerButton.hidden);
    readerButton.click();
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

    // Print from reader mode
    await helper.startPrint();
    await helper.openMoreSettings();
    let sourceVersionSection = helper.get("source-version-section");
    ok(
      BrowserTestUtils.isHidden(sourceVersionSection),
      "Source version is hidden in reader mode"
    );
    await helper.closeDialog();
  }, "simplifyArticleSample.html");
});
