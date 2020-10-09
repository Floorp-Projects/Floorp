/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const frameSource =
  "<a href='about:mozilla'>some text</a><a id='other' href='about:about'>other text</a>";
const sources = [
  `<html><iframe id="f" srcdoc="${frameSource}"></iframe></html>`,
  `<html><iframe id="f" src="https://example.com/document-builder.sjs?html=${frameSource}"></iframe></html>`,
];

add_task(async function print_selection() {
  // Testing the native print dialog is much harder.
  await SpecialPowers.pushPrefEnv({
    set: [["print.tab_modal.enabled", true]],
  });

  for (let source of sources) {
    is(
      document.querySelector(".printPreviewBrowser"),
      null,
      "There shouldn't be any print preview browser"
    );

    await BrowserTestUtils.withNewTab(
      "data:text/html," + source,
      async function(browser) {
        let frameBC = browser.browsingContext.children[0];
        await SpecialPowers.spawn(frameBC, [], () => {
          let element = content.document.getElementById("other");
          content.focus();
          content.getSelection().selectAllChildren(element);
        });

        // If you change this, change nsContextMenu.printSelection() too.
        PrintUtils.startPrintWindow(
          "tests",
          frameBC,
          null,
          /* aSelectionOnly = */ true
        );

        await BrowserTestUtils.waitForCondition(
          () => !!document.querySelector(".printPreviewBrowser")
        );

        let previewBrowser = document.querySelector(".printPreviewBrowser");
        async function getPreviewText() {
          return SpecialPowers.spawn(previewBrowser, [], function() {
            return content.document.body.textContent;
          });
        }
        // The preview process is async, wait for it to not be empty.
        await BrowserTestUtils.waitForCondition(getPreviewText);
        let textContent = await getPreviewText();
        is(textContent, "other text", "Correct content loaded");
        // Closing the tab also closes the preview dialog and such.
      }
    );
  }
});
