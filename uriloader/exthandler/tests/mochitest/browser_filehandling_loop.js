/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * If the user has set Firefox itself as a helper app,
 * we should force prompting what to do, rather than ending up
 * in an infinite loop.
 * In an ideal world, we'd also test the case where we are the OS
 * default handler app, but that would require test infrastructure
 * to make ourselves the OS default (or at least fool ourselves into
 * believing we are) which we don't have...
 */
add_task(async function test_helperapp() {
  // Set up the test infrastructure:
  const mimeSvc = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  const handlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
    Ci.nsIHandlerService
  );
  let handlerInfo = mimeSvc.getFromTypeAndExtension("application/x-foo", "foo");
  registerCleanupFunction(() => {
    handlerSvc.remove(handlerInfo);
  });
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
    registerCleanupFunction(() => (gBrowser.addTab = oldAddTab));
    let wrongThingHappenedPromise = new Promise(resolve => {
      gBrowser.addTab = function (aURI) {
        ok(false, "Tried to open unexpected URL in a tab: " + aURI);
        resolve(null);
        // Pass a dummy object to avoid upsetting BrowserContentHandler -
        // if it thinks opening the tab failed, it tries to open a window instead,
        // which we can't prevent as easily, and at which point we still end up
        // with runaway tabs.
        return {};
      };
    });

    let askedUserPromise = BrowserTestUtils.domWindowOpenedAndLoaded();

    info("Clicking a link that should open the unknown content type dialog");
    await SpecialPowers.spawn(browser, [], () => {
      let link = content.document.createElement("a");
      link.download = "foo.foo";
      link.textContent = "Foo file";
      link.href = "data:application/x-foo,hello";
      content.document.body.append(link);
      link.click();
    });
    let dialog = await Promise.race([
      wrongThingHappenedPromise,
      askedUserPromise,
    ]);
    ok(dialog, "Should have gotten a dialog");
    Assert.stringContains(
      dialog.document.location.href,
      "unknownContentType",
      "Should have opened correct dialog."
    );

    let closePromise = BrowserTestUtils.windowClosed(dialog);
    dialog.close();
    await closePromise;
    askedUserPromise = null;
  });
});
