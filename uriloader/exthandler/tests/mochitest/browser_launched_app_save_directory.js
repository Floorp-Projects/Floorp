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

// This test ensures that a file downloaded with "open with app" option
// is actually saved in default Downloads directory.
add_task(async function aDownloadLaunchedWithAppIsSavedInDownloadsFolder() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.improvements_to_download_panel", true]],
  });

  let publicList = await Downloads.getList(Downloads.PUBLIC);
  registerCleanupFunction(async () => {
    await publicList.removeFinished();
  });
  let downloadFinishedPromise = promiseDownloadFinished(publicList);
  let initialTabsCount = gBrowser.tabs.length;

  let loadingTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "file_pdf_application_pdf.pdf"
  );

  let download = await downloadFinishedPromise;
  await BrowserTestUtils.waitForCondition(
    () => gBrowser.tabs.length == initialTabsCount + 2
  );

  gBrowser.removeCurrentTab();
  BrowserTestUtils.removeTab(loadingTab);

  let downloadDir = await DownloadIntegration.getSystemDownloadsDirectory();
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
});
