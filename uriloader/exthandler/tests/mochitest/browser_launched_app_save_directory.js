const { DownloadIntegration } = ChromeUtils.import(
  "resource://gre/modules/DownloadIntegration.jsm"
);

const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.always_ask_before_handling_new_types", false],
      ["image.webp.enabled", true],
    ],
  });
  const allowDirectoriesVal = DownloadIntegration.allowDirectories;
  DownloadIntegration.allowDirectories = true;
  registerCleanupFunction(() => {
    DownloadIntegration.allowDirectories = allowDirectoriesVal;
    Services.prefs.clearUserPref("browser.download.dir");
    Services.prefs.clearUserPref("browser.download.folderList");
  });
});

async function aDownloadLaunchedWithAppIsSavedInFolder(downloadDir) {
  let publicList = await Downloads.getList(Downloads.PUBLIC);
  registerCleanupFunction(async () => {
    await publicList.removeFinished();
  });

  let downloadFinishedPromise = promiseDownloadFinished(publicList);
  let initialTabsCount = gBrowser.tabs.length;

  let loadingTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_PATH + "file_green.webp",
    waitForLoad: false,
    waitForStateStop: true,
  });

  let download = await downloadFinishedPromise;
  await BrowserTestUtils.waitForCondition(
    () => gBrowser.tabs.length == initialTabsCount + 2
  );

  gBrowser.removeCurrentTab();
  BrowserTestUtils.removeTab(loadingTab);

  ok(
    download.target.path.startsWith(downloadDir),
    "Download should be placed in default download directory: " +
      downloadDir +
      ", and it's located in " +
      download.target.path
  );

  Assert.ok(
    await OS.File.exists(download.target.path),
    "The file should not have been deleted."
  );

  try {
    info("removing " + download.target.path);
    if (Services.appinfo.OS === "WINNT") {
      // We need to make the file writable to delete it on Windows.
      await IOUtils.setPermissions(download.target.path, 0o600);
    }
    await IOUtils.remove(download.target.path);
  } catch (ex) {
    info("The file " + download.target.path + " is not removed, " + ex);
  }
}

add_task(async function aDownloadLaunchedWithAppIsSavedInCustomDir() {
  //Test the temp dir.
  let time = new Date().getTime();
  let tempDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  tempDir.append(time);
  Services.prefs.setComplexValue("browser.download.dir", Ci.nsIFile, tempDir);
  let downloadDir = await DownloadIntegration.getPreferredDownloadsDirectory();
  Assert.notEqual(downloadDir, "");
  Assert.equal(downloadDir, tempDir.path);
  Assert.ok(await IOUtils.exists(downloadDir));
  registerCleanupFunction(async () => {
    await IOUtils.remove(tempDir.path, { recursive: true });
  });
  await aDownloadLaunchedWithAppIsSavedInFolder(downloadDir);
});

add_task(async function aDownloadLaunchedWithAppIsSavedInDownloadsDir() {
  // Test the system downloads directory.
  Services.prefs.setIntPref("browser.download.folderList", 1);
  let systemDir = await DownloadIntegration.getSystemDownloadsDirectory();
  let downloadDir = await DownloadIntegration.getPreferredDownloadsDirectory();
  Assert.notEqual(downloadDir, "");
  Assert.equal(downloadDir, systemDir);

  await aDownloadLaunchedWithAppIsSavedInFolder(downloadDir);
});

add_task(async function aDownloadLaunchedWithAppIsSavedInDesktopDir() {
  // Test the desktop directory.
  Services.prefs.setIntPref("browser.download.folderList", 0);
  let downloadDir = await DownloadIntegration.getPreferredDownloadsDirectory();
  Assert.notEqual(downloadDir, "");
  Assert.equal(downloadDir, Services.dirsvc.get("Desk", Ci.nsIFile).path);

  await aDownloadLaunchedWithAppIsSavedInFolder(downloadDir);
});
