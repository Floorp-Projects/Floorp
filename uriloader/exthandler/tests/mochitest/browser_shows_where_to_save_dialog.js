/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { DownloadIntegration } = ChromeUtils.importESModule(
  "resource://gre/modules/DownloadIntegration.sys.mjs"
);

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

const { handleInternally, useHelperApp, useSystemDefault, saveToDisk } =
  Ci.nsIHandlerInfo;

let MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.always_ask_before_handling_new_types", false],
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
// if useDownloadDir ("Always ask where to save files") is set to false and
// the filetype is set to save to disk.
add_task(async function aDownloadSavedToDiskPromptsForFolder() {
  let publicList = await Downloads.getList(Downloads.PUBLIC);
  ensureMIMEState(
    { preferredAction: saveToDisk },
    { type: "text/plain", ext: "txt" }
  );
  registerCleanupFunction(async () => {
    await publicList.removeFinished();
  });
  let filePickerShownPromise = new Promise(resolve => {
    MockFilePicker.showCallback = function (fp) {
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

  info("Waiting on filepicker.");
  await filePickerShownPromise;
  ok(true, "filepicker should have been shown");

  BrowserTestUtils.removeTab(loadingTab);
});

// This test ensures that downloads configured to open internally create only
// one file destination when saved via the filepicker, and don't prompt.
add_task(async function testFilesHandledInternally() {
  let dir = await setupFilePickerDirectory();

  ensureMIMEState(
    { preferredAction: handleInternally },
    { type: "image/webp", ext: "webp" }
  );

  let filePickerShown = false;
  MockFilePicker.showCallback = function (fp) {
    filePickerShown = true;
    return Ci.nsIFilePicker.returnCancel;
  };

  let thirdTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    url => {
      info("Got load for " + url);
      return url.endsWith("file_green.webp") && url.startsWith("file:");
    },
    true,
    true
  );
  let loadingTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_PATH + "file_green.webp",
    waitForLoad: false,
    waitForStateStop: true,
  });

  let openedTab = await thirdTabPromise;
  ok(!filePickerShown, "file picker should not have shown up.");

  assertCorrectFile(dir, "file_green.webp");

  // Cleanup
  BrowserTestUtils.removeTab(loadingTab);
  BrowserTestUtils.removeTab(openedTab);
});

// This test ensures that downloads configured to open with a system default
// app create only one file destination and don't open the filepicker.
add_task(async function testFilesHandledBySystemDefaultApp() {
  let dir = await setupFilePickerDirectory();

  ensureMIMEState({ preferredAction: useSystemDefault });

  let filePickerShown = false;
  MockFilePicker.showCallback = function (fp) {
    filePickerShown = true;
    return Ci.nsIFilePicker.returnCancel;
  };

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
  ok(!filePickerShown, "file picker should not have shown up.");

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

  let filePickerShown = false;
  MockFilePicker.showCallback = function (fp) {
    filePickerShown = true;
    return Ci.nsIFilePicker.returnCancel;
  };

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
  ok(!filePickerShown, "file picker should not have shown up.");
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
  MockFilePicker.showCallback = function (fp) {
    let file = saveDir.clone();
    file.append(fp.defaultString);
    MockFilePicker.setFiles([file]);
  };

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("browser.download.dir");
    Services.prefs.clearUserPref("browser.download.folderList");
    let publicList = await Downloads.getList(Downloads.PUBLIC);
    let unfinishedDownloads = new Set(
      (await publicList.getAll()).filter(dl => !dl.succeeded && !dl.error)
    );
    if (unfinishedDownloads.size) {
      info(`Have ${unfinishedDownloads.size} unfinished downloads, waiting.`);
      await new Promise(resolve => {
        let view = {
          onChanged(dl) {
            if (unfinishedDownloads.has(dl) && (dl.succeeded || dl.error)) {
              unfinishedDownloads.delete(dl);
              info(`Removed another download.`);
              if (!unfinishedDownloads.size) {
                publicList.removeView(view);
                resolve();
              }
            }
          },
        };
        publicList.addView(view);
      });
    }
    try {
      await IOUtils.remove(saveDir.path, { recursive: true });
    } catch (e) {
      console.error(e);
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

function ensureMIMEState(
  { preferredAction, preferredHandlerApp = null },
  { type = "application/pdf", ext = "pdf" } = {}
) {
  const mimeInfo = gMimeSvc.getFromTypeAndExtension(type, ext);
  mimeInfo.preferredAction = preferredAction;
  mimeInfo.preferredApplicationHandler = preferredHandlerApp;
  mimeInfo.alwaysAskBeforeHandling = false;
  gHandlerSvc.store(mimeInfo);
}
