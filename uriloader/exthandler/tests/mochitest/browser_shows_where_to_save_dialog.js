/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

// This test ensures that a "Save as..." filepicker dialog is shown for a file
// if useDownloadDir ("Always ask where to save files") is set to false
add_task(async function aDownloadLaunchedWithAppPromptsForFolder() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.useDownloadDir", false],
    ],
  });

  let MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(window);

  let publicList = await Downloads.getList(Downloads.PUBLIC);
  registerCleanupFunction(async () => {
    await publicList.removeFinished();
    MockFilePicker.cleanup();
  });
  let filePickerShown = new Promise(resolve => {
    MockFilePicker.showCallback = function(fp) {
      ok(true, "filepicker should have been shown");
      setTimeout(resolve, 0);
      return Ci.nsIFilePicker.returnCancel;
    };
  });

  let loadingTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "file_txt_attachment_test.txt"
  );

  await filePickerShown;

  BrowserTestUtils.removeTab(loadingTab);
});
