/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
// Allow rejections related to closing an about:debugging too soon after it has been
// just opened in a new tab and loaded.
PromiseTestUtils.allowMatchingRejectionsGlobally(/Connection closed/);

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

/**
 * Wait for all client requests to settle, meaning here that no new request has been
 * dispatched after the provided delay. (NOTE: same test helper used in about:debugging tests)
 */
async function waitForRequestsToSettle(store, delay = 500) {
  let hasSettled = false;

  // After each iteration of this while loop, we check is the timerPromise had the time
  // to resolve or if we captured a REQUEST_*_SUCCESS action before.
  while (!hasSettled) {
    let timer;

    // This timer will be executed only if no REQUEST_*_SUCCESS action is dispatched
    // during the delay. We consider that when no request are received for some time, it
    // means there are no ongoing requests anymore.
    const timerPromise = new Promise(resolve => {
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      timer = setTimeout(() => {
        hasSettled = true;
        resolve();
      }, delay);
    });

    // Wait either for a REQUEST_*_SUCCESS to be dispatched, or for the timer to resolve.
    await Promise.race([
      waitForDispatch(store, "REQUEST_EXTENSIONS_SUCCESS"),
      waitForDispatch(store, "REQUEST_TABS_SUCCESS"),
      waitForDispatch(store, "REQUEST_WORKERS_SUCCESS"),
      timerPromise,
    ]);

    // Clear the timer to avoid setting hasSettled to true accidently unless timerPromise
    // was the first to resolve.
    clearTimeout(timer);
  }
}

function waitForRequestsSuccess(store) {
  return Promise.all([
    waitForDispatch(store, "REQUEST_EXTENSIONS_SUCCESS"),
    waitForDispatch(store, "REQUEST_TABS_SUCCESS"),
    waitForDispatch(store, "REQUEST_WORKERS_SUCCESS"),
  ]);
}

add_task(async function testAboutDebugging() {
  let win = await loadInitialView("extension");

  let aboutAddonsTab = gBrowser.selectedTab;
  let debugAddonsBtn = win.document.querySelector(
    '#page-options [action="debug-addons"]'
  );

  // Verify the about:debugging is loaded.
  info(`Check about:debugging loads`);
  let loaded = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "about:debugging#/runtime/this-firefox",
    true
  );
  debugAddonsBtn.click();
  await loaded;
  let aboutDebuggingTab = gBrowser.selectedTab;
  const { AboutDebugging } = aboutDebuggingTab.linkedBrowser.contentWindow;
  // Avoid test failures due to closing the about:debugging tab
  // while it is still initializing.
  info("Wait until about:debugging actions are finished");
  await waitForRequestsSuccess(AboutDebugging.store);

  info("Switch back to about:addons");
  await BrowserTestUtils.switchTab(gBrowser, aboutAddonsTab);
  is(gBrowser.selectedTab, aboutAddonsTab, "Back to about:addons");

  info("Re-open about:debugging");
  let switched = TestUtils.waitForCondition(
    () => gBrowser.selectedTab == aboutDebuggingTab
  );
  debugAddonsBtn.click();
  await switched;
  await waitForRequestsToSettle(AboutDebugging.store);

  info("Force about:debugging to a different hash URL");
  aboutDebuggingTab.linkedBrowser.contentWindow.location.hash = "/setup";

  info("Switch back to about:addons again");
  await BrowserTestUtils.switchTab(gBrowser, aboutAddonsTab);
  is(gBrowser.selectedTab, aboutAddonsTab, "Back to about:addons");

  info("Re-open about:debugging a second time");
  switched = TestUtils.waitForCondition(
    () => gBrowser.selectedTab == aboutDebuggingTab
  );
  debugAddonsBtn.click();
  await switched;

  info("Wait until any new about:debugging request did settle");
  // Avoid test failures due to closing the about:debugging tab
  // while it is still initializing.
  await waitForRequestsToSettle(AboutDebugging.store);

  info("Remove the about:debugging tab");
  BrowserTestUtils.removeTab(aboutDebuggingTab);

  await closeView(win);
});
