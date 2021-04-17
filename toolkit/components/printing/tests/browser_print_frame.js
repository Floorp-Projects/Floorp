/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const frameSource = "<a href='about:mozilla'>some text</a>";
const SOURCES = [
  `Something else <iframe id="f" srcdoc="${frameSource}"></iframe>`,
  `Something else <iframe id="f" src="https://example.com/document-builder.sjs?html=${frameSource}"></iframe>`,
];

async function getPreviewText(previewBrowser) {
  return SpecialPowers.spawn(previewBrowser, [], function() {
    return content.document.body.textContent;
  });
}

add_task(async function print_frame() {
  let i = 0;
  for (const source of SOURCES) {
    // Testing the native print dialog is much harder.
    // Note we need to do this from here since resetPrintPrefs() below clears
    // out the pref.
    await SpecialPowers.pushPrefEnv({
      set: [["print.tab_modal.enabled", true]],
    });

    is(
      document.querySelector(".printPreviewBrowser"),
      null,
      "There shouldn't be any print preview browser"
    );

    await BrowserTestUtils.withNewTab(
      "data:text/html," + source,
      async function(browser) {
        let frameBC = browser.browsingContext.children[0];
        let helper = new PrintHelper(browser);

        // If you change this, change nsContextMenu.printFrame() too.
        PrintUtils.startPrintWindow(frameBC, {
          printFrameOnly: true,
        });

        await helper.waitForDialog({ waitFor: "loadComplete" });

        let previewBrowser = document.querySelector(".printPreviewBrowser");
        let previewText = () => getPreviewText(previewBrowser);
        // The preview process is async, wait for it to not be empty.
        let textContent = await TestUtils.waitForCondition(previewText);
        is(textContent, "some text", "Correct content loaded");

        let file = helper.mockFilePicker(`browser_print_frame-${i++}.pdf`);
        await helper.assertPrintToFile(file, () => {
          helper.click(helper.get("print-button"));
        });
        PrintHelper.resetPrintPrefs();
      }
    );
  }
});
