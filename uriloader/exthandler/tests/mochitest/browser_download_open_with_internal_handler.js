/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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
 * those choices are handled. When the download improvements are enabled
 * (browser.download.improvements_to_download_panel pref) the unknown content
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

add_task(async function setup() {
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
    subDialogWindow = await subdialogPromise;
    subDoc = subDialogWindow.document;

    // There is no content type here, so the type will be 'other'.
    checkTelemetry(
      "open " + file + " internal from download button",
      "ask",
      "other",
      "attachment"
    );

    // Prevent racing with initialization of the dialog and make sure that
    // the final state of the dialog has the correct visibility of the internal-handler option.
    await waitForAcceptButtonToGetEnabled(subDoc);
    subInternalHandlerRadio = subDoc.querySelector("#handleInternally");
    ok(
      subInternalHandlerRadio.hidden,
      "The option should be hidden when the dialog is opened from pdf.js"
    );
    subDoc.querySelector("#open").click();

    let tabOpenListener = () => {
      ok(
        false,
        "A new tab should not be opened when accepting the dialog with 'open-with-external-app' chosen"
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

    downloadFinishedPromise = promiseDownloadFinished(publicList);

    info("Accepting the dialog");
    subDoc.querySelector("#unknownContentType").acceptDialog();
    info("Waiting until DownloadIntegration.launchFile is called");
    await waitForLaunchFileCalled;
    DownloadIntegration.launchFile = oldLaunchFile;

    // Remove the first file (can't do this sooner or the second load fails):
    if (download?.target.exists) {
      try {
        info("removing " + download.target.path);
        await IOUtils.remove(download.target.path);
      } catch (ex) {
        /* ignore */
      }
    }

    gBrowser.tabContainer.removeEventListener("TabOpen", tabOpenListener);
    BrowserTestUtils.removeTab(loadingTab);
    BrowserTestUtils.removeTab(newTab);
    BrowserTestUtils.removeTab(extraTab);

    // Remove the remaining file once complete.
    download = await downloadFinishedPromise;
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
    let loadingTab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TEST_PATH + file
    );
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
    let loadingTab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TEST_PATH + file
    );
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
    loadingTab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TEST_PATH + file
    );
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
    Services.telemetry.clearEvents();

    const mimeInfosToRestore = alwaysAskForHandlingTypes({
      "text/xml": "xml",
      "binary/octet-stream": "xml",
    });

    for (let [file, checkDefault] of [
      // The default for binary/octet-stream is changed by the PDF tests above,
      // this may change given bug 1659008, so I'm just ignoring the default for now.
      ["file_xml_attachment_binary_octet_stream.xml", false],
      ["file_xml_attachment_test.xml", true],
    ]) {
      let dialogWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
      let loadingTab = await BrowserTestUtils.openNewForegroundTab(
        gBrowser,
        TEST_PATH + file
      );
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
        file == "file_xml_attachment_test.xml" ? "other" : "octetstream",
        "attachment"
      );

      ok(!internalHandlerRadio.hidden, "The option should be visible for XML");
      if (checkDefault) {
        ok(internalHandlerRadio.selected, "The option should be selected");
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
  let loadingTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "file_txt_attachment_test.txt"
  );
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
    let loadingTab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TEST_PATH + file
    );
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
    let loadingTab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TEST_PATH + "file_xml_attachment_test.xml"
    );
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
    },
    false
  );

  for (let improvements of [false, true]) {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.helperApps.showOpenOptionForPdfJS", true],
        ["browser.helperApps.showOpenOptionForViewableInternally", true],
        ["browser.download.improvements_to_download_panel", improvements],
      ],
    });

    for (let file of [
      "file_pdf_application_pdf.pdf",
      "file_pdf_binary_octet_stream.pdf",
    ]) {
      let openPDFDirectly =
        improvements && file == "file_pdf_application_pdf.pdf";
      await BrowserTestUtils.withNewTab(
        { gBrowser, url: "about:blank" },
        async browser => {
          let readyPromise;
          if (improvements) {
            if (openPDFDirectly) {
              readyPromise = BrowserTestUtils.browserLoaded(
                gBrowser.selectedBrowser
              );
            } else {
              readyPromise = BrowserTestUtils.waitForNewTab(gBrowser);
            }

            await SpecialPowers.spawn(
              browser,
              [TEST_PATH + file],
              async contentUrl => {
                content.location = contentUrl;
              }
            );
          } else {
            let dialogWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
            await SpecialPowers.spawn(
              browser,
              [TEST_PATH + file],
              async contentUrl => {
                content.location = contentUrl;
              }
            );

            let dialogWindow = await dialogWindowPromise;

            readyPromise = BrowserTestUtils.waitForNewTab(gBrowser);
            let dialog = dialogWindow.document.querySelector(
              "#unknownContentType"
            );
            dialog.getButton("accept").disabled = false;
            dialog.acceptDialog();
          }

          await readyPromise;

          let action = improvements ? "internal" : "ask";
          checkTelemetry(
            "open " + file + " internal",
            openPDFDirectly ? "none" : action,
            file.includes("octet") ? "octetstream" : "pdf",
            "attachment"
          );

          if (!openPDFDirectly) {
            await BrowserTestUtils.removeTab(gBrowser.selectedTab);
          }
        }
      );
    }
  }

  for (let mimeInfo of mimeInfosToRestore) {
    HandlerSvc.remove(mimeInfo);
  }
});
