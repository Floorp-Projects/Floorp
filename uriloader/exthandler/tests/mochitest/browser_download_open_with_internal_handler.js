/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

const { Downloads } = ChromeUtils.import(
  "resource://gre/modules/Downloads.jsm"
);
const { DownloadIntegration } = ChromeUtils.import(
  "resource://gre/modules/DownloadIntegration.jsm"
);

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

const MimeSvc = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
const HandlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
  Ci.nsIHandlerService
);

let MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

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

/**
 * This test covers which choices are presented for downloaded files and how
 * those choices are handled. Unless a pref is enabled
 * (browser.download.always_ask_before_handling_new_types) the unknown content
 * dialog will be skipped altogether by default when downloading.
 * To retain coverage for the non-default scenario, each task sets `alwaysAskBeforeHandling`
 * to true for the relevant mime-type and extensions.
 */
function alwaysAskForHandlingTypes(typeExtensions, ask = true) {
  let mimeInfos = [];
  for (let [type, ext] of Object.entries(typeExtensions)) {
    const mimeInfo = MimeSvc.getFromTypeAndExtension(type, ext);
    mimeInfo.alwaysAskBeforeHandling = ask;
    if (!ask) {
      mimeInfo.preferredAction = mimeInfo.handleInternally;
    }
    HandlerSvc.store(mimeInfo);
    mimeInfos.push(mimeInfo);
  }
  return mimeInfos;
}

function checkTelemetry(desc, expectedAction, expectedType, expectedReason) {
  let events = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    true
  );
  events = (events.parent || []).filter(
    e => e[1] == "downloads" && e[2] == "helpertype"
  );

  if (expectedAction == "none") {
    is(events.length, 0, desc + " number of events");
    return;
  }

  is(events.length, 1, desc + " number of events");

  let event = events[0];
  is(event[4], expectedAction, desc + " telemetry action");
  is(event[5].type, expectedType, desc + " telemetry type");
  is(event[5].reason, expectedReason, desc + " telemetry reason");
}

add_setup(async function() {
  // Remove the security delay for the dialog during the test.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.dialog_enable_delay", 0],
      ["browser.helperApps.showOpenOptionForPdfJS", true],
      ["browser.helperApps.showOpenOptionForViewableInternally", true],
    ],
  });

  // Restore handlers after the whole test has run
  const registerRestoreHandler = function(type, ext) {
    const mimeInfo = MimeSvc.getFromTypeAndExtension(type, ext);
    const existed = HandlerSvc.exists(mimeInfo);

    registerCleanupFunction(() => {
      if (existed) {
        HandlerSvc.store(mimeInfo);
      } else {
        HandlerSvc.remove(mimeInfo);
      }
    });
  };
  registerRestoreHandler("application/pdf", "pdf");
  registerRestoreHandler("binary/octet-stream", "pdf");
  registerRestoreHandler("application/unknown", "pdf");
  registerRestoreHandler("image/webp", "webp");
});

/**
 * Check that loading a PDF file with content-disposition: attachment
 * shows an option to open with the internal handler, and that the
 * internal option handler is not present when the download button
 * is clicked from pdf.js.
 */
