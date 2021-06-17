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
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.useDownloadDir", false],
    ],
  });

  let publicList = await Downloads.getList(Downloads.PUBLIC);
  registerCleanupFunction(async () => {
    await publicList.removeFinished();
  });
  let dialogWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
  let loadingTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "file_pdf_application_pdf.pdf"
  );

  let dialogWindow = await dialogWindowPromise;
  let doc = dialogWindow.document;
  let dialog = doc.querySelector("#unknownContentType");
  let button = dialog.getButton("accept");

  await TestUtils.waitForCondition(
    () => !button.disabled,
    "Wait for Accept button to get enabled"
  );

  let downloadFinishedPromise = promiseDownloadFinished(publicList);
  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);

  button.disabled = false;
  dialog.acceptDialog();
  let newTab = await newTabPromise;

  await ContentTask.spawn(newTab.linkedBrowser, null, async () => {
    await ContentTaskUtils.waitForCondition(
      () => content.document.readyState == "complete"
    );
  });

  let download = await downloadFinishedPromise;

  BrowserTestUtils.removeTab(loadingTab);
  BrowserTestUtils.removeTab(newTab);

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
