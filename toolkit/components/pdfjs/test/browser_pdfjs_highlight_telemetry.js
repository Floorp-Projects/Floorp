/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "https://example.com/browser/" + RELATIVE_DIR;

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
        set: [
          ["pdfjs.annotationEditorMode", 0],
          ["pdfjs.enableHighlight", true],
        ],
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
        (Glean.pdfjsEditingHighlight.kind.freeHighlight.testGetValue() || 0) +
          (Glean.pdfjsEditingHighlight.kind.highlight.testGetValue() || 0),
        0,
        "Should have no highlight"
      );

      await enableEditor(browser, "Highlight");
      const strs = ["In production", "buildbot automation"];
      for (let i = 0; i < strs.length; i++) {
        const str = strs[i];
        const N = i + 1;
        const spanBox = await getSpanBox(browser, str);
        await clickAt(
          browser,
          spanBox.x + 0.75 * spanBox.width,
          spanBox.y + 0.5 * spanBox.height,
          2
        );
        await waitForEditors(browser, ".highlightEditor", N);
        await Services.fog.testFlushAllChildren();

        Assert.equal(
          Glean.pdfjsEditingHighlight.kind.highlight.testGetValue(),
          N,
          `Should have ${N} highlights`
        );
        Assert.equal(
          Glean.pdfjsEditingHighlight.color.yellow.testGetValue(),
          N,
          `Should have ${N} yellow highlights`
        );
        Assert.equal(
          Glean.pdfjsEditingHighlight.method.main_toolbar.testGetValue(),
          N,
          `Should have ${N} highlights created from the toolbar`
        );

        await EventUtils.synthesizeKey("KEY_Escape");
        await waitForSelector(browser, ".highlightEditor:not(.selectedEditor)");

        document.getElementById("cmd_print").doCommand();
        await BrowserTestUtils.waitForCondition(() => {
          let preview = document.querySelector(".printPreviewBrowser");
          return preview && BrowserTestUtils.isVisible(preview);
        });
        await EventUtils.synthesizeKey("KEY_Escape");

        await Services.fog.testFlushAllChildren();

        Assert.equal(Glean.pdfjs.editing.print.testGetValue(), N);
        Assert.equal(Glean.pdfjsEditingHighlight.print.testGetValue(), N);
        Assert.equal(
          Glean.pdfjsEditingHighlight.numberOfColors.one.testGetValue(),
          N
        );
      }

      await click(
        browser,
        "#highlightParamsToolbarContainer button[title='Green']"
      );
      const spanBox = await getSpanBox(browser, "Mozilla automated testing");
      await BrowserTestUtils.synthesizeMouseAtPoint(
        spanBox.x - 10,
        spanBox.y + spanBox.height / 2,
        {
          type: "mousedown",
          button: 0,
        },
        browser
      );
      await BrowserTestUtils.synthesizeMouseAtPoint(
        spanBox.x + spanBox.width,
        spanBox.y + spanBox.height / 2,
        {
          type: "mousemove",
          button: 0,
        },
        browser
      );
      await BrowserTestUtils.synthesizeMouseAtPoint(
        spanBox.x + spanBox.width,
        spanBox.y + spanBox.height / 2,
        {
          type: "mouseup",
          button: 0,
        },
        browser
      );
      await waitForEditors(browser, ".highlightEditor", 3);

      await Services.fog.testFlushAllChildren();
      Assert.equal(Glean.pdfjsEditingHighlight.color.green.testGetValue(), 1);
      Assert.equal(
        Glean.pdfjsEditingHighlight.kind.free_highlight.testGetValue(),
        1
      );

      let telemetryPromise = waitForTelemetry(browser);
      await focus(browser, "#editorFreeHighlightThickness");
      await EventUtils.synthesizeKey("KEY_ArrowRight");

      await telemetryPromise;

      await Services.fog.testFlushAllChildren();
      Assert.equal(
        Glean.pdfjsEditingHighlight.thickness.testGetValue().values[12],
        1
      );
      Assert.equal(
        Glean.pdfjsEditingHighlight.thickness.testGetValue().values[13],
        1
      );
      Assert.equal(
        Glean.pdfjsEditingHighlight.thicknessChanged.testGetValue(),
        1
      );

      document.getElementById("cmd_print").doCommand();
      await BrowserTestUtils.waitForCondition(() => {
        let preview = document.querySelector(".printPreviewBrowser");
        return preview && BrowserTestUtils.isVisible(preview);
      });
      await EventUtils.synthesizeKey("KEY_Escape");

      await Services.fog.testFlushAllChildren();

      Assert.equal(Glean.pdfjs.editing.print.testGetValue(), 3);
      Assert.equal(Glean.pdfjsEditingHighlight.print.testGetValue(), 3);
      Assert.equal(
        Glean.pdfjsEditingHighlight.numberOfColors.one.testGetValue(),
        2
      );
      Assert.equal(
        Glean.pdfjsEditingHighlight.numberOfColors.two.testGetValue(),
        1
      );

      await click(browser, ".highlightEditor.free button.colorPicker");
      telemetryPromise = waitForTelemetry(browser);
      await click(
        browser,
        ".highlightEditor.free button.colorPicker button[title='Red']"
      );
      await telemetryPromise;

      await Services.fog.testFlushAllChildren();
      Assert.equal(Glean.pdfjsEditingHighlight.colorChanged.testGetValue(), 1);

      document.getElementById("cmd_print").doCommand();
      await BrowserTestUtils.waitForCondition(() => {
        let preview = document.querySelector(".printPreviewBrowser");
        return preview && BrowserTestUtils.isVisible(preview);
      });
      await EventUtils.synthesizeKey("KEY_Escape");

      await Services.fog.testFlushAllChildren();
      Assert.equal(
        Glean.pdfjsEditingHighlight.numberOfColors.one.testGetValue(),
        2
      );
      Assert.equal(
        Glean.pdfjsEditingHighlight.numberOfColors.two.testGetValue(),
        2
      );

      telemetryPromise = waitForTelemetry(browser);
      await EventUtils.synthesizeKey("KEY_Delete");
      await telemetryPromise;

      await Services.fog.testFlushAllChildren();
      Assert.equal(Glean.pdfjsEditingHighlight.deleted.testGetValue(), 1);

      await SpecialPowers.spawn(browser, [], async function () {
        const viewer = content.wrappedJSObject.PDFViewerApplication;
        viewer.pdfDocument.annotationStorage.resetModified();
        await viewer.close();
      });
    }
  );
});
