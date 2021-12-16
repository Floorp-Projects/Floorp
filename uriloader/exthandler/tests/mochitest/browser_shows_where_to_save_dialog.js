/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { DownloadIntegration } = ChromeUtils.import(
  "resource://gre/modules/DownloadIntegration.jsm"
);

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

const { handleInternally, useHelperApp, useSystemDefault } = Ci.nsIHandlerInfo;

let MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.useDownloadDir", false],
    ],
  });

  registerCleanupFunction(async () => {
    let hiddenPromise = BrowserTestUtils.waitForEvent(
      DownloadsPanel.panel,
      "popuphidden"
    );
    DownloadsPanel.hidePanel();
    await hiddenPromise;
    MockFilePicker.cleanup();
  });
});

// This test ensures that a "Save as..." filepicker dialog is shown for a file
// if useDownloadDir ("Always ask where to save files") is set to false
add_task(async function aDownloadLaunchedWithAppPromptsForFolder() {
  let publicList = await Downloads.getList(Downloads.PUBLIC);
  registerCleanupFunction(async () => {
    await publicList.removeFinished();
  });
  let filePickerShown = new Promise(resolve => {
    MockFilePicker.showCallback = function(fp) {
      ok(true, "filepicker should have been shown");
      setTimeout(resolve, 0);
      return Ci.nsIFilePicker.returnCancel;
    };
  });

  let loadingTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_PATH + "file_txt_attachment_test.txt",
    waitForLoad: false,
    waitForStateStop: true,
  });

  await filePickerShown;

  BrowserTestUtils.removeTab(loadingTab);
});

// This test ensures that downloads configured to open internally create only
// one file destination when saved via the filepicker.
add_task(async function testFilesHandledInternally() {
  let dir = await setupFilePickerDirectory();

  ensureMIMEState({ preferredAction: handleInternally });

  let loadingTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_PATH + "file_image_svgxml.svg",
    waitForLoad: false,
    waitForStateStop: true,
  });

  await TestUtils.waitForCondition(() => {
    return (
      gBrowser.tabs.length === 3 &&
      gBrowser?.tabs[2]?.label.endsWith("file_image_svgxml.svg")
    );
  }, "A new tab for the downloaded svg wasn't open.");

  assertCorrectFile(dir, "file_image_svgxml.svg");

  // Cleanup
  BrowserTestUtils.removeTab(loadingTab);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// This test ensures that downloads configured to open with a system default
// app create only one file destination when saved via the filepicker.
add_task(async function testFilesHandledBySystemDefaultApp() {
  let dir = await setupFilePickerDirectory();

  ensureMIMEState({ preferredAction: useSystemDefault });

  let oldLaunchFile = DownloadIntegration.launchFile;
  let launchFileCalled = new Promise(resolve => {
    DownloadIntegration.launchFile = async (file, mimeInfo) => {
      is(
        useSystemDefault,
        mimeInfo.preferredAction,
        "The file should be launched with a system app handler."
      );
      resolve();
    };
  });

  let loadingTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_PATH + "file_pdf_application_pdf.pdf",
    waitForLoad: false,
    waitForStateStop: true,
  });

  await launchFileCalled;

  assertCorrectFile(dir, "file_pdf_application_pdf.pdf");

  // Cleanup
  BrowserTestUtils.removeTab(loadingTab);
  DownloadIntegration.launchFile = oldLaunchFile;
});

// This test ensures that downloads configured to open with a helper app create
// only one file destination when saved via the filepicker.
add_task(async function testFilesHandledByHelperApp() {
  let dir = await setupFilePickerDirectory();

  // Create a custom helper app so we can check that a launcherPath is
  // configured for the serialized download.
  let appHandler = Cc[
    "@mozilla.org/uriloader/local-handler-app;1"
  ].createInstance(Ci.nsILocalHandlerApp);
  appHandler.name = "Dummy Test Handler";
  appHandler.executable = Services.dirsvc.get("ProfD", Ci.nsIFile);
  appHandler.executable.append("helper_handler_test.exe");

  if (!appHandler.executable.exists()) {
    appHandler.executable.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o777);
  }

  ensureMIMEState({
    preferredAction: useHelperApp,
    preferredHandlerApp: appHandler,
  });

  let publicDownloads = await Downloads.getList(Downloads.PUBLIC);
  let downloadFinishedPromise = new Promise(resolve => {
    publicDownloads.addView({
      onDownloadChanged(download) {
        if (download.succeeded || download.error) {
          ok(
            download.launcherPath.includes("helper_handler_test.exe"),
            "Launcher path is available."
          );
          resolve();
        }
      },
    });
  });

  let oldLaunchFile = DownloadIntegration.launchFile;
  let launchFileCalled = new Promise(resolve => {
    DownloadIntegration.launchFile = async (file, mimeInfo) => {
      is(
        useHelperApp,
        mimeInfo.preferredAction,
        "The file should be launched with a helper app handler."
      );
      resolve();
    };
  });

  let loadingTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_PATH + "file_pdf_application_pdf.pdf",
    waitForLoad: false,
    waitForStateStop: true,
  });

  await downloadFinishedPromise;
  await launchFileCalled;
  assertCorrectFile(dir, "file_pdf_application_pdf.pdf");

  // Cleanup
  BrowserTestUtils.removeTab(loadingTab);
  DownloadIntegration.launchFile = oldLaunchFile;
});

async function setupFilePickerDirectory() {
  let saveDir = createSaveDir();
  Services.prefs.setComplexValue("browser.download.dir", Ci.nsIFile, saveDir);
  Services.prefs.setIntPref("browser.download.folderList", 2);

  MockFilePicker.displayDirectory = saveDir;
  MockFilePicker.returnValue = MockFilePicker.returnOK;
  MockFilePicker.showCallback = function(fp) {
    let file = saveDir.clone();
    file.append(fp.defaultString);
    MockFilePicker.setFiles([file]);
  };

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("browser.download.dir");
    Services.prefs.clearUserPref("browser.download.folderList");
    try {
      await IOUtils.remove(saveDir, { recursive: true });
    } catch (e) {
      Cu.reportError(e);
    }
  });

  return saveDir;
}

function assertCorrectFile(saveDir, filename) {
  info("Make sure additional files haven't been created.");
  let iter = saveDir.directoryEntries;
  let file = iter.nextFile;
  ok(file.path.includes(filename), "Download has correct filename");
  ok(!iter.nextFile, "Only one file was created.");
}

function createSaveDir() {
  info("Creating save directory.");
  let time = new Date().getTime();
  let saveDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  saveDir.append(time);
  return saveDir;
}

function ensureMIMEState({ preferredAction, preferredHandlerApp = null }) {
  const mimeInfo = gMimeSvc.getFromTypeAndExtension("application/pdf", "pdf");
  mimeInfo.preferredAction = preferredAction;
  mimeInfo.preferredApplicationHandler = preferredHandlerApp;
  gHandlerSvc.store(mimeInfo);
}
