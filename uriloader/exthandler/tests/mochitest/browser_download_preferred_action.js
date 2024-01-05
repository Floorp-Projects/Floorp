/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { DownloadIntegration } = ChromeUtils.importESModule(
  "resource://gre/modules/DownloadIntegration.sys.mjs"
);
const { FileTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/FileTestUtils.sys.mjs"
);
const gHandlerService = Cc[
  "@mozilla.org/uriloader/handler-service;1"
].getService(Ci.nsIHandlerService);
const gMIMEService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
const localHandlerAppFactory = Cc["@mozilla.org/uriloader/local-handler-app;1"];

const ROOT_URL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

const FILE_TYPES_MIME_SETTINGS = {};

// File types to test
const FILE_TYPES_TO_TEST = [
  // application/ms-word files cannot render in the browser so
  // handleInternally does not work for it
  {
    extension: "doc",
    mimeType: "application/ms-word",
    blockHandleInternally: true,
  },
  {
    extension: "pdf",
    mimeType: "application/pdf",
  },
  {
    extension: "pdf",
    mimeType: "application/unknown",
  },
  {
    extension: "pdf",
    mimeType: "binary/octet-stream",
  },
  // text/plain files automatically render in the browser unless
  // the CD header explicitly tells the browser to download it
  {
    extension: "txt",
    mimeType: "text/plain",
    requireContentDispositionHeader: true,
  },
  {
    extension: "xml",
    mimeType: "binary/octet-stream",
  },
].map(file => {
  return {
    ...file,
    url: `${ROOT_URL}mime_type_download.sjs?contentType=${file.mimeType}&extension=${file.extension}`,
  };
});

// Preferred action types to apply to each downloaded file
const PREFERRED_ACTIONS = [
  "saveToDisk",
  "alwaysAsk",
  "useHelperApp",
  "handleInternally",
  "useSystemDefault",
].map(property => {
  let label = property.replace(/([A-Z])/g, " $1");
  label = label.charAt(0).toUpperCase() + label.slice(1);
  return {
    id: Ci.nsIHandlerInfo[property],
    label,
  };
});

async function createDownloadTest(
  downloadList,
  localHandlerApp,
  file,
  action,
  useContentDispositionHeader
) {
  // Skip handleInternally case for files that cannot be handled internally
  if (
    action.id === Ci.nsIHandlerInfo.handleInternally &&
    file.blockHandleInternally
  ) {
    return;
  }
  let skipDownload =
    action.id === Ci.nsIHandlerInfo.handleInternally &&
    file.mimeType === "application/pdf";
  // Types that require the CD header only display as handleInternally
  // when the CD header is missing
  if (file.requireContentDispositionHeader && !useContentDispositionHeader) {
    if (action.id === Ci.nsIHandlerInfo.handleInternally) {
      skipDownload = true;
    } else {
      return;
    }
  }
  info(
    `Testing download with mime-type ${file.mimeType} and extension ${
      file.extension
    }, preferred action "${action.label}," and ${
      useContentDispositionHeader
        ? "Content-Disposition: attachment"
        : "no Content-Disposition"
    } header.`
  );
  info("Preparing for download...");
  // apply preferredAction settings
  let mimeSettings = gMIMEService.getFromTypeAndExtension(
    file.mimeType,
    file.extension
  );
  mimeSettings.preferredAction = action.id;
  mimeSettings.alwaysAskBeforeHandling =
    action.id === Ci.nsIHandlerInfo.alwaysAsk;
  if (action.id === Ci.nsIHandlerInfo.useHelperApp) {
    mimeSettings.preferredApplicationHandler = localHandlerApp;
  }
  gHandlerService.store(mimeSettings);
  // delayed check for files opened in a new tab, except for skipped downloads
  let expectViewInBrowserTab =
    action.id === Ci.nsIHandlerInfo.handleInternally && !skipDownload;
  let viewInBrowserTabOpened = null;
  if (expectViewInBrowserTab) {
    viewInBrowserTabOpened = BrowserTestUtils.waitForNewTab(
      gBrowser,
      uri => uri.includes("file://"),
      true
    );
  }
  // delayed check for launched files
  let expectLaunch =
    action.id === Ci.nsIHandlerInfo.useSystemDefault ||
    action.id === Ci.nsIHandlerInfo.useHelperApp;
  let oldLaunchFile = DownloadIntegration.launchFile;
  let fileLaunched = null;
  if (expectLaunch) {
    fileLaunched = Promise.withResolvers();
    DownloadIntegration.launchFile = () => {
      ok(
        expectLaunch,
        `The file ${file.mimeType} should be launched with an external application.`
      );
      fileLaunched.resolve();
    };
  }
  info(`Start download of ${file.url}`);
  let dialogWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
  let downloadFinishedPromise = skipDownload
    ? null
    : promiseDownloadFinished(downloadList);
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, file.url);
  if (action.id === Ci.nsIHandlerInfo.alwaysAsk) {
    info("Check Always Ask dialog.");
    let dialogWindow = await dialogWindowPromise;
    is(
      dialogWindow.location.href,
      "chrome://mozapps/content/downloads/unknownContentType.xhtml",
      "Should show unknownContentType dialog for Always Ask preferred actions."
    );
    let doc = dialogWindow.document;
    let dialog = doc.querySelector("#unknownContentType");
    let acceptButton = dialog.getButton("accept");
    acceptButton.disabled = false;
    let saveItem = doc.querySelector("#save");
    saveItem.disabled = false;
    saveItem.click();
    dialog.acceptDialog();
  }
  let download = null;
  let downloadPath = null;
  if (!skipDownload) {
    info("Wait for download to finish...");
    download = await downloadFinishedPromise;
    downloadPath = download.target.path;
  }
  // check delayed assertions
  if (expectLaunch) {
    info("Wait for file to be launched in external application...");
    await fileLaunched.promise;
  }
  if (expectViewInBrowserTab) {
    info("Wait for file to be opened in new tab...");
    let viewInBrowserTab = await viewInBrowserTabOpened;
    ok(
      viewInBrowserTab,
      `The file ${file.mimeType} should be opened in a new tab.`
    );
    BrowserTestUtils.removeTab(viewInBrowserTab);
  }
  info("Checking for saved file...");
  let saveFound = downloadPath && (await IOUtils.exists(downloadPath));
  info("Cleaning up...");
  if (saveFound) {
    try {
      info(`Deleting file ${downloadPath}...`);
      await IOUtils.remove(downloadPath);
    } catch (ex) {
      info(`Error: ${ex}`);
    }
  }
  info("Removing download from list...");
  await downloadList.removeFinished();
  info("Clearing settings...");
  DownloadIntegration.launchFile = oldLaunchFile;
  info("Asserting results...");
  if (download) {
    ok(download.succeeded, "Download should complete successfully");
    ok(
      !download._launchedFromPanel,
      "Download should never be launched from panel"
    );
  }
  if (skipDownload) {
    ok(!saveFound, "Download should not be saved to disk");
  } else {
    ok(saveFound, "Download should be saved to disk");
  }
}

