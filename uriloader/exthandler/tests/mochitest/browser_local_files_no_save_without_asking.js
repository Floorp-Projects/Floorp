/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that loading a local PDF file
 * prompts the user when pdfjs.disabled is set to true,
 * and alwaysAsk is false;
 */
add_task(
  async function test_check_browser_local_files_no_save_without_asking() {
    // Get a ref to the pdf we want to open.
    let file = getChromeDir(getResolvedURI(gTestPath));
    file.append("file_pdf_binary_octet_stream.pdf");

    await SpecialPowers.pushPrefEnv({ set: [["pdfjs.disabled", true]] });

    const mimeSvc = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
    const handlerSvc = Cc[
      "@mozilla.org/uriloader/handler-service;1"
    ].getService(Ci.nsIHandlerService);
    const mimeInfo = mimeSvc.getFromTypeAndExtension("application/pdf", "pdf");
    // This test covers a bug that only occurs when the mimeInfo is set to Always Ask = false
    // Here we check if we ask the user what to do for local files, if the file is set to save to disk automatically;
    // that is, we check that we prompt the user despite the user's preference.
    mimeInfo.preferredAction = mimeInfo.saveToDisk;
    mimeInfo.alwaysAskBeforeHandling = false;
    handlerSvc.store(mimeInfo);

    info("Testing with " + file.path);
    let publicList = await Downloads.getList(Downloads.PUBLIC);
    registerCleanupFunction(async () => {
      await publicList.removeFinished();
    });

    let publicDownloads = await publicList.getAll();
    is(
      publicDownloads.length,
      0,
      "download should not appear in publicDownloads list"
    );

    let dialogWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
    var loadingTab = BrowserTestUtils.addTab(gBrowser, file.path);

    let dialogWindow = await dialogWindowPromise;

    is(
      dialogWindow.location.href,
      "chrome://mozapps/content/downloads/unknownContentType.xhtml",
      "Should have seen the unknown content dialogWindow."
    );

    let doc = dialogWindow.document;

    let dialog = doc.querySelector("#unknownContentType");
    dialog.cancelDialog();
    BrowserTestUtils.removeTab(loadingTab);
  }
);