add_task(async function test_check_open_with_internal_handler() {
  Services.telemetry.clearEvents();

  const mimeInfosToRestore = alwaysAskForHandlingTypes({
    "application/pdf": "pdf",
    "binary/octet-stream": "pdf",
  });

  for (let file of [
    "file_pdf_application_pdf.pdf",
    "file_pdf_binary_octet_stream.pdf",
  ]) {
    info("Testing with " + file);
    let publicList = await Downloads.getList(Downloads.PUBLIC);
    registerCleanupFunction(async () => {
      await publicList.removeFinished();
    });
    let dialogWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
    let loadingTab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      opening: TEST_PATH + file,
      waitForLoad: false,
      waitForStateStop: true,
    });
    // Add an extra tab after the loading tab so we can test that
    // pdf.js is opened in the adjacent tab and not at the end of
    // the tab strip.
    let extraTab = await BrowserTestUtils.addTab(gBrowser, "about:blank");
    let dialogWindow = await dialogWindowPromise;
    is(
      dialogWindow.location.href,
      "chrome://mozapps/content/downloads/unknownContentType.xhtml",
      "Should have seen the unknown content dialogWindow."
    );
    let doc = dialogWindow.document;
    let internalHandlerRadio = doc.querySelector("#handleInternally");

    await waitForAcceptButtonToGetEnabled(doc);

    checkTelemetry(
      "open " + file + " internal",
      "ask",
      file.includes("octet") ? "octetstream" : "pdf",
      "attachment"
    );

    ok(!internalHandlerRadio.hidden, "The option should be visible for PDF");
    ok(internalHandlerRadio.selected, "The option should be selected");

    let downloadFinishedPromise = promiseDownloadFinished(publicList);
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

    let download = await downloadFinishedPromise;

    let subdialogPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
    // Current tab has file: URI and TEST_PATH is http uri, so uri will be different
    BrowserTestUtils.loadURI(newTab.linkedBrowser, TEST_PATH + file);
    let subDialogWindow = await subdialogPromise;
    let subDoc = subDialogWindow.document;

    checkTelemetry(
      "open " + file + " internal from current tab",
      "ask",
      file.includes("octet") ? "octetstream" : "pdf",
      "attachment"
    );

    // Prevent racing with initialization of the dialog and make sure that
    // the final state of the dialog has the correct visibility of the internal-handler option.
    await waitForAcceptButtonToGetEnabled(subDoc);
    let subInternalHandlerRadio = subDoc.querySelector("#handleInternally");
    ok(
      !subInternalHandlerRadio.hidden,
      "This option should be shown when the dialog is shown for another PDF"
    );
    // Cancel dialog
    subDoc.querySelector("#unknownContentType").cancelDialog();

    let filepickerPromise = new Promise(resolve => {
      MockFilePicker.showCallback = function(fp) {
        setTimeout(() => {
          resolve(fp.defaultString);
        }, 0);
        return Ci.nsIFilePicker.returnCancel;
      };
    });

    subdialogPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
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
    let filename = await filepickerPromise;
    is(filename, file, "filename was set in filepicker");

    // Remove the first file (can't do this sooner or the second load fails):
    if (download?.target.exists) {
      try {
        info("removing " + download.target.path);
        await IOUtils.remove(download.target.path);
      } catch (ex) {
        /* ignore */
      }
    }

    BrowserTestUtils.removeTab(loadingTab);
    BrowserTestUtils.removeTab(newTab);
    BrowserTestUtils.removeTab(extraTab);

    await publicList.removeFinished();
  }
  for (let mimeInfo of mimeInfosToRestore) {
    HandlerSvc.remove(mimeInfo);
  }
});

/**
 * Test that choosing to open in an external application doesn't
 * open the PDF into pdf.js
 */
add_task(async function test_check_open_with_external_application() {
  Services.telemetry.clearEvents();

  const mimeInfosToRestore = alwaysAskForHandlingTypes({
    "application/pdf": "pdf",
    "binary/octet-stream": "pdf",
  });

  for (let file of [
    "file_pdf_application_pdf.pdf",
    "file_pdf_binary_octet_stream.pdf",
  ]) {
    info("Testing with " + file);
    let publicList = await Downloads.getList(Downloads.PUBLIC);
    registerCleanupFunction(async () => {
      await publicList.removeFinished();
    });
    let dialogWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
    let loadingTab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      opening: TEST_PATH + file,
      waitForLoad: false,
      waitForStateStop: true,
    });
    let dialogWindow = await dialogWindowPromise;
    is(
      dialogWindow.location.href,
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

    checkTelemetry(
      "open " + file + " external",
      "ask",
      file.includes("octet") ? "octetstream" : "pdf",
      "attachment"
    );

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
    let download = publicDownloads[0];
    ok(
      !download.launchWhenSucceeded,
      "launchWhenSucceeded should be false after launchFile is called"
    );

    BrowserTestUtils.removeTab(loadingTab);
    if (download?.target.exists) {
      try {
        info("removing " + download.target.path);
        await IOUtils.remove(download.target.path);
      } catch (ex) {
        /* ignore */
      }
    }
    await publicList.removeFinished();
  }
  for (let mimeInfo of mimeInfosToRestore) {
    HandlerSvc.remove(mimeInfo);
  }
});

