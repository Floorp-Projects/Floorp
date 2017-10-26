/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that downloads started from a private window by clicking on a link end
 * up in the global list of private downloads (see bug 1367581).
 */

"use strict";

Cu.import("resource://gre/modules/Downloads.jsm", this);
Cu.import("resource://gre/modules/DownloadPaths.jsm", this);
Cu.import("resource://testing-common/MockRegistrar.jsm", this);

const TEST_TARGET_FILE_NAME = "test-download.txt";

// While the previous test file should have deleted all the temporary files it
// used, on Windows these might still be pending deletion on the physical file
// system.  Thus, start from a new base number every time, to make a collision
// with a file that is still pending deletion highly unlikely.
let gFileCounter = Math.floor(Math.random() * 1000000);

/**
 * Returns a reference to a temporary file, that is guaranteed not to exist, and
 * to have never been created before.
 *
 * @param aLeafName
 *        Suggested leaf name for the file to be created.
 *
 * @return nsIFile pointing to a non-existent file in a temporary directory.
 *
 * @note It is not enough to delete the file if it exists, or to delete the file
 *       after calling nsIFile.createUnique, because on Windows the delete
 *       operation in the file system may still be pending, preventing a new
 *       file with the same name to be created.
 */
function getTempFile(aLeafName) {
  // Prepend a serial number to the extension in the suggested leaf name.
  let [base, ext] = DownloadPaths.splitBaseNameAndExtension(aLeafName);
  let leafName = base + "-" + gFileCounter + ext;
  gFileCounter++;

  // Get a file reference under the temporary directory for this test file.
  let file = FileUtils.getFile("TmpD", [leafName]);
  Assert.ok(!file.exists());

  registerCleanupFunction(() => {
    try {
      file.remove(false);
    } catch (ex) {
      // On Windows, we may get an access denied error if the file existed
      // before, and was recently deleted.
      if (!(ex instanceof Components.Exception &&
            (ex.result == Cr.NS_ERROR_FILE_ACCESS_DENIED ||
             ex.result == Cr.NS_ERROR_FILE_TARGET_DOES_NOT_EXIST ||
             ex.result == Cr.NS_ERROR_FILE_NOT_FOUND))) {
        throw ex;
      }
    }
  });

  return file;
}

add_task(async function test_setup() {
  // Save downloads to disk without showing the dialog.
  let cid = MockRegistrar.register("@mozilla.org/helperapplauncherdialog;1", {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIHelperAppLauncherDialog]),
    show(launcher) {
      launcher.saveToDisk(null, false);
    },
    promptForSaveToFileAsync(launcher) {
      // The dialog should create the empty placeholder file.
      let file = getTempFile(TEST_TARGET_FILE_NAME);
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
    let tab = await BrowserTestUtils.openNewForegroundTab(win.gBrowser,
      `data:text/html,<a download href="data:text/plain,">download</a>`);

    let promiseNextPrivateDownload = new Promise(resolve => {
      privateList.addView({
        onDownloadAdded(download) {
          privateList.removeView(this);
          resolve(download);
        },
      });
    });

    await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
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
