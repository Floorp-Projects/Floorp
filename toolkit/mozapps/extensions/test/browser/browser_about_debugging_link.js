/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {PromiseTestUtils} = ChromeUtils.import("resource://testing-common/PromiseTestUtils.jsm");
// Whitelist rejections related to closing an about:debugging too soon after it has been
// just opened in a new tab and loaded.
PromiseTestUtils.whitelistRejectionsGlobally(/Connection closed/);

function waitForDispatch(store, type) {
  return new Promise(resolve => {
    store.dispatch({
      type: "@@service/waitUntil",
      predicate: action => action.type === type,
      run: (dispatch, getState, action) => {
        resolve(action);
      },
    });
  });
}

function waitForRequestsSuccess(store) {
  return Promise.all([
    waitForDispatch(store, "REQUEST_EXTENSIONS_SUCCESS"),
    waitForDispatch(store, "REQUEST_TABS_SUCCESS"),
    waitForDispatch(store, "REQUEST_WORKERS_SUCCESS"),
  ]);
}

add_task(async function testAboutDebugging() {
  let win = await open_manager(null);
  let categoryUtilities = new CategoryUtilities(win);
  await categoryUtilities.openType("extension");

  let aboutAddonsTab = gBrowser.selectedTab;
  let debugAddonsBtn = win.document.getElementById("utils-debugAddons");

  async function checkUrl(expected, newAboutAddons) {
    // Set the new about:debugging pref.
    await SpecialPowers.pushPrefEnv({
      set: [["devtools.aboutdebugging.new-enabled", newAboutAddons]],
    });

    // Verify the right URL is loaded.
    info(`Check about:debugging URL with new: ${newAboutAddons}`);
    let loaded = BrowserTestUtils.waitForNewTab(gBrowser, expected, true);
    debugAddonsBtn.doCommand();
    await loaded;
    let aboutDebuggingTab = gBrowser.selectedTab;

    if (newAboutAddons) {
      const {AboutDebugging} = aboutDebuggingTab.linkedBrowser.contentWindow;
      // Avoid test failures due to closing the about:debugging tab
      // while it is still initializing.
      info("Wait until about:debugging actions are finished");
      await waitForRequestsSuccess(AboutDebugging.store);
    }

    info("Switch back to about:addons");
    await BrowserTestUtils.switchTab(gBrowser, aboutAddonsTab);
    is(gBrowser.selectedTab, aboutAddonsTab, "Back to about:addons");

    info("Re-open about:debugging");
    let switched = TestUtils.waitForCondition(
      () => gBrowser.selectedTab == aboutDebuggingTab);
    debugAddonsBtn.doCommand();
    await switched;

    info("Remove the about:debugging tab");
    BrowserTestUtils.removeTab(aboutDebuggingTab);

    // Reset prefs.
    await SpecialPowers.popPrefEnv();
  }

  await checkUrl("about:debugging#/runtime/this-firefox", true);
  await checkUrl("about:debugging#addons", false);

  await close_manager(win);
});
