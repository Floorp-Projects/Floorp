/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "https://example.com/browser/" + RELATIVE_DIR;

function waitForFullScreenState(browser, state) {
  return new Promise(resolve => {
    let eventReceived = false;

    let observe = (subject, topic, data) => {
      if (!eventReceived) {
        return;
      }
      Services.obs.removeObserver(observe, "fullscreen-painted");
      resolve();
    };
    Services.obs.addObserver(observe, "fullscreen-painted");

    browser.ownerGlobal.addEventListener(
      `MozDOMFullscreen:${state ? "Entered" : "Exited"}`,
      () => {
        eventReceived = true;
      },
      { once: true }
    );
  });
}

/**
 * Test that we can enter in presentation mode in using the keyboard shortcut
 * without having to interact with the pdf.
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

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function(browser) {
      // check that PDF is opened with internal viewer
      await waitForPdfJSCanvas(browser, `${TESTROOT}file_pdfjs_test.pdf`);

      const fullscreenPromise = waitForFullScreenState(browser, true);

      await SpecialPowers.spawn(browser, [], async function() {
        const { ContentTaskUtils } = ChromeUtils.import(
          "resource://testing-common/ContentTaskUtils.jsm"
        );
        const EventUtils = ContentTaskUtils.getEventUtils(content);
        await EventUtils.synthesizeKey(
          "p",
          { ctrlKey: true, altKey: true },
          content
        );
      });

      await fullscreenPromise;
      ok(true, "Should be in fullscreen");

      await SpecialPowers.spawn(browser, [], async function() {
        const viewer = content.wrappedJSObject.PDFViewerApplication;
        await viewer.close();
      });
    }
  );
});
