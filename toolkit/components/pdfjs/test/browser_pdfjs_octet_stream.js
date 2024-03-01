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
    async function (newTabBrowser) {
      await waitForPdfJS(newTabBrowser, PDF_URL);
      is(newTabBrowser.currentURI.spec, PDF_URL, "Should load pdfjs");
    }
  );
});

/**
 * Check that if the octet-stream thing is in a frame, we don't load it inside PDF.js
 */
add_task(async function test_octet_stream_in_frame() {
  await SpecialPowers.pushPrefEnv({
    set: [["pdfjs.handleOctetStream", true]],
  });

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

  let downloadsPanelPromise = BrowserTestUtils.waitForEvent(
    DownloadsPanel.panel,
    "popupshown"
  );

  // Once downloaded, the PDF will be opened as a file:// URI in a new tab
  let previewTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    url => {
      let uri = NetUtil.newURI(url);
      return uri.scheme == "file" && uri.spec.endsWith(".pdf");
    },
    false, // dont wait for load
    true // any tab, not just the next one
  );

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: `data:text/html,<iframe src='${PDF_URL}'>` },
    async function (newTabBrowser) {
      // wait until downloadsPanel opens before continuing with test
      info("Waiting for download panel to open");
      await downloadsPanelPromise;
      is(
        DownloadsPanel.panel.state,
        "open",
        "Check the download panel state is 'open'"
      );
      let downloadList = await Downloads.getList(Downloads.PUBLIC);
      let [download] = downloadList._downloads;

      // Verify the downloaded PDF opened in a new tab,
      // with its download file URI
      info("Waiting for preview tab");
      let previewTab = await previewTabPromise;
      ok(previewTab, "PDF opened in a new tab");

      is(DownloadsPanel.isPanelShowing, true, "DownloadsPanel should be open.");
      is(
        downloadList._downloads.length,
        1,
        "File should be successfully downloaded."
      );

      await BrowserTestUtils.removeTab(previewTab);

      info("cleaning up downloads");
      try {
        if (Services.appinfo.OS === "WINNT") {
          // We need to make the file writable to delete it on Windows.
          await IOUtils.setPermissions(download.target.path, 0o600);
        }
        await IOUtils.remove(download.target.path);
      } catch (error) {
        info("The file " + download.target.path + " is not removed, " + error);
      }
      await downloadList.remove(download);
      await download.finalize();

      if (DownloadsPanel.panel.state !== "closed") {
        let hiddenPromise = BrowserTestUtils.waitForEvent(
          DownloadsPanel.panel,
          "popuphidden"
        );
        DownloadsPanel.hidePanel();
        await hiddenPromise;
      }
      is(
        DownloadsPanel.panel.state,
        "closed",
        "Check that the download panel is closed"
      );
    }
  );
});
