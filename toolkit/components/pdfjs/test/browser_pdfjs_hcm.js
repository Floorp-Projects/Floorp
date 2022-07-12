/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "http://example.com/browser/" + RELATIVE_DIR;

/**
 * Get the first and last pixels on the drawn canvas.
 * @param {Object} browser
 * @returns {Object}
 */
async function getFirstLastPixels(browser) {
  return SpecialPowers.spawn(browser, [], async function() {
    const { document } = content;
    const canvas = document.querySelector("canvas");

    Assert.ok(!!canvas, "We must have a canvas");

    const data = Array.from(
      canvas.getContext("2d").getImageData(0, 0, canvas.width, canvas.height)
        .data
    );

    return {
      first: data.slice(0, 3),
      last: data.slice(-4, -1),
    };
  });
}

/**
 * Test that the pdf has the correct color depending on the high contrast mode.
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
    set: [
      ["browser.display.document_color_use", 0],
      ["browser.display.background_color", "#ff0000"],
      ["browser.display.foreground_color", "#00ff00"],
      ["browser.display.use_system_colors", false],
    ],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function(browser) {
      // check that PDF is opened with internal viewer
      await waitForPdfJSCanvas(
        browser,
        `${TESTROOT}file_pdfjs_hcm.pdf#zoom=800`
      );

      const { first, last } = await getFirstLastPixels(browser);
      // The first pixel must be black and not green.
      Assert.deepEqual(first, [0, 0, 0]);

      // The last pixel must be white and not red.
      Assert.deepEqual(last, [255, 255, 255]);
    }
  );

  // Enable HCM.
  await SpecialPowers.pushPrefEnv({
    set: [["ui.useAccessibilityTheme", 1]],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function(browser) {
      // check that PDF is opened with internal viewer
      await waitForPdfJSCanvas(
        browser,
        `${TESTROOT}file_pdfjs_hcm.pdf#zoom=800`
      );

      const { first, last } = await getFirstLastPixels(browser);
      // The first pixel must be green.
      Assert.deepEqual(first, [0, 255, 0]);

      // The last pixel must be red.
      Assert.deepEqual(last, [255, 0, 0]);
    }
  );
});
