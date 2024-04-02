/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "https://example.com/browser/" + RELATIVE_DIR;
const pdfUrl = TESTROOT + "file_pdfjs_test.pdf";
const caretBrowsingModePref = "accessibility.browsewithcaret";

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
    async function test_caret_browsing_mode(browser) {
      await waitForPdfJS(browser, pdfUrl);

      let promise = BrowserTestUtils.waitForContentEvent(
        browser,
        "updatedPreference",
        false,
        null,
        true
      );
      await SpecialPowers.pushPrefEnv({
        set: [[caretBrowsingModePref, true]],
      });
      await promise;
      await TestUtils.waitForTick();

      await SpecialPowers.spawn(browser, [], async function () {
        const viewer = content.wrappedJSObject.PDFViewerApplication;
        Assert.ok(
          viewer.supportsCaretBrowsingMode,
          "Caret browsing mode is supported"
        );
      });

      promise = BrowserTestUtils.waitForContentEvent(
        browser,
        "updatedPreference",
        false,
        null,
        true
      );
      await SpecialPowers.popPrefEnv();
      await promise;
      await TestUtils.waitForTick();

      await SpecialPowers.spawn(browser, [], async function () {
        const viewer = content.wrappedJSObject.PDFViewerApplication;
        Assert.ok(
          !viewer.supportsCaretBrowsingMode,
          "Caret browsing mode isn't supported"
        );
      });

      await SpecialPowers.spawn(browser, [], async function () {
        const viewer = content.wrappedJSObject.PDFViewerApplication;
        await viewer.close();
      });
    }
  );
});
