/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "http://example.com/browser/" + RELATIVE_DIR;

// Test telemetry.
add_task(async function test() {
  let mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  let handlerInfo = mimeService.getFromTypeAndExtension(
    "application/pdf",
    "pdf"
  );

  // Make sure pdf.js is the default handler.
  is(
    handlerInfo.alwaysAskBeforeHandling,
    false,
    "pdf handler defaults to always-ask is false"
  );
  is(
    handlerInfo.preferredAction,
    Ci.nsIHandlerInfo.handleInternally,
    "pdf handler defaults to internal"
  );

  info("Pref action: " + handlerInfo.preferredAction);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function(browser) {
      await SpecialPowers.pushPrefEnv({
        set: [["pdfjs.annotationEditorMode", 0]],
      });

      Services.fog.testResetFOG();

      // check that PDF is opened with internal viewer
      await waitForPdfJSAllLayers(browser, TESTROOT + "file_pdfjs_test.pdf", [
        [
          "annotationEditorLayer",
          "annotationLayer",
          "textLayer",
          "canvasWrapper",
        ],
        ["annotationEditorLayer", "textLayer", "canvasWrapper"],
      ]);

      let spanBox = await getSpanBox(browser, "and found references");

      await Services.fog.testFlushAllChildren();
      Assert.equal(Glean.pdfjs.editing.freetext.testGetValue() || 0, 0);

      await enableEditor(browser, "FreeText");
      await addFreeText(browser, "hello", spanBox);

      await BrowserTestUtils.waitForCondition(
        async () => (await countElements(browser, ".freeTextEditor")) !== 0
      );
      Assert.equal(await countElements(browser, ".freeTextEditor"), 1);

      await Services.fog.testFlushAllChildren();

      Assert.equal(Glean.pdfjs.editing.freetext.testGetValue(), 1);

      spanBox = await getSpanBox(browser, "forums and ask questions");
      await addFreeText(browser, "world", spanBox);

      await BrowserTestUtils.waitForCondition(
        async () => (await countElements(browser, ".freeTextEditor")) !== 1
      );
      Assert.equal(await countElements(browser, ".freeTextEditor"), 2);

      await Services.fog.testFlushAllChildren();

      Assert.equal(Glean.pdfjs.editing.freetext.testGetValue(), 2);

      Assert.equal(Glean.pdfjs.editing.print.testGetValue() || 0, 0);
      document.getElementById("cmd_print").doCommand();
      await BrowserTestUtils.waitForCondition(() => {
        let preview = document.querySelector(".printPreviewBrowser");
        return preview && BrowserTestUtils.is_visible(preview);
      });
      EventUtils.synthesizeKey("KEY_Escape");

      await Services.fog.testFlushAllChildren();

      Assert.equal(Glean.pdfjs.editing.print.testGetValue(), 1);

      await SpecialPowers.spawn(browser, [], async function() {
        var viewer = content.wrappedJSObject.PDFViewerApplication;
        await viewer.close();
      });
    }
  );
});
