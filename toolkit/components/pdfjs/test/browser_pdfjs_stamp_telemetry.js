/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "https://example.com/browser/" + RELATIVE_DIR;

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
  this
);

const MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);
MockFilePicker.returnValue = MockFilePicker.returnOK;
const file = new FileUtils.File(getTestFilePath("moz.png"));
MockFilePicker.setFiles([file]);

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
    async function (browser) {
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

      await Services.fog.testFlushAllChildren();
      Assert.equal(
        Glean.pdfjs.editing.stamp.testGetValue() || 0,
        0,
        "Should have no stamp"
      );

      await enableEditor(browser, "Stamp");
      await clickOn(browser, `#editorStampAddImage`);
      await waitForSelector(browser, ".altText");

      await Services.fog.testFlushAllChildren();

      Assert.equal(
        Glean.pdfjs.editing.stamp.testGetValue(),
        1,
        "Should have 1 stamp"
      );
      Assert.equal(
        Glean.pdfjs.stamp.inserted_image.testGetValue(),
        1,
        "Should have 1 inserted_image"
      );

      await clickOn(browser, ".altText");
      await waitForSelector(
        browser,
        "#altTextDialog",
        "Wait for the dialog to be visible"
      );

      await clickOn(browser, "#descriptionTextarea");
      await write(browser, "Hello World");

      await clickOn(browser, "#altTextSave");
      await TestUtils.waitForTick();
      await Services.fog.testFlushAllChildren();

      Assert.equal(
        Glean.pdfjs.stamp.alt_text_save.testGetValue(),
        1,
        "Should have 1 alt_text_save"
      );
      Assert.equal(
        Glean.pdfjs.stamp.alt_text_description.testGetValue(),
        1,
        "Should have 1 alt_text_description"
      );

      await SpecialPowers.spawn(browser, [], async () => {
        const altText = content.document.querySelector(".altText");
        await EventUtils.synthesizeMouseAtCenter(
          altText,
          { type: "mousemove" },
          content
        );
        await ContentTaskUtils.waitForCondition(
          () => ContentTaskUtils.isVisible(altText.querySelector(".tooltip")),
          "Wait for tooltip"
        );
      });

      await TestUtils.waitForTick();
      await Services.fog.testFlushAllChildren();
      Assert.equal(
        Glean.pdfjs.stamp.alt_text_tooltip.testGetValue(),
        1,
        "Should have 1 alt_text_tooltip"
      );

      await clickOn(browser, ".altText");
      await waitForSelector(
        browser,
        "#altTextDialog",
        "Wait for the dialog to be visible"
      );
      await clickOn(browser, "#altTextCancel");
      await TestUtils.waitForTick();

      await Services.fog.testFlushAllChildren();

      Assert.equal(
        Glean.pdfjs.stamp.alt_text_cancel.testGetValue(),
        1,
        "Should have 1 alt_text_cancel"
      );

      await clickOn(browser, ".altText");
      await waitForSelector(
        browser,
        "#altTextDialog",
        "Wait for the dialog to be visible"
      );
      await clickOn(browser, "#descriptionTextarea");
      await write(browser, "Hello World");

      await clickOn(browser, "#altTextSave");
      await TestUtils.waitForTick();
      await Services.fog.testFlushAllChildren();

      Assert.equal(
        Glean.pdfjs.stamp.alt_text_save.testGetValue(),
        2,
        "Should have 2 alt_text_save"
      );
      Assert.equal(
        Glean.pdfjs.stamp.alt_text_description.testGetValue(),
        2,
        "Should have 2 alt_text_description"
      );
      Assert.equal(
        Glean.pdfjs.stamp.alt_text_edit.testGetValue(),
        1,
        "Should have 1 alt_text_edit"
      );

      await clickOn(browser, ".altText");
      await waitForSelector(
        browser,
        "#altTextDialog",
        "Wait for the dialog to be visible"
      );
      await clickOn(browser, "#decorativeButton");
      await TestUtils.waitForTick();
      await clickOn(browser, "#altTextSave");
      await TestUtils.waitForTick();
      await Services.fog.testFlushAllChildren();

      Assert.equal(
        Glean.pdfjs.stamp.alt_text_decorative.testGetValue(),
        1,
        "Should have 1 alt_text_decorative"
      );
      Assert.equal(
        Glean.pdfjs.stamp.alt_text_keyboard.testGetValue() || 0,
        0,
        "Should have 0 alt_text_keyboard"
      );

      await click(browser, ".altText");
      await click(browser, "#descriptionButton");
      await click(browser, "#altTextSave");

      await TestUtils.waitForTick();
      await Services.fog.testFlushAllChildren();

      Assert.equal(
        Glean.pdfjs.stamp.alt_text_keyboard.testGetValue(),
        1,
        "Should have 1 alt_text_keyboard"
      );

      await SpecialPowers.spawn(browser, [], async function () {
        const viewer = content.wrappedJSObject.PDFViewerApplication;
        viewer.pdfDocument.annotationStorage.resetModified();
        await viewer.close();
      });
    }
  );
});
