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
    async function(browser) {
      Services.fog.testResetFOG();

      // check that PDF is opened with internal viewer
      await waitForPdfJS(browser, TESTROOT + "file_pdfjs_test.pdf");

      await Services.fog.testFlushAllChildren();
      Assert.equal(Glean.pdfjs.buttons.cursor_hand_tool.testGetValue() || 0, 0);

      info("Clicking on the secondary toolbar button...");
      await SpecialPowers.spawn(browser, [], () => {
        content.document.querySelector("#secondaryToolbarToggle").click();
      });

      await BrowserTestUtils.waitForCondition(async () =>
        SpecialPowers.spawn(browser, [], async function() {
          return !content.document
            .querySelector("#secondaryToolbar")
            .classList.contains("hidden");
        })
      );

      info("Clicking on the cursor handtool button...");
      await SpecialPowers.spawn(browser, [], () => {
        content.document.querySelector("#cursorHandTool").click();
      });

      await Services.fog.testFlushAllChildren();
      Assert.equal(Glean.pdfjs.buttons.cursor_hand_tool.testGetValue(), 1);

      await SpecialPowers.spawn(browser, [], async function() {
        var viewer = content.wrappedJSObject.PDFViewerApplication;
        await viewer.close();
      });
    }
  );
});
