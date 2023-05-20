/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "https://example.com/browser/" + RELATIVE_DIR;

/**
 * Test that the pdf doesn't use the minimum font size.
 */
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

  await SpecialPowers.pushPrefEnv({
    set: [["font.minimum-size.x-western", 56]],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      // check that PDF is opened with internal viewer
      await waitForPdfJSAllLayers(
        browser,
        `${TESTROOT}file_pdfjs_test.pdf#zoom=10`,
        []
      );

      await SpecialPowers.spawn(browser, [true], () => {
        const pageNumberInput = content.document.querySelector("#pageNumber");
        const { style } = pageNumberInput;
        const baseHeight =
          content.document.defaultView.getComputedStyle(pageNumberInput).height;

        style.fontSize = "1px";
        const height =
          content.document.defaultView.getComputedStyle(pageNumberInput).height;

        // Changing the font size should change the height of the input
        // element. If the minimum font size is used, the height won't change.
        Assert.notEqual(
          baseHeight,
          height,
          "The pdf viewer musn't use the minimum font size."
        );
      });

      await SpecialPowers.spawn(browser, [], async function () {
        var viewer = content.wrappedJSObject.PDFViewerApplication;
        await viewer.close();
      });
    }
  );
});
