const { DownloadIntegration } = ChromeUtils.import(
  "resource://gre/modules/DownloadIntegration.jsm"
);

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

// New file is being downloaded and no dialogs are shown in the way.
add_task(async function skipDialogAndDownloadFile() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.always_ask_before_handling_new_types", false],
      ["browser.download.useDownloadDir", true],
      ["image.webp.enabled", true],
    ],
  });

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

  // We just open the file to be downloaded... and wait for it to be downloaded!
  // We see no dialogs to be accepted in the process.
  let download = await downloadFinishedPromise;
  await BrowserTestUtils.waitForCondition(
    () => gBrowser.tabs.length == initialTabsCount + 2
  );

  gBrowser.removeCurrentTab();
  BrowserTestUtils.removeTab(loadingTab);

  Assert.ok(
    await IOUtils.exists(download.target.path),
    "The file should have been downloaded."
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
