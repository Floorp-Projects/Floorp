/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test for bug 1209344 <https://bugzilla.mozilla.org/show_bug.cgi?id=1209344>:
 * Provide link to about:debugging#addons
 */

var gManagerWindow;

const URI_ABOUT_DEBUGGING = "about:debugging#addons";

function test() {
  waitForExplicitFinish();

  open_manager("addons://list/extension", (aManager) => {
    gManagerWindow = aManager;

    test_about_debugging(URI_ABOUT_DEBUGGING, () => {
        close_manager(gManagerWindow, finish);
    });
  });
}

function test_about_debugging(aExpectedAboutUri, aCallback) {
  info("Waiting for about:debugging tab");
  gBrowser.tabContainer.addEventListener("TabOpen", function listener(event) {
    gBrowser.tabContainer.removeEventListener("TabOpen", listener, true);
    function wantLoad(url) {
      return url != "about:blank";
    }
    BrowserTestUtils.browserLoaded(event.target.linkedBrowser, false, wantLoad).then(() => {
      is(gBrowser.currentURI.spec, URI_ABOUT_DEBUGGING, "Should have loaded the right page");

      gBrowser.removeCurrentTab();

      if (gUseInContentUI) {
        is(gBrowser.currentURI.spec, "about:addons", "Should be back to the add-ons manager");
        run_next_test();
      }
      else {
        waitForFocus(run_next_test, gManagerWindow);
      }
    });
  });


  gManagerWindow.gViewController.doCommand("cmd_debugAddons");
}

function end_test() {
  close_manager(gManagerWindow, () => {
    finish();
  });
}
