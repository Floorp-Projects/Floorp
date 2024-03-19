/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * TestCase for bug 1726801
 * <https://bugzilla.mozilla.org/show_bug.cgi?id=1726801>
 *
 * Load an image in a standalone tab and verify that the per-site download
 * folder is correctly retrieved when using "Save Page As" to save the image.
 */

/*
 * ================
 * Helper functions
 * ================
 */

async function setFile(downloadLastDir, aURI, aValue) {
  downloadLastDir.setFile(aURI, aValue);
  await TestUtils.waitForTick();
}

function newDirectory() {
  let dir = FileUtils.getDir("TmpD", ["testdir"]);
  dir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  return dir;
}

function clearHistory() {
  Services.obs.notifyObservers(null, "browser:purge-session-history");
}

async function clearHistoryAndWait() {
  clearHistory();
  await TestUtils.waitForTick();
  await TestUtils.waitForTick();
}

/*
 * ====
 * Test
 * ====
 */

let MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window.browsingContext);

add_task(async function () {
  const IMAGE_URL =
    "http://mochi.test:8888/browser/toolkit/content/tests/browser/doggy.png";

  await BrowserTestUtils.withNewTab(IMAGE_URL, async function () {
    let tmpDir = FileUtils.getDir("TmpD", []);
    let dir = newDirectory();
    let downloadLastDir = new DownloadLastDir(null);
    // Set the desired target directory for the IMAGE_URL
    await setFile(downloadLastDir, IMAGE_URL, dir);
    // Ensure that "browser.download.lastDir" points to a different directory
    await setFile(downloadLastDir, null, tmpDir);
    registerCleanupFunction(async function () {
      await clearHistoryAndWait();
      dir.remove(true);
    });

    // Prepare mock file picker.
    let showFilePickerPromise = new Promise(resolve => {
      MockFilePicker.showCallback = fp => resolve(fp.displayDirectory.path);
    });
    registerCleanupFunction(function () {
      MockFilePicker.cleanup();
    });

    // Run "Save Page As"
    EventUtils.synthesizeKey("s", { accelKey: true });

    let dirPath = await showFilePickerPromise;
    is(dirPath, dir.path, "Verify proposed download folder.");
  });
});
