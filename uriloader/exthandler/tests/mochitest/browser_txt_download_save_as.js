/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { DownloadIntegration } = ChromeUtils.importESModule(
  "resource://gre/modules/DownloadIntegration.sys.mjs"
);
const HandlerService = Cc[
  "@mozilla.org/uriloader/handler-service;1"
].getService(Ci.nsIHandlerService);
const MIMEService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

const {
  saveToDisk,
  alwaysAsk,
  handleInternally,
  useHelperApp,
  useSystemDefault,
} = Ci.nsIHandlerInfo;
const testDir = createTemporarySaveDirectory();
const MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
  this
);

async function testSaveAsDownload() {
  await BrowserTestUtils.withNewTab(
    `data:text/html,<a id="test-link" href="${TEST_PATH}/file_txt_attachment_test.txt">Test TXT Link</a>`,
    async browser => {
      let menu = document.getElementById("contentAreaContextMenu");
      ok(menu, "Context menu exists on the page");

      let popupShown = BrowserTestUtils.waitForEvent(menu, "popupshown");
      BrowserTestUtils.synthesizeMouseAtCenter(
        "a#test-link",
        { type: "contextmenu", button: 2 },
        browser
      );
      await popupShown;
      info("Context menu popup was successfully displayed");

      let filePickerPromise = setupFilePicker();

      info("Clicking Save As... context menu");
      let menuitem = menu.querySelector("#context-savelink");
      menu.activateItem(menuitem);
      await filePickerPromise;
    }
  );
}

async function setupFilePicker() {
  return new Promise(resolve => {
    MockFilePicker.returnValue = MockFilePicker.returnOK;
    MockFilePicker.displayDirectory = testDir;
    MockFilePicker.showCallback = fp => {
      ok(true, "filepicker should be visible");
      ok(
        fp.defaultExtension === "txt",
        "Default extension in filepicker should be txt"
      );
      ok(
        fp.defaultString === "file_txt_attachment_test.txt",
        "Default string name in filepicker should have the correct file name"
      );
      const destFile = testDir.clone();
      destFile.append(fp.defaultString);
      MockFilePicker.setFiles([destFile]);

      mockTransferCallback = success => {
        ok(success, "File should have been downloaded successfully");
        ok(destFile.exists(), "File should exist in test directory");
        resolve(destFile);
      };
    };
  });
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.always_ask_before_handling_new_types", false],
      ["browser.download.useDownloadDir", false],
    ],
  });
  mockTransferRegisterer.register();

  let oldLaunchFile = DownloadIntegration.launchFile;
  DownloadIntegration.launchFile = () => {
    ok(false, "Download should not have launched");
  };

  registerCleanupFunction(async () => {
    DownloadIntegration.launchFile = oldLaunchFile;
    mockTransferRegisterer.unregister();

    // We only want to run MockFilerPicker.cleanup after the entire test is run.
    // Otherwise, we cannot use MockFilePicker for each preferredAction.
    MockFilePicker.cleanup();

    testDir.remove(true);
    ok(!testDir.exists(), "Test directory should be removed");
  });
});

/**
 * Tests that selecting the context menu item `Save Link As…` on a txt file link
 * opens the file picker and only downloads the file without any launches when
 * browser.download.always_ask_before_handling_new_types is disabled.
 */
add_task(async function test_txt_save_as_link() {
  let mimeInfo;

  for (let preferredAction of [
    saveToDisk,
    alwaysAsk,
    handleInternally,
    useHelperApp,
    useSystemDefault,
  ]) {
    mimeInfo = MIMEService.getFromTypeAndExtension("text/plain", "txt");
    mimeInfo.alwaysAskBeforeHandling = preferredAction === alwaysAsk;
    mimeInfo.preferredAction = preferredAction;
    HandlerService.store(mimeInfo);

    info(
      `Setting up filepicker with preferredAction ${preferredAction} and ask = ${mimeInfo.alwaysAskBeforeHandling}`
    );
    await testSaveAsDownload(mimeInfo);
  }
});

/**
 * Tests that selecting the context menu item `Save Link As…` on a txt file link
 * opens the file picker and only downloads the file without any launches when
 * browser.download.always_ask_before_handling_new_types is disabled. For this
 * particular test, set alwaysAskBeforeHandling to true.
 */
add_task(async function test_txt_save_as_link_alwaysAskBeforeHandling() {
  let mimeInfo;

  for (let preferredAction of [
    saveToDisk,
    alwaysAsk,
    handleInternally,
    useHelperApp,
    useSystemDefault,
  ]) {
    mimeInfo = MIMEService.getFromTypeAndExtension("text/plain", "txt");
    mimeInfo.alwaysAskBeforeHandling = true;
    mimeInfo.preferredAction = preferredAction;
    HandlerService.store(mimeInfo);

    info(
      `Setting up filepicker with preferredAction ${preferredAction} and ask = ${mimeInfo.alwaysAskBeforeHandling}`
    );
    await testSaveAsDownload(mimeInfo);
  }
});
