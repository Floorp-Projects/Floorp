/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const URL =
  getRootDirectory(gTestPath).replace("chrome://mochitests/content",
                                      "https://example.com") +
    "download_page.html";

const HELPERAPP_DIALOG_CONTRACT_ID = "@mozilla.org/helperapplauncherdialog;1";
const HELPERAPP_DIALOG_CID =
        Components.ID(Cc[HELPERAPP_DIALOG_CONTRACT_ID].number);
const MOCK_HELPERAPP_DIALOG_CID =
        Components.ID("{2f372b6f-56c9-46d5-af0d-9f09bb69860c}");

let registrar = Components.manager
                          .QueryInterface(Ci.nsIComponentRegistrar);
let curDialogResolve = null;

function HelperAppLauncherDialog() {
}

HelperAppLauncherDialog.prototype = {
  show: function(aLauncher, aWindowContext, aReason) {
    ok(true, "Showing the helper app dialog");
    curDialogResolve(aWindowContext);
    executeSoon(() => { aLauncher.cancel(Cr.NS_ERROR_ABORT); });
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIHelperAppLauncherDialog])
};

function promiseHelperAppDialog() {
  return new Promise((resolve) => {
    curDialogResolve = resolve;
  });
}

let mockHelperAppService;

add_task(async function setup() {
  // Replace the real helper app dialog with our own.
  mockHelperAppService = XPCOMUtils._getFactory(HelperAppLauncherDialog);
  registrar.registerFactory(MOCK_HELPERAPP_DIALOG_CID, "",
                            HELPERAPP_DIALOG_CONTRACT_ID,
                            mockHelperAppService);
});

add_task(async function simple_navigation() {
  // Tests that simple navigation gives us the right windowContext (that is,
  // the window that we're using).
  await BrowserTestUtils.withNewTab({ gBrowser, url: URL }, function* (browser) {
    let dialogAppeared = promiseHelperAppDialog();
    yield BrowserTestUtils.synthesizeMouseAtCenter("#regular_load", {}, browser);
    let windowContext = yield dialogAppeared;

    is(windowContext.gBrowser.selectedBrowser.currentURI.spec, URL,
       "got the right windowContext");
  });
});

add_task(async function target_blank() {
  // Tests that a link with target=_blank opens a new tab and closes it,
  // returning the window that we're using for navigation.
  await BrowserTestUtils.withNewTab({ gBrowser, url: URL }, function* (browser) {
    let dialogAppeared = promiseHelperAppDialog();
    let tabOpened = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen").then((event) => {
      return event.target;
    });

    yield BrowserTestUtils.synthesizeMouseAtCenter("#target_blank", {}, browser);

    let windowContext = yield dialogAppeared;
    is(windowContext.gBrowser.selectedBrowser.currentURI.spec, URL,
       "got the right windowContext");
    let tab = yield tabOpened;
    is(tab.linkedBrowser, null, "tab was opened and closed");
  });
});

add_task(async function new_window() {
  // Tests that a link that forces us to open a new window (by specifying a
  // width and a height in window.open) opens a new window for the load,
  // realizes that we need to close that window and returns the *original*
  // window as the window context.
  await BrowserTestUtils.withNewTab({ gBrowser, url: URL }, function* (browser) {
    let dialogAppeared = promiseHelperAppDialog();
    let windowOpened = BrowserTestUtils.waitForNewWindow(false);

    yield BrowserTestUtils.synthesizeMouseAtCenter("#new_window", {}, browser);

    let windowContext = yield dialogAppeared;
    is(windowContext.gBrowser.selectedBrowser.currentURI.spec, URL,
       "got the right windowContext");
    let win = yield windowOpened;
    is(win.closed, true, "window was opened and closed");
  });
});

add_task(async function cleanup() {
  // Unregister our factory from XPCOM and restore the original CID.
  registrar.unregisterFactory(MOCK_HELPERAPP_DIALOG_CID, mockHelperAppService);
  registrar.registerFactory(HELPERAPP_DIALOG_CID, "",
                            HELPERAPP_DIALOG_CONTRACT_ID, null);
});