/**
 * Test that choosing to open a PDF with an external application works and
 * then downloading the same file again and choosing Open with Firefox opens
 * the download in Firefox.
 */
add_task(async function test_check_open_with_external_then_internal() {
  // This test only runs on Windows because appPicker.xhtml is only used on Windows.
  if (AppConstants.platform != "win") {
    return;
  }

  // This test covers a bug that only occurs when the mimeInfo is set to Always Ask
  const mimeInfo = MimeSvc.getFromTypeAndExtension("application/pdf", "pdf");
  console.log(
    "mimeInfo.preferredAction is currently:",
    mimeInfo.preferredAction
  );
  mimeInfo.preferredAction = mimeInfo.alwaysAsk;
  mimeInfo.alwaysAskBeforeHandling = true;
  HandlerSvc.store(mimeInfo);

  for (let [file, mimeType] of [
    ["file_pdf_application_pdf.pdf", "application/pdf"],
    ["file_pdf_binary_octet_stream.pdf", "binary/octet-stream"],
    ["file_pdf_application_unknown.pdf", "application/unknown"],
  ]) {
    info("Testing with " + file);
    let originalMimeInfo = MimeSvc.getFromTypeAndExtension(mimeType, "pdf");

    let publicList = await Downloads.getList(Downloads.PUBLIC);
    registerCleanupFunction(async () => {
      await publicList.removeFinished();
    });
    let dialogWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
    // Open a new tab to the PDF file which will trigger the Unknown Content Type dialog
    // and choose to open the PDF with an external application.
    let loadingTab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      opening: TEST_PATH + file,
      waitForLoad: false,
      waitForStateStop: true,
    });
    let dialogWindow = await dialogWindowPromise;
    is(
      dialogWindow.location.href,
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
    let openHandlerMenulist = doc.querySelector("#openHandler");
    let originalDefaultHandler = openHandlerMenulist.label;
    doc.querySelector("#open").click();
    doc.querySelector("#openHandlerPopup").click();
    let oldOpenDialog = dialogWindow.openDialog;
    dialogWindow.openDialog = (location, unused2, unused3, params) => {
      is(location, "chrome://global/content/appPicker.xhtml", "app picker");
      let handlerApp = params.mimeInfo.possibleLocalHandlers.queryElementAt(
        0,
        Ci.nsILocalHandlerApp
      );
      ok(handlerApp.executable, "handlerApp should be executable");
      ok(handlerApp.executable.isFile(), "handlerApp should be a file");
      params.handlerApp = handlerApp;
    };
    doc.querySelector("#choose").click();
    dialogWindow.openDialog = oldOpenDialog;
    await TestUtils.waitForCondition(
      () => originalDefaultHandler != openHandlerMenulist.label,
      "waiting for openHandler to get updated"
    );
    let newDefaultHandler = openHandlerMenulist.label;
    info(`was ${originalDefaultHandler}, now ${newDefaultHandler}`);
    let button = dialog.getButton("accept");
    button.disabled = false;
    info("Accepting the dialog");
    dialog.acceptDialog();
    info("Waiting until DownloadIntegration.launchFile is called");
    await waitForLaunchFileCalled;
    BrowserTestUtils.removeTab(loadingTab);

    // Now, open a new tab to the PDF file which will trigger the Unknown Content Type dialog
    // and choose to open the PDF internally. The previously used external application should be shown as
    // the external option.
    dialogWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
    loadingTab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      opening: TEST_PATH + file,
      waitForLoad: false,
      waitForStateStop: true,
    });
    dialogWindow = await dialogWindowPromise;
    is(
      dialogWindow.location.href,
      "chrome://mozapps/content/downloads/unknownContentType.xhtml",
      "Should have seen the unknown content dialogWindow."
    );

    DownloadIntegration.launchFile = () => {
      ok(false, "The file should not be launched with an external application");
    };

    doc = dialogWindow.document;
    await waitForAcceptButtonToGetEnabled(doc);
    openHandlerMenulist = doc.querySelector("#openHandler");
    is(openHandlerMenulist.label, newDefaultHandler, "'new' handler");
    dialog = doc.querySelector("#unknownContentType");
    doc.querySelector("#handleInternally").click();
    info("Accepting the dialog");
    button = dialog.getButton("accept");
    button.disabled = false;
    let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
    dialog.acceptDialog();

    info("waiting for new tab to open");
    let newTab = await newTabPromise;

    await ContentTask.spawn(newTab.linkedBrowser, null, async () => {
      await ContentTaskUtils.waitForCondition(
        () => content.document.readyState == "complete"
      );
    });

    is(
      newTab.linkedBrowser.contentPrincipal.origin,
      "resource://pdf.js",
      "PDF should be opened with pdf.js"
    );

    BrowserTestUtils.removeTab(loadingTab);
    BrowserTestUtils.removeTab(newTab);

    // Now trigger the dialog again and select the system
    // default option to reset the state for the next iteration of the test.
    // Reset the state for the next iteration of the test.
    HandlerSvc.store(originalMimeInfo);
    DownloadIntegration.launchFile = oldLaunchFile;
    let [download] = await publicList.getAll();
    if (download?.target.exists) {
      try {
        info("removing " + download.target.path);
        await IOUtils.remove(download.target.path);
      } catch (ex) {
        /* ignore */
      }
    }
    await publicList.removeFinished();
  }
});

