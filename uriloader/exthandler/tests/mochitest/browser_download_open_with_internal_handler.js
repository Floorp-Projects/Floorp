/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://gre/modules/Downloads.jsm", this);

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

function waitForAcceptButtonToGetEnabled(doc) {
  let dialog = doc.querySelector("#unknownContentType");
  let button = dialog.getButton("accept");
  return TestUtils.waitForCondition(
    () => !button.disabled,
    "Wait for Accept button to get enabled"
  );
}

async function waitForPdfJS(browser, url) {
  await SpecialPowers.pushPrefEnv({
    set: [["pdfjs.eventBusDispatchToDOM", true]],
  });
  // Runs tests after all "load" event handlers have fired off
  let loadPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "documentloaded",
    false,
    null,
    true
  );
  await SpecialPowers.spawn(browser, [url], contentUrl => {
    content.location = contentUrl;
  });
  return loadPromise;
}

add_task(async function setup() {
  // Remove the security delay for the dialog during the test.
  await SpecialPowers.pushPrefEnv({
    set: [["security.dialog_enable_delay", 0]],
  });
});

/**
 * Check that loading a PDF file with content-disposition: attachment
 * shows an option to open with the internal handler, and that the
 * internal option handler is not present when the download button
 * is clicked from pdf.js.
 */
add_task(async function test_check_open_with_internal_handler() {
  const { DownloadIntegration } = ChromeUtils.import(
    "resource://gre/modules/DownloadIntegration.jsm"
  );
  for (let file of [
    "file_pdf_application_pdf.pdf",
    "file_pdf_binary_octet_stream.pdf",
  ]) {
    info("Testing with " + file);
    await SpecialPowers.pushPrefEnv({
      set: [["browser.helperApps.showOpenOptionForPdfJS", true]],
    });
    let publicList = await Downloads.getList(Downloads.PUBLIC);
    registerCleanupFunction(async () => {
      await publicList.removeFinished();
    });
    let dialogWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
    let loadingTab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TEST_PATH + file
    );
    // Add an extra tab after the loading tab so we can test that
    // pdf.js is opened in the adjacent tab and not at the end of
    // the tab strip.
    let extraTab = await BrowserTestUtils.addTab(gBrowser, "about:blank");
    let dialogWindow = await dialogWindowPromise;
    is(
      dialogWindow.location,
      "chrome://mozapps/content/downloads/unknownContentType.xhtml",
      "Should have seen the unknown content dialogWindow."
    );
    let doc = dialogWindow.document;
    let internalHandlerRadio = doc.querySelector("#handleInternally");

    await waitForAcceptButtonToGetEnabled(doc);

    ok(!internalHandlerRadio.hidden, "The option should be visible for PDF");
    ok(internalHandlerRadio.selected, "The option should be selected");

    let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
    let dialog = doc.querySelector("#unknownContentType");
    let button = dialog.getButton("accept");
    button.disabled = false;
    dialog.acceptDialog();
    info("waiting for new tab to open");
    let newTab = await newTabPromise;

    is(
      newTab._tPos - 1,
      loadingTab._tPos,
      "pdf.js should be opened in an adjacent tab"
    );

    await ContentTask.spawn(newTab.linkedBrowser, null, async () => {
      await ContentTaskUtils.waitForCondition(
        () => content.document.readyState == "complete"
      );
    });

    let publicDownloads = await publicList.getAll();
    is(
      publicDownloads.length,
      1,
      "download should appear in publicDownloads list"
    );
    let subdialogPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
    await SpecialPowers.spawn(newTab.linkedBrowser, [], async () => {
      let downloadButton;
      await ContentTaskUtils.waitForCondition(() => {
        downloadButton = content.document.querySelector("#download");
        return !!downloadButton;
      });
      ok(downloadButton, "Download button should be present in pdf.js");
      downloadButton.click();
    });
    info(
      "Waiting for unknown content type dialog to appear from pdf.js download button click"
    );
    let subDialogWindow = await subdialogPromise;
    let subDoc = subDialogWindow.document;
    // Prevent racing with initialization of the dialog and make sure that
    // the final state of the dialog has the correct visibility of the internal-handler option.
    await waitForAcceptButtonToGetEnabled(subDoc);
    let subInternalHandlerRadio = subDoc.querySelector("#handleInternally");
    ok(
      subInternalHandlerRadio.hidden,
      "The option should be hidden when the dialog is opened from pdf.js"
    );
    subDoc.querySelector("#open").click();

    let tabOpenListener = () => {
      ok(
        false,
        "A new tab should not be opened when accepting the dialog with 'Save' chosen"
      );
    };
    gBrowser.tabContainer.addEventListener("TabOpen", tabOpenListener);

    let oldLaunchFile = DownloadIntegration.launchFile;
    let waitForLaunchFileCalled = new Promise(resolve => {
      DownloadIntegration.launchFile = async () => {
        ok(true, "The file should be launched with an external application");
        resolve();
      };
    });

    info("Accepting the dialog");
    subDoc.querySelector("#unknownContentType").acceptDialog();
    info("Waiting until DownloadIntegration.launchFile is called");
    await waitForLaunchFileCalled;
    DownloadIntegration.launchFile = oldLaunchFile;

    gBrowser.tabContainer.removeEventListener("TabOpen", tabOpenListener);
    BrowserTestUtils.removeTab(loadingTab);
    BrowserTestUtils.removeTab(newTab);
    BrowserTestUtils.removeTab(extraTab);
    await publicList.removeFinished();
  }
});

