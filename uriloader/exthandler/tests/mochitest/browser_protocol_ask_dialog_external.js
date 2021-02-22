/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gHandlerService = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
  Ci.nsIHandlerService
);

/**
 * Creates dummy protocol handler
 */
function initTestHandlers() {
  let handlerInfo = HandlerServiceTestUtils.getBlankHandlerInfo("yoink");

  let appHandler = Cc[
    "@mozilla.org/uriloader/local-handler-app;1"
  ].createInstance(Ci.nsILocalHandlerApp);
  // This is a dir and not executable, but that's enough for here.
  appHandler.executable = Services.dirsvc.get("XCurProcD", Ci.nsIFile);
  handlerInfo.possibleApplicationHandlers.appendElement(appHandler);
  handlerInfo.preferredApplicationHandler = appHandler;
  handlerInfo.preferredAction = handlerInfo.useHelperApp;
  handlerInfo.alwaysAskBeforeHandling = false;
  gHandlerService.store(handlerInfo);
  registerCleanupFunction(() => {
    gHandlerService.remove(handlerInfo);
  });
}

/**
 * Check that if we get a direct request from another app / the OS to open a
 * link, we always prompt, even if we think we know what the correct answer
 * is. This is to avoid infinite loops in such situations where the OS and
 * Firefox have conflicting ideas about the default handler, or where our
 * checks with the OS don't work (Linux and/or Snap, at time of this comment).
 */
add_task(async function test_external_asks_anyway() {
  await SpecialPowers.pushPrefEnv({
    set: [["network.protocol-handler.prompt-from-external", true]],
  });
  initTestHandlers();

  let cmdLineHandler = Cc["@mozilla.org/browser/final-clh;1"].getService(
    Ci.nsICommandLineHandler
  );
  let fakeCmdLine = {
    length: 1,
    _arg: "yoink:yoink",

    getArgument(aIndex) {
      if (aIndex == 0) {
        return this._arg;
      }
      throw Components.Exception("", Cr.NS_ERROR_INVALID_ARG);
    },

    findFlag() {
      return -1;
    },

    handleFlagWithParam() {
      if (this._argCount) {
        this._argCount = 0;
        return this._arg;
      }

      return "";
    },

    state: 2,

    STATE_INITIAL_LAUNCH: 0,
    STATE_REMOTE_AUTO: 1,
    STATE_REMOTE_EXPLICIT: 2,

    preventDefault: false,

    resolveURI() {
      return Services.io.newURI(this._arg);
    },
    QueryInterface: ChromeUtils.generateQI(["nsICommandLine"]),
  };
  let chooserDialogOpenPromise = waitForProtocolAppChooserDialog(
    gBrowser,
    true
  );
  cmdLineHandler.handle(fakeCmdLine);
  let dialog = await chooserDialogOpenPromise;
  ok(dialog, "Should have prompted.");

  let dialogClosedPromise = waitForProtocolAppChooserDialog(
    gBrowser.selectedBrowser,
    false
  );
  let dialogEl = dialog._frame.contentDocument.querySelector("dialog");
  dialogEl.cancelDialog();
  await dialogClosedPromise;
  // We will have opened a tab; close it.
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