/**
 * Check that the "Open with internal handler" option is presented
 * for other viewable internally types.
 */
add_task(
  async function test_internal_handler_hidden_with_viewable_internally_type() {
    await SpecialPowers.pushPrefEnv({
      set: [["image.webp.enabled", true]],
    });
    Services.telemetry.clearEvents();

    const mimeInfosToRestore = alwaysAskForHandlingTypes({
      "binary/octet-stream": "xml",
      "image/webp": "webp",
    });

    for (let [file, checkDefault] of [
      // The default for binary/octet-stream is changed by the PDF tests above,
      // this may change given bug 1659008, so I'm just ignoring the default for now.
      ["file_xml_attachment_binary_octet_stream.xml", false],
      ["file_green.webp", true],
    ]) {
      let dialogWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
      let loadingTab = await BrowserTestUtils.openNewForegroundTab({
        gBrowser,
        opening: TEST_PATH + file,
        waitForLoad: false,
        waitForStateStop: true,
      });
      let dialogWindow = await dialogWindowPromise;
      is(
        dialogWindow.location.href,
        "chrome://mozapps/content/downloads/unknownContentType.xhtml",
        "Should have seen the unknown content dialogWindow."
      );
      let doc = dialogWindow.document;
      let internalHandlerRadio = doc.querySelector("#handleInternally");

      // Prevent racing with initialization of the dialog and make sure that
      // the final state of the dialog has the correct visibility of the internal-handler option.
      await waitForAcceptButtonToGetEnabled(doc);

      checkTelemetry(
        "open " + file + " for viewable internal type",
        "ask",
        file.endsWith(".webp") ? "other" : "octetstream",
        "attachment"
      );

      let fileDesc = file.substring(file.lastIndexOf(".") + 1);

      ok(
        !internalHandlerRadio.hidden,
        `The option should be visible for ${fileDesc}`
      );
      if (checkDefault) {
        ok(
          internalHandlerRadio.selected,
          `The option should be selected for ${fileDesc}`
        );
      }

      let dialog = doc.querySelector("#unknownContentType");
      dialog.cancelDialog();
      BrowserTestUtils.removeTab(loadingTab);
    }
    for (let mimeInfo of mimeInfosToRestore) {
      HandlerSvc.remove(mimeInfo);
    }
  }
);

/**
 * Check that the "Open with internal handler" option is not presented
 * for non-PDF, non-viewable-internally types.
 */
