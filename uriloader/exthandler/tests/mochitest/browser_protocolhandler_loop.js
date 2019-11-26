/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_helperapp() {
  // Set up the test infrastructure:
  const kProt = "foopydoopydoo";
  const extProtocolSvc = Cc[
    "@mozilla.org/uriloader/external-protocol-service;1"
  ].getService(Ci.nsIExternalProtocolService);
  const handlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
    Ci.nsIHandlerService
  );
  let handlerInfo = extProtocolSvc.getProtocolHandlerInfo(kProt);
  if (handlerSvc.exists(handlerInfo)) {
    handlerSvc.fillHandlerInfo(handlerInfo, "");
  }
  // Say we want to use a specific app:
  handlerInfo.preferredAction = Ci.nsIHandlerInfo.useHelperApp;
  handlerInfo.alwaysAskBeforeHandling = false;

  // Say it's us:
  let selfFile = Services.dirsvc.get("XREExeF", Ci.nsIFile);
  // Make sure it's the .app
  if (AppConstants.platform == "macosx") {
    while (
      !selfFile.leafName.endsWith(".app") &&
      !selfFile.leafName.endsWith(".app/")
    ) {
      selfFile = selfFile.parent;
    }
  }
  let selfHandlerApp = Cc[
    "@mozilla.org/uriloader/local-handler-app;1"
  ].createInstance(Ci.nsILocalHandlerApp);
  selfHandlerApp.executable = selfFile;
  handlerInfo.possibleApplicationHandlers.appendElement(selfHandlerApp);
  handlerInfo.preferredApplicationHandler = selfHandlerApp;
  handlerSvc.store(handlerInfo);

  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    // Now, do some safety stubbing. If we do end up recursing we spawn
    // infinite tabs. We definitely don't want that. Avoid it by stubbing
    // our external URL handling bits:
    let oldAddTab = gBrowser.addTab;
    registerCleanupFunction(
      () => (gBrowser.addTab = gBrowser.loadOneTab = oldAddTab)
    );
    let wrongThingHappenedPromise = new Promise(resolve => {
      gBrowser.addTab = gBrowser.loadOneTab = function(aURI) {
        ok(false, "Tried to open unexpected URL in a tab: " + aURI);
        resolve(null);
        // Pass a dummy object to avoid upsetting BrowserContentHandler -
        // if it thinks opening the tab failed, it tries to open a window instead,
        // which we can't prevent as easily, and at which point we still end up
        // with runaway tabs.
        return {};
      };
    });
    // We can't use TestUtils.topicObserved because it leaks.
    let askedUserPromise = new Promise(r => {
      let obs = () => {
        r("yes");
        Services.obs.removeObserver(obs, "domwindowopened");
      };
      Services.obs.addObserver(obs, "domwindowopened");
    });
    BrowserTestUtils.loadURI(browser, kProt + ":test");
    let win = await Promise.race([wrongThingHappenedPromise, askedUserPromise]);
    ok(win, "Should have gotten a window");
    // This is really annoying. Hanging on to the window from the observer
    // leaks for some reason. Just close it now. It has no window type, so use
    // the lack of one to distinguish it from the browser and the harness.
    for (let openWin of Services.wm.getEnumerator("")) {
      if (!openWin.document.documentElement.getAttribute("windowtype")) {
        openWin.close();
      }
    }
    askedUserPromise = null;
  });
});
