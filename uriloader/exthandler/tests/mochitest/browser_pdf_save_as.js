/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const HandlerService = Cc[
  "@mozilla.org/uriloader/handler-service;1"
].getService(Ci.nsIHandlerService);
const MIMEService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

const { saveToDisk, alwaysAsk, handleInternally, useSystemDefault } =
  Ci.nsIHandlerInfo;
const MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

async function testPdfFilePicker(mimeInfo) {
  await BrowserTestUtils.withNewTab(
    `data:text/html,<a id="test-link" href="${TEST_PATH}/file_pdf_application_pdf.pdf">Test PDF Link</a>`,
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

      let filePickerPromise = new Promise(resolve => {
        MockFilePicker.showCallback = fp => {
          ok(true, "filepicker should be visible");
          ok(
            fp.defaultExtension === "pdf",
            "Default extension in filepicker should be pdf"
          );
          ok(
            fp.defaultString === "file_pdf_application_pdf.pdf",
            "Default string name in filepicker should have the correct pdf file name"
          );
          setTimeout(resolve, 0);
          return Ci.nsIFilePicker.returnCancel;
        };
      });

      let menuitem = menu.querySelector("#context-savelink");
      menu.activateItem(menuitem);
      await filePickerPromise;
    }
  );
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.always_ask_before_handling_new_types", false],
      ["browser.download.useDownloadDir", false],
    ],
  });

  registerCleanupFunction(async () => {
    let publicList = await Downloads.getList(Downloads.PUBLIC);
    await publicList.removeFinished();

    if (DownloadsPanel.isVisible) {
      info("Closing downloads panel");
      let hiddenPromise = BrowserTestUtils.waitForEvent(
        DownloadsPanel.panel,
        "popuphidden"
      );
      DownloadsPanel.hidePanel();
      await hiddenPromise;
    }

    let mimeInfo = MIMEService.getFromTypeAndExtension(
      "application/pdf",
      "pdf"
    );
    let existed = HandlerService.exists(mimeInfo);
    if (existed) {
      HandlerService.store(mimeInfo);
    } else {
      HandlerService.remove(mimeInfo);
    }

    // We only want to run MockFilerPicker.cleanup after the entire test is run.
    // Otherwise, we cannot use MockFilePicker for each preferredAction.
    MockFilePicker.cleanup();
  });
});

/**
 * Tests that selecting the context menu item `Save Link As…` on a PDF link
 * opens the file picker when always_ask_before_handling_new_types is disabled,
 * regardless of preferredAction.
 */
add_task(async function test_pdf_save_as_link() {
  let mimeInfo;

  for (let preferredAction of [
    saveToDisk,
    alwaysAsk,
    handleInternally,
    useSystemDefault,
  ]) {
    mimeInfo = MIMEService.getFromTypeAndExtension("application/pdf", "pdf");
    mimeInfo.alwaysAskBeforeHandling = preferredAction === alwaysAsk;
    mimeInfo.preferredAction = preferredAction;
    HandlerService.store(mimeInfo);

    info(`Testing filepicker for preferredAction ${preferredAction}`);
    await testPdfFilePicker(mimeInfo);
  }
});