add_task(async function test_internal_handler_hidden_with_other_type() {
  Services.telemetry.clearEvents();

  const mimeInfosToRestore = alwaysAskForHandlingTypes({
    "text/plain": "txt",
  });

  let dialogWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
  let loadingTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_PATH + "file_txt_attachment_test.txt",
    waitForLoad: false,
    waitForStateStop: true,
  });
  let dialogWindow = await dialogWindowPromise;
  is(
    dialogWindow.location.href,
    "chrome://mozapps/content/downloads/unknownContentType.xhtml",
    "Should have seen the unknown content dialogWindow."
  );
  let doc = dialogWindow.document;

  // Prevent racing with initialization of the dialog and make sure that
  // the final state of the dialog has the correct visibility of the internal-handler option.
  await waitForAcceptButtonToGetEnabled(doc);

  checkTelemetry(
    "open file_txt_attachment_test.txt for non-viewable internal type",
    "ask",
    "other",
    "attachment"
  );

  let internalHandlerRadio = doc.querySelector("#handleInternally");
  ok(
    internalHandlerRadio.hidden,
    "The option should be hidden for unknown file type"
  );

  let dialog = doc.querySelector("#unknownContentType");
  dialog.cancelDialog();
  BrowserTestUtils.removeTab(loadingTab);
  for (let mimeInfo of mimeInfosToRestore) {
    HandlerSvc.remove(mimeInfo);
  }
});

/**
 * Check that the "Open with internal handler" option is not presented
 * when the feature is disabled for PDFs.
 */
add_task(async function test_internal_handler_hidden_with_pdf_pref_disabled() {
  const mimeInfosToRestore = alwaysAskForHandlingTypes({
    "application/pdf": "pdf",
    "binary/octet-stream": "pdf",
  });
  await SpecialPowers.pushPrefEnv({
    set: [["browser.helperApps.showOpenOptionForPdfJS", false]],
  });
  for (let file of [
    "file_pdf_application_pdf.pdf",
    "file_pdf_binary_octet_stream.pdf",
  ]) {
    let dialogWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
    let loadingTab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      opening: TEST_PATH + file,
      waitForLoad: false,
      waitForStateStop: true,
    });
    let dialogWindow = await dialogWindowPromise;
    is(
      dialogWindow.location.href,
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
  for (let mimeInfo of mimeInfosToRestore) {
    HandlerSvc.remove(mimeInfo);
  }
});

/**
 * Check that the "Open with internal handler" option is not presented
 * for other viewable internally types when disabled.
 */
add_task(
  async function test_internal_handler_hidden_with_viewable_internally_pref_disabled() {
    const mimeInfosToRestore = alwaysAskForHandlingTypes({
      "text/xml": "xml",
    });
    await SpecialPowers.pushPrefEnv({
      set: [["browser.helperApps.showOpenOptionForViewableInternally", false]],
    });
    let dialogWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
    let loadingTab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      opening: TEST_PATH + "file_xml_attachment_test.xml",
      waitForLoad: false,
      waitForStateStop: true,
    });
    let dialogWindow = await dialogWindowPromise;
    is(
      dialogWindow.location.href,
      "chrome://mozapps/content/downloads/unknownContentType.xhtml",
      "Should have seen the unknown content dialogWindow."
    );
    let doc = dialogWindow.document;

    await waitForAcceptButtonToGetEnabled(doc);

    let internalHandlerRadio = doc.querySelector("#handleInternally");
    ok(
      internalHandlerRadio.hidden,
      "The option should be hidden for XML when the pref is false"
    );

    let dialog = doc.querySelector("#unknownContentType");
    dialog.cancelDialog();
    BrowserTestUtils.removeTab(loadingTab);
    for (let mimeInfo of mimeInfosToRestore) {
      HandlerSvc.remove(mimeInfo);
    }
  }
);

/*
 * This test sets the action to internal. The files should open directly without asking.
 */
