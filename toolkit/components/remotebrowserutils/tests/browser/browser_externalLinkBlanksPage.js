/*
 * Test that following a link with a scheme that opens externally (like
 * irc:) does not blank the page (Bug 1630757).
 */

const { HandlerServiceTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/HandlerServiceTestUtils.sys.mjs"
);

let gHandlerService = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
  Ci.nsIHandlerService
);

let Pages = [httpURL("dummy_page.html"), fileURL("dummy_page.html")];

/**
 * Creates dummy protocol handler
 */
function initTestHandlers() {
  let handlerInfo =
    HandlerServiceTestUtils.getBlankHandlerInfo("test-proto://");

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

async function runTest() {
  initTestHandlers();

  for (let page of Pages) {
    await BrowserTestUtils.withNewTab(page, async function (aBrowser) {
      await SpecialPowers.spawn(aBrowser, [], async () => {
        let h = content.document.createElement("h1");
        ok(h);
        h.innerHTML = "My heading";
        h.id = "my-heading";
        content.document.body.append(h);
        is(content.document.getElementById("my-heading"), h, "h exists");

        let a = content.document.createElement("a");
        ok(a);
        a.innerHTML = "my link";
        a.id = "my-link";
        content.document.body.append(a);
      });

      await SpecialPowers.spawn(aBrowser, [], async () => {
        let url = "test-proto://some-thing";

        let a = content.document.getElementById("my-link");
        ok(a);
        a.href = url;
        a.click();
      });

      await SpecialPowers.spawn(aBrowser, [], async () => {
        ok(
          content.document.getElementById("my-heading"),
          "Page contents not erased"
        );
      });
    });
  }
  await SpecialPowers.popPrefEnv();
}

add_task(runTest);
