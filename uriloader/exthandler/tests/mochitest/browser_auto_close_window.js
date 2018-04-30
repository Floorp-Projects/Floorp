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
  QueryInterface: ChromeUtils.generateQI([Ci.nsIHelperAppLauncherDialog])
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
  await BrowserTestUtils.withNewTab({ gBrowser, url: URL }, async function(browser) {
    let dialogAppeared = promiseHelperAppDialog();
    await BrowserTestUtils.synthesizeMouseAtCenter("#regular_load", {}, browser);
    let windowContext = await dialogAppeared;

    is(windowContext.gBrowser.selectedBrowser.currentURI.spec, URL,
       "got the right windowContext");
  });
});

// Given a browser pointing to download_page.html, clicks on the link that
// opens with target="_blank" (i.e. a new tab) and ensures that we
// automatically open and close that tab.
async function testNewTab(browser) {
  let targetURL = browser.currentURI.spec;
  let dialogAppeared = promiseHelperAppDialog();
  let tabOpened = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen").then((event) => {
    return [ event.target, BrowserTestUtils.waitForTabClosing(event.target) ];
  });

  await BrowserTestUtils.synthesizeMouseAtCenter("#target_blank", {}, browser);

  let windowContext = await dialogAppeared;
  is(windowContext.gBrowser.selectedBrowser.currentURI.spec, targetURL,
     "got the right windowContext");
  let [ tab, closingPromise ] = await tabOpened;
  await closingPromise;
  is(tab.linkedBrowser, null, "tab was opened and closed");
}

add_task(async function target_blank() {
  // Tests that a link with target=_blank opens a new tab and closes it,
  // returning the window that we're using for navigation.
  await BrowserTestUtils.withNewTab({ gBrowser, url: URL }, async function(browser) {
    await testNewTab(browser);
  });
});

add_task(async function new_window() {
  // Tests that a link that forces us to open a new window (by specifying a
  // width and a height in window.open) opens a new window for the load,
  // realizes that we need to close that window and returns the *original*
  // window as the window context.
  await BrowserTestUtils.withNewTab({ gBrowser, url: URL }, async function(browser) {
    let dialogAppeared = promiseHelperAppDialog();
    let windowOpened = BrowserTestUtils.waitForNewWindow();

    await BrowserTestUtils.synthesizeMouseAtCenter("#new_window", {}, browser);

    let windowContext = await dialogAppeared;
    is(windowContext.gBrowser.selectedBrowser.currentURI.spec, URL,
       "got the right windowContext");
    let win = await windowOpened;

    // The window should close on its own. If not, this test will time out.
    await BrowserTestUtils.domWindowClosed(win);
    ok(win.closed, "window was opened and closed");
  });
});

add_task(async function nested_window_opens() {
  // Tests that the window auto-closing feature works if the download is
  // initiated by a window that, itself, has an opener (see bug 1373109).
  await BrowserTestUtils.withNewTab({ gBrowser, url: URL }, async function(outerBrowser) {
    BrowserTestUtils.synthesizeMouseAtCenter("#open_in_new_tab", {}, outerBrowser);
    let secondTab = await BrowserTestUtils.waitForNewTab(gBrowser, null, true);
    let nestedBrowser = secondTab.linkedBrowser;

    await ContentTask.spawn(nestedBrowser, null, function() {
      ok(content.opener, "this window has an opener");
    });

    await testNewTab(nestedBrowser);

    isnot(secondTab.linkedBrowser, null, "the page that triggered the download is still open");
    BrowserTestUtils.removeTab(secondTab);
  });
});

add_task(async function cleanup() {
  // Unregister our factory from XPCOM and restore the original CID.
  registrar.unregisterFactory(MOCK_HELPERAPP_DIALOG_CID, mockHelperAppService);
  registrar.registerFactory(HELPERAPP_DIALOG_CID, "",
                            HELPERAPP_DIALOG_CONTRACT_ID, null);
});
