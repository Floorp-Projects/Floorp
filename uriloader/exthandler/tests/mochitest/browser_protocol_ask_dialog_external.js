/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gHandlerService = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
  Ci.nsIHandlerService
);

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

/**
 * Creates dummy protocol handler
 */
function initTestHandlers() {
  let handlerInfoThatAsks =
    HandlerServiceTestUtils.getBlankHandlerInfo("local-app-test");

  let appHandler = Cc[
    "@mozilla.org/uriloader/local-handler-app;1"
  ].createInstance(Ci.nsILocalHandlerApp);
  // This is a dir and not executable, but that's enough for here.
  appHandler.executable = Services.dirsvc.get("XCurProcD", Ci.nsIFile);
  handlerInfoThatAsks.possibleApplicationHandlers.appendElement(appHandler);
  handlerInfoThatAsks.preferredApplicationHandler = appHandler;
  handlerInfoThatAsks.preferredAction = handlerInfoThatAsks.useHelperApp;
  handlerInfoThatAsks.alwaysAskBeforeHandling = false;
  gHandlerService.store(handlerInfoThatAsks);

  let webHandlerInfo =
    HandlerServiceTestUtils.getBlankHandlerInfo("web+somesite");
  let webHandler = Cc[
    "@mozilla.org/uriloader/web-handler-app;1"
  ].createInstance(Ci.nsIWebHandlerApp);
  webHandler.name = "Somesite";
  webHandler.uriTemplate = "https://example.com/handle_url?u=%s";
  webHandlerInfo.possibleApplicationHandlers.appendElement(webHandler);
  webHandlerInfo.preferredApplicationHandler = webHandler;
  webHandlerInfo.preferredAction = webHandlerInfo.useHelperApp;
  webHandlerInfo.alwaysAskBeforeHandling = false;
  gHandlerService.store(webHandlerInfo);

  registerCleanupFunction(() => {
    gHandlerService.remove(webHandlerInfo);
    gHandlerService.remove(handlerInfoThatAsks);
  });
}

function makeCmdLineHelper(url) {
  return Cu.createCommandLine(
    ["-url", url],
    null,
    Ci.nsICommandLine.STATE_REMOTE_EXPLICIT
  );
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["network.protocol-handler.prompt-from-external", true]],
  });
  initTestHandlers();
});

/**
 * Check that if we get a direct request from another app / the OS to open a
 * link, we always prompt, even if we think we know what the correct answer
 * is. This is to avoid infinite loops in such situations where the OS and
 * Firefox have conflicting ideas about the default handler, or where our
 * checks with the OS don't work (Linux and/or Snap, at time of this comment).
 */
add_task(async function test_external_asks_anyway() {
  let cmdLineHandler = Cc["@mozilla.org/browser/final-clh;1"].getService(
    Ci.nsICommandLineHandler
  );
  let chooserDialogOpenPromise = waitForProtocolAppChooserDialog(
    gBrowser,
    true
  );
  let fakeCmdLine = makeCmdLineHelper("local-app-test:dummy");
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

/**
 * Like the previous test, but avoid asking for web and extension handlers,
 * as we can open those ourselves without looping.
 */
add_task(async function test_web_app_doesnt_ask() {
  // Listen for a dialog open and fail the test if it does:
  let dialogOpenListener = () => ok(false, "Shouldn't have opened a dialog!");
  document.documentElement.addEventListener("dialogopen", dialogOpenListener);
  registerCleanupFunction(() =>
    document.documentElement.removeEventListener(
      "dialogopen",
      dialogOpenListener
    )
  );

  // Set up a promise for a tab to open with the right URL:
  const kURL = "web+somesite:dummy";
  const kLoadedURL =
    "https://example.com/handle_url?u=" + encodeURIComponent(kURL);
  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, kLoadedURL);

  // Load the URL:
  let cmdLineHandler = Cc["@mozilla.org/browser/final-clh;1"].getService(
    Ci.nsICommandLineHandler
  );
  let fakeCmdLine = makeCmdLineHelper(kURL);
  cmdLineHandler.handle(fakeCmdLine);

  // Check that the tab loaded. If instead the dialog opened, the dialogopen handler
  // will fail the test.
  let tab = await tabPromise;
  is(
    tab.linkedBrowser.currentURI.spec,
    kLoadedURL,
    "Should have opened the right URL."
  );
  BrowserTestUtils.removeTab(tab);

  // We do this both here and in cleanup so it's easy to add tasks to this test,
  // and so we clean up correctly if the test aborts before we get here.
  document.documentElement.removeEventListener(
    "dialogopen",
    dialogOpenListener
  );
});

add_task(async function external_https_redirect_doesnt_ask() {
  Services.perms.addFromPrincipal(
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "https://example.com"
    ),
    "open-protocol-handler^local-app-test",
    Services.perms.ALLOW_ACTION
  );
  // Listen for a dialog open and fail the test if it does:
  let dialogOpenListener = () => ok(false, "Shouldn't have opened a dialog!");
  document.documentElement.addEventListener("dialogopen", dialogOpenListener);
  registerCleanupFunction(() => {
    document.documentElement.removeEventListener(
      "dialogopen",
      dialogOpenListener
    );
    Services.perms.removeAll();
  });

  let initialTab = gBrowser.selectedTab;

  gHandlerService.wrappedJSObject.mockProtocolHandler("local-app-test");
  registerCleanupFunction(() =>
    gHandlerService.wrappedJSObject.mockProtocolHandler()
  );

  // Set up a promise for an app to have launched with the right URI:
  let loadPromise = TestUtils.topicObserved("mocked-protocol-handler");

  // Load the URL:
  const kURL = "local-app-test:redirect";
  let cmdLineHandler = Cc["@mozilla.org/browser/final-clh;1"].getService(
    Ci.nsICommandLineHandler
  );
  let fakeCmdLine = makeCmdLineHelper(
    TEST_PATH + "redirect_helper.sjs?uri=" + encodeURIComponent(kURL)
  );
  cmdLineHandler.handle(fakeCmdLine);

  // Check that the mock app was launched. If the dialog showed instead,
  // the test will fail.
  let [uri] = await loadPromise;
  is(uri.spec, "local-app-test:redirect", "Should have seen correct URI.");
  // We might have opened a blank tab, see bug 1718104 and friends.
  if (gBrowser.selectedTab != initialTab) {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }

  // We do this both here and in cleanup so it's easy to add tasks to this test,
  // and so we clean up correctly if the test aborts before we get here.
  document.documentElement.removeEventListener(
    "dialogopen",
    dialogOpenListener
  );
  gHandlerService.wrappedJSObject.mockProtocolHandler();
});