add_task(async function test_download_preferred_action() {
  // Prepare tests
  for (const index in FILE_TYPES_TO_TEST) {
    let file = FILE_TYPES_TO_TEST[index];
    let originalMimeSettings = gMIMEService.getFromTypeAndExtension(
      file.mimeType,
      file.extension
    );
    if (gHandlerService.exists(originalMimeSettings)) {
      FILE_TYPES_MIME_SETTINGS[index] = originalMimeSettings;
    }
  }
  let downloadList = await Downloads.getList(Downloads.PUBLIC);
  let oldLaunchFile = DownloadIntegration.launchFile;
  registerCleanupFunction(async function () {
    await removeAllDownloads();
    DownloadIntegration.launchFile = oldLaunchFile;
    Services.prefs.clearUserPref(
      "browser.download.always_ask_before_handling_new_types"
    );
    BrowserTestUtils.startLoadingURIString(
      gBrowser.selectedBrowser,
      "about:home"
    );
    for (const index in FILE_TYPES_TO_TEST) {
      let file = FILE_TYPES_TO_TEST[index];
      let mimeSettings = gMIMEService.getFromTypeAndExtension(
        file.mimeType,
        file.extension
      );
      if (FILE_TYPES_MIME_SETTINGS[index]) {
        gHandlerService.store(FILE_TYPES_MIME_SETTINGS[index]);
      } else {
        gHandlerService.remove(mimeSettings);
      }
    }
  });
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.always_ask_before_handling_new_types", false]],
  });
  let launcherPath = FileTestUtils.getTempFile("app-launcher").path;
  let localHandlerApp = localHandlerAppFactory.createInstance(
    Ci.nsILocalHandlerApp
  );
  localHandlerApp.executable = new FileUtils.File(launcherPath);
  localHandlerApp.executable.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o755);
  // run tests
  for (const file of FILE_TYPES_TO_TEST) {
    // The CD header specifies the download file extension on download
    let fileNoHeader = file;
    let fileWithHeader = structuredClone(file);
    fileWithHeader.url += "&withHeader";
    for (const action of PREFERRED_ACTIONS) {
      // Clone file objects to prevent side-effects between iterations
      await createDownloadTest(
        downloadList,
        localHandlerApp,
        structuredClone(fileWithHeader),
        action,
        true
      );
      await createDownloadTest(
        downloadList,
        localHandlerApp,
        structuredClone(fileNoHeader),
        action,
        false
      );
    }
  }
});
