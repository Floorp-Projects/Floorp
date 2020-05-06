/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that downloads started from a private window by clicking on a link end
 * up in the global list of private downloads (see bug 1367581).
 */

"use strict";

ChromeUtils.import("resource://gre/modules/Downloads.jsm", this);
ChromeUtils.import("resource://gre/modules/DownloadPaths.jsm", this);
ChromeUtils.import("resource://testing-common/FileTestUtils.jsm", this);
ChromeUtils.import("resource://testing-common/MockRegistrar.jsm", this);

add_task(async function test_setup() {
  // Save downloads to disk without showing the dialog.
  let cid = MockRegistrar.register("@mozilla.org/helperapplauncherdialog;1", {
    QueryInterface: ChromeUtils.generateQI([Ci.nsIHelperAppLauncherDialog]),
    show(launcher) {
      launcher.promptForSaveDestination();
    },
    promptForSaveToFileAsync(launcher) {
      // The dialog should create the empty placeholder file.
      let file = FileTestUtils.getTempFile();
      file.create(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
      launcher.saveDestinationAvailable(file);
    },
  });
  registerCleanupFunction(() => {
    MockRegistrar.unregister(cid);
  });
});

add_task(async function test_download_privatebrowsing() {
  let privateList = await Downloads.getList(Downloads.PRIVATE);
  let publicList = await Downloads.getList(Downloads.PUBLIC);

  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  try {
    let tab = await BrowserTestUtils.openNewForegroundTab(
      win.gBrowser,
      `data:text/html,<a download href="data:text/plain,">download</a>`
    );

    let promiseNextPrivateDownload = new Promise(resolve => {
      privateList.addView({
        onDownloadAdded(download) {
          privateList.removeView(this);
          resolve(download);
        },
      });
    });

    await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
      content.document.querySelector("a").click();
    });

    // Wait for the download to finish so the file can be safely deleted later.
    let download = await promiseNextPrivateDownload;
    await download.whenSucceeded();

    // Clean up after checking that there are no new public downloads either.
    let publicDownloads = await publicList.getAll();
    Assert.equal(publicDownloads.length, 0);
    await privateList.removeFinished();
  } finally {
    await BrowserTestUtils.closeWindow(win);
  }
});
