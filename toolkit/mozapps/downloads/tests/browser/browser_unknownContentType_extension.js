/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

/**
 * Check that case-sensitivity doesn't cause us to duplicate
 * file name extensions.
 */
add_task(async function test_download_filename_extension() {
  let windowObserver = BrowserTestUtils.domWindowOpenedAndLoaded();
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
  let f = new FileUtils.File(download.target.path);
  // We cannot assume that the filename is a particular
  let extensions = f.leafName.substring(f.leafName.indexOf("."));
  is(extensions, ".EXE", "Should not duplicate extension");
  await list.remove(download);
  f.remove(true);
  BrowserTestUtils.removeTab(tab);
});
