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
      Services.fog.testResetFOG();

      await Services.fog.testFlushAllChildren();
      Assert.equal(Glean.pdfjs.timeToView.testGetValue(), undefined);
      Assert.equal(Glean.pdfjs.used.testGetValue() || 0, 0);

      // check that PDF is opened with internal viewer
      await waitForPdfJSCanvas(browser, TESTROOT + "file_pdfjs_test.pdf");

      await Services.fog.testFlushAllChildren();
      Assert.ok(Glean.pdfjs.timeToView.testGetValue().sum !== 0);
      Assert.equal(Glean.pdfjs.used.testGetValue(), 1);

      await SpecialPowers.spawn(browser, [], async function () {
        var viewer = content.wrappedJSObject.PDFViewerApplication;
        await viewer.close();
      });
    }
  );
});
