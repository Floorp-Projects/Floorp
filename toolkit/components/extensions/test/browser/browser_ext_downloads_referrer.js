/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

const { BrowserTestUtils } = ChromeUtils.import(
  "resource://testing-common/BrowserTestUtils.jsm"
);

const URL_PATH = "browser/toolkit/components/extensions/test/browser/data";
const TEST_URL = `http://example.com/${URL_PATH}/test_downloads_referrer.html`;
const DOWNLOAD_URL = `http://example.com/${URL_PATH}/test-download.txt`;

async function triggerSaveAs({ selector }) {
  const contextMenu = window.document.getElementById("contentAreaContextMenu");
  const popupshown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  await BrowserTestUtils.synthesizeMouseAtCenter(
    selector,
    { type: "contextmenu", button: 2 },
    gBrowser.selectedBrowser
  );
  await popupshown;
  let saveLinkCommand = window.document.getElementById("context-savelink");
  contextMenu.activateItem(saveLinkCommand);
}

add_setup(() => {
  const tempDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  tempDir.append("test-download-dir");
  if (!tempDir.exists()) {
    tempDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  }

  let MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(window);
  registerCleanupFunction(function() {
    MockFilePicker.cleanup();

    if (tempDir.exists()) {
      tempDir.remove(true);
    }
  });

  MockFilePicker.displayDirectory = tempDir;
  MockFilePicker.showCallback = function(fp) {
    info("MockFilePicker: shown");
    const filename = fp.defaultString;
    info("MockFilePicker: save as " + filename);
    const destFile = tempDir.clone();
    destFile.append(filename);
    MockFilePicker.setFiles([destFile]);
    info("MockFilePicker: showCallback done");
  };
});

add_task(async function test_download_item_referrer_info() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["downloads"],
    },
    async background() {
      browser.downloads.onCreated.addListener(async downloadInfo => {
        browser.test.sendMessage("download-on-created", downloadInfo);
      });
      browser.downloads.onChanged.addListener(async downloadInfo => {
        // Wait download to be completed.
        if (downloadInfo.state?.current !== "complete") {
          return;
        }
        browser.test.sendMessage("download-completed");
      });

      // Call an API method implemented in the parent process to make sure
      // registering the downloas.onCreated event listener has been completed.
      await browser.runtime.getBrowserInfo();

      browser.test.sendMessage("bg-page:ready");
    },
  });

  await extension.startup();
  await extension.awaitMessage("bg-page:ready");

  await BrowserTestUtils.withNewTab({ gBrowser, url: TEST_URL }, async () => {
    await triggerSaveAs({ selector: "a.test-link" });
    const downloadInfo = await extension.awaitMessage("download-on-created");
    is(downloadInfo.url, DOWNLOAD_URL, "Got the expected download url");
    is(downloadInfo.referrer, TEST_URL, "Got the expected referrer");
  });

  // Wait for the download to have been completed and removed.
  await extension.awaitMessage("download-completed");

  await extension.unload();
});