/**
 * Test that choosing to open in an external application doesn't
 * open the PDF into pdf.js
 */
add_task(async function test_check_open_with_external_application() {
  const { DownloadIntegration } = ChromeUtils.import(
    "resource://gre/modules/DownloadIntegration.jsm"
  );
  for (let file of [
    "file_pdf_application_pdf.pdf",
    "file_pdf_binary_octet_stream.pdf",
  ]) {
    info("Testing with " + file);
    await SpecialPowers.pushPrefEnv({
      set: [["browser.helperApps.showOpenOptionForPdfJS", true]],
    });
    let publicList = await Downloads.getList(Downloads.PUBLIC);
    registerCleanupFunction(async () => {
      await publicList.removeFinished();
    });
    let dialogWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
    let loadingTab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TEST_PATH + file
    );
    let dialogWindow = await dialogWindowPromise;
    is(
      dialogWindow.location,
      "chrome://mozapps/content/downloads/unknownContentType.xhtml",
      "Should have seen the unknown content dialogWindow."
    );

    let oldLaunchFile = DownloadIntegration.launchFile;
    let waitForLaunchFileCalled = new Promise(resolve => {
      DownloadIntegration.launchFile = () => {
        ok(true, "The file should be launched with an external application");
        resolve();
      };
    });

    let doc = dialogWindow.document;
    await waitForAcceptButtonToGetEnabled(doc);
    let dialog = doc.querySelector("#unknownContentType");
    doc.querySelector("#open").click();
    let button = dialog.getButton("accept");
    button.disabled = false;
    info("Accepting the dialog");
    dialog.acceptDialog();
    info("Waiting until DownloadIntegration.launchFile is called");
    await waitForLaunchFileCalled;
    DownloadIntegration.launchFile = oldLaunchFile;

    let publicDownloads = await publicList.getAll();
    is(
      publicDownloads.length,
      1,
      "download should appear in publicDownloads list"
    );
    ok(
      !publicDownloads[0].launchWhenSucceeded,
      "launchWhenSucceeded should be false after launchFile is called"
    );

    BrowserTestUtils.removeTab(loadingTab);
    await publicList.removeFinished();
  }
});

/**
 * Check that the "Open with internal handler" option is not presented
 * for non-PDF types.
 */
add_task(async function test_internal_handler_hidden_with_nonpdf_type() {
  let dialogWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
  let loadingTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "file_txt_attachment_test.txt"
  );
  let dialogWindow = await dialogWindowPromise;
  is(
    dialogWindow.location,
    "chrome://mozapps/content/downloads/unknownContentType.xhtml",
    "Should have seen the unknown content dialogWindow."
  );
  let doc = dialogWindow.document;

  // Prevent racing with initialization of the dialog and make sure that
  // the final state of the dialog has the correct visibility of the internal-handler option.
  await waitForAcceptButtonToGetEnabled(doc);

  let internalHandlerRadio = doc.querySelector("#handleInternally");
  ok(
    internalHandlerRadio.hidden,
    "The option should be hidden for unknown file type"
  );

  let dialog = doc.querySelector("#unknownContentType");
  dialog.cancelDialog();
  BrowserTestUtils.removeTab(loadingTab);
});

/**
 * Check that the "Open with internal handler" option is not presented
 * when the feature is disabled.
 */
add_task(async function test_internal_handler_hidden_with_pref_disabled() {
  for (let file of [
    "file_pdf_application_pdf.pdf",
    "file_pdf_binary_octet_stream.pdf",
  ]) {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.helperApps.showOpenOptionForPdfJS", false]],
    });

    let dialogWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
    let loadingTab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TEST_PATH + file
    );
    let dialogWindow = await dialogWindowPromise;
    is(
      dialogWindow.location,
      "chrome://mozapps/content/downloads/unknownContentType.xhtml",
      "Should have seen the unknown content dialogWindow."
    );
    let doc = dialogWindow.document;

    await waitForAcceptButtonToGetEnabled(doc);

    let internalHandlerRadio = doc.querySelector("#handleInternally");
    ok(
      internalHandlerRadio.hidden,
      "The option should be hidden for PDF when the pref is false"
    );

    let dialog = doc.querySelector("#unknownContentType");
    dialog.cancelDialog();
    BrowserTestUtils.removeTab(loadingTab);
  }
});
