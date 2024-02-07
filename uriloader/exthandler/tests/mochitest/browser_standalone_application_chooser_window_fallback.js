/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the fallback fixed in bug 1875460, if the modern tab dialog box is not
// supported.

const TEST_URL =
  "https://example.com/browser/" +
  "uriloader/exthandler/tests/mochitest/FTPprotocolHandler.html";

add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["security.external_protocol_requires_permission", false]],
  });

  // Load a page with an FTP link.
  let browser = gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(browser, TEST_URL);
  await BrowserTestUtils.browserLoaded(browser, false, TEST_URL);

  // Make sure no handler is set, forcing the dialog to show.
  let protoSvc = Cc[
    "@mozilla.org/uriloader/external-protocol-service;1"
  ].getService(Ci.nsIExternalProtocolService);
  let protoInfo = protoSvc.getProtocolHandlerInfo("ftp");
  ok(!protoInfo.preferredApplicationHandler, "no preferred handler is set");
  let handlers = protoInfo.possibleApplicationHandlers;
  is(0, handlers.length, "no handler registered for ftp");
  protoInfo.alwaysAskBeforeHandling = true;
  let handlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
    Ci.nsIHandlerService
  );
  handlerSvc.store(protoInfo);

  // Delete getTabDialogBox from gBrowser, to test the fallback to the standalone
  // application chooser window.
  let _getTabDialogBox = gBrowser.getTabDialogBox;
  delete gBrowser.getTabDialogBox;

  let appChooserDialogOpenPromise = BrowserTestUtils.domWindowOpened(
    null,
    async win => {
      await BrowserTestUtils.waitForEvent(win, "load");
      Assert.ok(
        win.document.documentURI ==
          "chrome://mozapps/content/handling/appChooser.xhtml",
        "application chooser dialog opened"
      );
      return true;
    }
  );
  let link = "#link";
  await BrowserTestUtils.synthesizeMouseAtCenter(link, {}, browser);
  let appChooserDialog = await appChooserDialogOpenPromise;

  let appChooserDialogClosePromise =
    BrowserTestUtils.domWindowClosed(appChooserDialog);
  let dialog = appChooserDialog.document.getElementsByTagName("dialog")[0];
  let cancelButton = dialog.getButton("cancel");
  cancelButton.click();
  await appChooserDialogClosePromise;

  // Restore the original getTabDialogBox(), to not affect other tests.
  gBrowser.getTabDialogBox = _getTabDialogBox;
});