add_task(async function test_check_open_with_internal_handler_noask() {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();

  const mimeInfosToRestore = alwaysAskForHandlingTypes(
    {
      "application/pdf": "pdf",
      "binary/octet-stream": "pdf",
      "application/octet-stream": "pdf",
    },
    false
  );

  // Build the matrix of tests to perform.
  let matrix = {
    alwaysOpenPDFInline: [false, true],
    improvements: [false, true],
    file: [
      "file_pdf_application_pdf.pdf",
      "file_pdf_binary_octet_stream.pdf",
      "file_pdf_application_octet_stream.pdf",
    ],
    where: ["top", "popup", "frame"],
  };
  let tests = [{}];
  for (let [key, values] of Object.entries(matrix)) {
    tests = tests.flatMap(test =>
      values.map(value => ({ [key]: value, ...test }))
    );
  }

  for (let test of tests) {
    info(`test case: ${JSON.stringify(test)}`);
    let { alwaysOpenPDFInline, improvements, file, where } = test;

    // These are the cases that can be opened inline. binary/octet-stream
    // isn't handled by pdfjs.
    let canHandleInline =
      file == "file_pdf_application_pdf.pdf" ||
      (file == "file_pdf_application_octet_stream.pdf" && where != "frame");

    // We expect a dialog to appear when the improvements is set to false,
    // and either the open pdf attachments inline setting is false or
    // we cannot handle the pdf inline.
    let expectDialog =
      !improvements && (!alwaysOpenPDFInline || !canHandleInline);

    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.helperApps.showOpenOptionForPdfJS", true],
        ["browser.helperApps.showOpenOptionForViewableInternally", true],
        ["browser.download.improvements_to_download_panel", improvements],
        ["browser.download.always_ask_before_handling_new_types", expectDialog],
        ["browser.download.open_pdf_attachments_inline", alwaysOpenPDFInline],
      ],
    });

    async function doNavigate(browser) {
      await SpecialPowers.spawn(
        browser,
        [TEST_PATH + file, where],
        async (contentUrl, where_) => {
          switch (where_) {
            case "top":
              content.location = contentUrl;
              break;
            case "popup":
              content.open(contentUrl);
              break;
            case "frame":
              let frame = content.document.createElement("iframe");
              frame.setAttribute("src", contentUrl);
              content.document.body.appendChild(frame);
              break;
            default:
              ok(false, "Unknown where value");
              break;
          }
        }
      );
    }

    // If this is true, the pdf is opened directly without downloading it.
    // Otherwise, it must first be downloaded and optionally displayed in
    // a tab with a file url.
    let openPDFDirectly =
      !expectDialog && alwaysOpenPDFInline && canHandleInline;

    await BrowserTestUtils.withNewTab(
      { gBrowser, url: TEST_PATH + "blank.html" },
      async browser => {
        let readyPromise;
        if (expectDialog) {
          let dialogWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
          await doNavigate(browser);

          let dialogWindow = await dialogWindowPromise;

          readyPromise = BrowserTestUtils.waitForNewTab(gBrowser);
          let dialog = dialogWindow.document.querySelector(
            "#unknownContentType"
          );
          dialog.getButton("accept").disabled = false;
          dialog.acceptDialog();
        } else {
          readyPromise = BrowserTestUtils.waitForNewTab(
            gBrowser,
            null,
            false,
            !openPDFDirectly
          );

          await doNavigate(browser);
        }

        await readyPromise;

        is(
          gBrowser.selectedBrowser.currentURI.scheme,
          openPDFDirectly ? "https" : "file",
          "Loaded PDF uri has the correct scheme"
        );

        // intentionally don't bother checking session history without ship to
        // keep complexity down.
        if (Services.appinfo.sessionHistoryInParent) {
          let shistory = browser.browsingContext.sessionHistory;
          is(shistory.count, 1, "should a single shentry");
          is(shistory.index, 0, "should be on the first entry");
          let shentry = shistory.getEntryAtIndex(shistory.index);
          is(shentry.URI.spec, TEST_PATH + "blank.html");
        }

        await SpecialPowers.spawn(
          browser,
          [TEST_PATH + "blank.html"],
          async blankUrl => {
            ok(
              !docShell.isAttemptingToNavigate,
              "should not still be attempting to navigate"
            );
            is(
              content.location.href,
              blankUrl,
              "original browser hasn't navigated"
            );
          }
        );

        let action = expectDialog ? "ask" : "internal";
        checkTelemetry(
          "open " + file + " internal",
          openPDFDirectly ? "none" : action,
          file.includes("octet") ? "octetstream" : "pdf",
          "attachment"
        );

        await BrowserTestUtils.removeTab(gBrowser.selectedTab);
      }
    );
  }

  for (let mimeInfo of mimeInfosToRestore) {
    HandlerSvc.remove(mimeInfo);
  }
});

add_task(async () => {
  MockFilePicker.cleanup();
});
