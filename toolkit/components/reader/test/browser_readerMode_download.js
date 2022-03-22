/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// This test verifies that the toolbar is hidden when saving in reader mode

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

/* import-globals-from ../../../../../toolkit/content/tests/browser/common/mockTransfer.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
  this
);

function createTemporarySaveDirectory() {
  var saveDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  if (!saveDir.exists()) {
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  }
  return saveDir;
}

function downloadHadFinished(publicList) {
  return new Promise(resolve => {
    publicList.addView({
      onDownloadChanged(download) {
        if (download.succeeded || download.error) {
          publicList.removeView(this);
          resolve(download);
        }
      },
    });
  });
}

/**
 * Test that the reader modes toolbar is hidden on page download
 */
add_task(async function() {
  // Open the page in reader mode
  await BrowserTestUtils.withNewTab(
    TEST_PATH + "readerModeArticleShort.html",
    async function(browser) {
      let pageShownPromise = BrowserTestUtils.waitForContentEvent(
        browser,
        "AboutReaderContentReady"
      );
      let readerButton = document.getElementById("reader-mode-button");
      readerButton.click();
      await pageShownPromise;
      // Reader mode page open -- Begin downloading the Page
      var fileName;

      var destDir = createTemporarySaveDirectory();
      var destFile = destDir.clone();
      MockFilePicker.displayDirectory = destDir;
      MockFilePicker.showCallback = function(fp) {
        fileName = fp.defaultString;
        destFile.append(fileName);
        MockFilePicker.setFiles([destFile]);
        MockFilePicker.filterIndex = 1; // kSaveAsType_URL
      };

      let fileSavePageAsElement = document.getElementById("menu_savePage");
      fileSavePageAsElement.doCommand();

      // Wait for the download to complete
      let publicList = await Downloads.getList(Downloads.PUBLIC);

      let downloadFinishedPromise = downloadHadFinished(publicList);

      let download = await downloadFinishedPromise;

      // Open the downloaded page
      let fileDir = PathUtils.join(download.target.path);
      let loadPromise = BrowserTestUtils.browserLoaded(
        gBrowser.selectedBrowser
      );
      BrowserTestUtils.loadURI(gBrowser.selectedBrowser, fileDir);
      await loadPromise;
      // Check that the toolbar is hidden
      await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
        Assert.ok(
          content.document.getElementById("toolbar").hidden,
          "The toolbar is hidden"
        );
      });
    }
  );
});
