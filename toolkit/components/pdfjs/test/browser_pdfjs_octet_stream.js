/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TESTROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "http://mochi.test:8888/"
);

// Get a ref to the pdf we want to open.
const PDF_URL = TESTROOT + "file_pdfjs_object_stream.pdf";

var gMIMEService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);

/**
 * Check that if we open a PDF with octet-stream mimetype, it can load
 * PDF.js .
 */
add_task(async function test_octet_stream_opens_pdfjs() {
  await SpecialPowers.pushPrefEnv({ set: [["pdfjs.handleOctetStream", true]] });
  let handlerInfo = gMIMEService.getFromTypeAndExtension(
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

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function(newTabBrowser) {
      await waitForPdfJS(newTabBrowser, PDF_URL);
      is(newTabBrowser.currentURI.spec, PDF_URL, "Should load pdfjs");
    }
  );
});

/**
 * Check that if the octet-stream thing is in a frame, we don't load it inside
 * PDF.js
 */
add_task(async function test_octet_stream_in_frame_downloads() {
  await SpecialPowers.pushPrefEnv({ set: [["pdfjs.handleOctetStream", true]] });

  let handlerInfo = gMIMEService.getFromTypeAndExtension(
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

  let dialogPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: `data:text/html,<iframe src='${PDF_URL}'>` },
    async function(newTabBrowser) {
      let dialogWin = await dialogPromise;
      ok(dialogWin, "Should have a dialog asking what to do.");
      dialogWin.close(); // This is going to cancel the dialog.
    }
  );
});
