/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

function waitDelay(delay) {
  return new Promise((resolve, reject) => {
    /* eslint-disable mozilla/no-arbitrary-setTimeout */
    window.setTimeout(resolve, delay);
  });
}

/**
 * Check that case-sensitivity doesn't cause us to duplicate
 * file name extensions.
 */
add_task(async function test_download_filename_extension() {
  let windowObserver = BrowserTestUtils.domWindowOpenedAndLoaded();

  // If browser.download.improvements_to_download_panel pref is enabled,
  // the unknownContentType will not appear by default.
  // So wait an amount of time to ensure it hasn't opened.
  let windowOpenDelay = waitDelay(1000);
  let uctWindow = await Promise.race([windowOpenDelay, windowObserver.promise]);
  const prefEnabled = Services.prefs.getBoolPref(
    "browser.download.improvements_to_download_panel"
  );

  if (prefEnabled) {
    SimpleTest.is(
      !uctWindow,
      true,
      "UnknownContentType window shouldn't open."
    );
    return;
  }

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_PATH + "unknownContentType.EXE",
    waitForLoad: false,
  });
  let win = await windowObserver;

  let list = await Downloads.getList(Downloads.ALL);
  let downloadFinishedPromise = new Promise(resolve => {
    list.addView({
      onDownloadChanged(download) {
        if (download.stopped) {
          list.removeView(this);
          resolve(download);
        }
      },
    });
  });

  let dialog = win.document.querySelector("dialog");
  dialog.getButton("accept").removeAttribute("disabled");
  dialog.acceptDialog();
  let download = await downloadFinishedPromise;
  // We cannot assume that the filename didn't change.
  let filename = PathUtils.filename(download.target.path);
  Assert.ok(
    filename.indexOf(".") == filename.lastIndexOf("."),
    "Should not duplicate extension"
  );
  Assert.ok(filename.endsWith(".EXE"), "Should not change extension");
  await list.remove(download);
  BrowserTestUtils.removeTab(tab);
  try {
    await IOUtils.remove(download.target.path);
  } catch (ex) {
    // Ignore errors in removing the file, the system may keep it locked and
    // it's not a critical issue.
    info("Failed to remove the file " + ex);
  }
});
