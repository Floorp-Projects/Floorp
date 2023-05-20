/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "https://example.com/browser/" + RELATIVE_DIR;

/**
 * Get the number of red pixels in the canvas.
 * @param {Object} browser
 * @returns {Object}
 */
async function getRedPixels(browser) {
  return SpecialPowers.spawn(browser, [], async function () {
    const { document } = content;
    const canvas = document.querySelector("canvas");

    Assert.ok(!!canvas, "We must have a canvas");

    const data = canvas
      .getContext("2d")
      .getImageData(0, 0, canvas.width, canvas.height).data;

    let redPixels = 0;
    let total = 0;

    for (let i = 0, ii = data.length; i < ii; i += 4) {
      const R = data[i];
      const G = data[i + 1];
      const B = data[i + 2];

      if (R > 128 && R > 4 * G && R > 4 * B) {
        redPixels += 1;
      }
      total += 1;
    }

    return [redPixels, total];
  });
}

/**
 * Test that the pdf has the correct color thanks to the SVG filters.
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
    async function (browser) {
      // check that PDF is opened with internal viewer
      await waitForPdfJSCanvas(
        browser,
        `${TESTROOT}file_pdfjs_transfer_map.pdf#zoom=100`
      );

      const [redPixels, total] = await getRedPixels(browser);
      Assert.ok(
        redPixels / total >= 0.1,
        `Not enough red pixels: only ${redPixels} / ${total} red pixels!`
      );
    }
  );
});
