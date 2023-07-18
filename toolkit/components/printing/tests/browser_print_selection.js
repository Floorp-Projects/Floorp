/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const frameSource =
  "<a href='about:mozilla'>some text</a><a id='other' href='about:about'>other text</a>";
const sources = [
  `<html><iframe id="f" srcdoc="${frameSource}"></iframe></html>`,
  `<html><iframe id="f" src="https://example.com/document-builder.sjs?html=${frameSource}"></iframe></html>`,
];

async function getPreviewText(previewBrowser) {
  return SpecialPowers.spawn(previewBrowser, [], function () {
    return content.document.body.textContent;
  });
}

add_task(async function print_selection() {
  let i = 0;
  for (let source of sources) {
    is(
      document.querySelector(".printPreviewBrowser"),
      null,
      "There shouldn't be any print preview browser"
    );

    await BrowserTestUtils.withNewTab(
      "data:text/html," + source,
      async function (browser) {
        let frameBC = browser.browsingContext.children[0];
        await SpecialPowers.spawn(frameBC, [], () => {
          let element = content.document.getElementById("other");
          content.focus();
          content.getSelection().selectAllChildren(element);
        });

        let helper = new PrintHelper(browser);

        // If you change this, change nsContextMenu.printSelection() too.
        PrintUtils.startPrintWindow(frameBC, {
          printSelectionOnly: true,
        });

        await waitForPreviewVisible();

        let previewBrowser = document.querySelector(
          ".printPreviewBrowser[previewtype='selection']"
        );
        let previewText = () => getPreviewText(previewBrowser);
        // The preview process is async, wait for it to not be empty.
        let textContent = await TestUtils.waitForCondition(previewText);
        is(textContent, "other text", "Correct content loaded");

        let printSelect = document
          .querySelector(".printSettingsBrowser")
          .contentDocument.querySelector("#source-version-selection-radio");
        ok(
          BrowserTestUtils.is_visible(printSelect),
          "Print selection checkbox is shown"
        );
        ok(printSelect.checked, "Print selection checkbox is checked");

        let file = helper.mockFilePicker(`browser_print_selection-${i++}.pdf`);
        await helper.assertPrintToFile(file, () => {
          helper.click(helper.get("print-button"));
        });
        PrintHelper.resetPrintPrefs();
      }
    );
  }
});

add_task(async function print_selection_parent_process() {
  is(
    document.querySelector(".printPreviewBrowser"),
    null,
    "There shouldn't be any print preview browser"
  );

  await BrowserTestUtils.withNewTab("about:support", async function (browser) {
    ok(!browser.isRemoteBrowser, "Page loaded in parent process");
    let selectedText = await SpecialPowers.spawn(
      browser.browsingContext,
      [],
      () => {
        let element = content.document.querySelector("h1");
        content.focus();
        content.getSelection().selectAllChildren(element);
        return element.textContent;
      }
    );
    ok(selectedText, "There is selected text");

    let helper = new PrintHelper(browser);

    // If you change this, change nsContextMenu.printSelection() too.
    PrintUtils.startPrintWindow(browser.browsingContext, {
      printSelectionOnly: true,
    });

    await waitForPreviewVisible();

    let previewBrowser = document.querySelector(
      ".printPreviewBrowser[previewtype='selection']"
    );
    let previewText = () => getPreviewText(previewBrowser);
    // The preview process is async, wait for it to not be empty.
    let textContent = await TestUtils.waitForCondition(previewText);
    is(textContent, selectedText, "Correct content loaded");

    let printSelect = document
      .querySelector(".printSettingsBrowser")
      .contentDocument.querySelector("#source-version-selection-radio");
    ok(
      BrowserTestUtils.is_visible(printSelect),
      "Print selection checkbox is shown"
    );
    ok(printSelect.checked, "Print selection checkbox is checked");

    let file = helper.mockFilePicker(`browser_print_selection_parent.pdf`);
    await helper.assertPrintToFile(file, () => {
      helper.click(helper.get("print-button"));
    });
    PrintHelper.resetPrintPrefs();
  });
});

add_task(async function no_print_selection() {
  // Ensures the print selection checkbox is hidden if nothing is selected
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();
    await helper.openMoreSettings();

    let printSelect = helper.get("source-version-selection");
    ok(
      BrowserTestUtils.is_hidden(printSelect),
      "Print selection checkbox is hidden"
    );
    await helper.closeDialog();
  });
});

add_task(async function print_selection_switch() {
  await PrintHelper.withTestPage(async helper => {
    await SpecialPowers.spawn(helper.sourceBrowser, [], async function () {
      let element = content.document.querySelector("h1");
      content.window.getSelection().selectAllChildren(element);
    });

    await helper.startPrint();
    await helper.openMoreSettings();
    let printSource = helper.get("source-version-source-radio");
    ok(printSource.checked, "Print source radio is checked");
    let printSelect = helper.get("source-version-selection-radio");
    ok(!printSelect.checked, "Print selection radio is not checked");

    function getCurrentBrowser(previewType) {
      let browser = document.querySelector(
        `.printPreviewBrowser[previewtype="${previewType}"]`
      );
      is(
        browser.parentElement.getAttribute("previewtype"),
        previewType,
        "Expected browser is showing"
      );
      return browser;
    }

    let selectedText = "Article title";
    let fullText = await getPreviewText(getCurrentBrowser("source"));

    helper.assertSettingsMatch({
      printSelectionOnly: false,
    });

    await helper.assertSettingsChanged(
      { printSelectionOnly: false },
      { printSelectionOnly: true },
      async () => {
        await helper.waitForPreview(() => helper.click(printSelect));
        let text = await getPreviewText(getCurrentBrowser("selection"));
        is(text, selectedText, "Correct content loaded");
      }
    );

    await helper.assertSettingsChanged(
      { printSelectionOnly: true },
      { printSelectionOnly: false },
      async () => {
        await helper.waitForPreview(() => helper.click(printSource));
        let text = await getPreviewText(getCurrentBrowser("source"));
        is(text, fullText, "Correct content loaded");
      }
    );

    await helper.closeDialog();
  });
});

add_task(async function open_system_print_with_selection_and_pdf() {
  await BrowserTestUtils.withNewTab(
    "data:text/html," + sources[0],
    async function (browser) {
      let frameBC = browser.browsingContext.children[0];
      await SpecialPowers.spawn(frameBC, [], () => {
        let element = content.document.getElementById("other");
        content.focus();
        content.getSelection().selectAllChildren(element);
      });

      let helper = new PrintHelper(browser);

      // Add another printer so the system dialog link is shown on Windows.
      helper.addMockPrinter("A printer");

      PrintUtils.startPrintWindow(frameBC, {});

      await waitForPreviewVisible();

      // Ensure that the PDF printer is selected since the way settings are
      // cloned is different in this case.
      is(
        helper.settings.printerName,
        "Mozilla Save to PDF",
        "Mozilla Save to PDF is the current printer."
      );

      await helper.setupMockPrint();

      helper.click(helper.get("open-dialog-link"));
      await helper.withClosingFn(() => {
        helper.resolveShowSystemDialog();
        helper.resolvePrint();
      });

      ok(
        helper.systemDialogOpenedWithSelection,
        "Expect system print dialog to be notified of selection"
      );

      PrintHelper.resetPrintPrefs();
    }
  );
});
