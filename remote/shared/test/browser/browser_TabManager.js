/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TabManager } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/TabManager.sys.mjs"
);

const FRAME_URL = "https://example.com/document-builder.sjs?html=frame";
const FRAME_MARKUP = `<iframe src="${encodeURI(FRAME_URL)}"></iframe>`;
const TEST_URL = `https://example.com/document-builder.sjs?html=${encodeURI(
  FRAME_MARKUP
)}`;

add_task(async function test_getBrowsingContextById() {
  const browser = gBrowser.selectedBrowser;

  is(TabManager.getBrowsingContextById(null), null);
  is(TabManager.getBrowsingContextById(undefined), null);
  is(TabManager.getBrowsingContextById("wrong-id"), null);

  info(`Navigate to ${TEST_URL}`);
  await loadURL(browser, TEST_URL);

  const contexts = browser.browsingContext.getAllBrowsingContextsInSubtree();
  is(contexts.length, 2, "Top context has 1 child");

  const topContextId = TabManager.getIdForBrowsingContext(contexts[0]);
  is(TabManager.getBrowsingContextById(topContextId), contexts[0]);
  const childContextId = TabManager.getIdForBrowsingContext(contexts[1]);
  is(TabManager.getBrowsingContextById(childContextId), contexts[1]);
});

add_task(async function test_addTab_focus() {
  let tabsCount = gBrowser.tabs.length;

  let newTab1, newTab2, newTab3;
  try {
    newTab1 = await TabManager.addTab({ focus: true });

    ok(gBrowser.tabs.includes(newTab1), "A new tab was created");
    is(gBrowser.tabs.length, tabsCount + 1);
    is(gBrowser.selectedTab, newTab1, "Tab added with focus: true is selected");

    newTab2 = await TabManager.addTab({ focus: false });

    ok(gBrowser.tabs.includes(newTab2), "A new tab was created");
    is(gBrowser.tabs.length, tabsCount + 2);
    is(
      gBrowser.selectedTab,
      newTab1,
      "Tab added with focus: false is not selected"
    );

    newTab3 = await TabManager.addTab();

    ok(gBrowser.tabs.includes(newTab3), "A new tab was created");
    is(gBrowser.tabs.length, tabsCount + 3);
    is(
      gBrowser.selectedTab,
      newTab1,
      "Tab added with no focus parameter is not selected (defaults to false)"
    );
  } finally {
    gBrowser.removeTab(newTab1);
    gBrowser.removeTab(newTab2);
    gBrowser.removeTab(newTab3);
  }
});

add_task(async function test_addTab_referenceTab() {
  let tab1, tab2, tab3, tab4;
  try {
    tab1 = await TabManager.addTab();
    // Add a second tab with no referenceTab, should be added at the end.
    tab2 = await TabManager.addTab();
    // Add a third tab with tab1 as referenceTab, should be added right after tab1.
    tab3 = await TabManager.addTab({ referenceTab: tab1 });
    // Add a fourth tab with tab2 as referenceTab, should be added right after tab2.
    tab4 = await TabManager.addTab({ referenceTab: tab2 });

    // Check that the tab order is as expected: tab1 > tab3 > tab2 > tab4
    const tab1Index = gBrowser.tabs.indexOf(tab1);
    is(gBrowser.tabs[tab1Index + 1], tab3);
    is(gBrowser.tabs[tab1Index + 2], tab2);
    is(gBrowser.tabs[tab1Index + 3], tab4);
  } finally {
    gBrowser.removeTab(tab1);
    gBrowser.removeTab(tab2);
    gBrowser.removeTab(tab3);
    gBrowser.removeTab(tab4);
  }
});

add_task(async function test_addTab_window() {
  const win1 = await BrowserTestUtils.openNewBrowserWindow();
  const win2 = await BrowserTestUtils.openNewBrowserWindow();
  try {
    // openNewBrowserWindow should ensure the new window is focused.
    is(Services.wm.getMostRecentBrowserWindow(null), win2);

    const newTab1 = await TabManager.addTab({ window: win1 });
    is(
      newTab1.ownerGlobal,
      win1,
      "The new tab was opened in the specified window"
    );

    const newTab2 = await TabManager.addTab({ window: win2 });
    is(
      newTab2.ownerGlobal,
      win2,
      "The new tab was opened in the specified window"
    );

    const newTab3 = await TabManager.addTab();
    is(
      newTab3.ownerGlobal,
      win2,
      "The new tab was opened in the foreground window"
    );
  } finally {
    await BrowserTestUtils.closeWindow(win1);
    await BrowserTestUtils.closeWindow(win2);
  }
});

add_task(async function test_getNavigableForBrowsingContext() {
  const browser = gBrowser.selectedBrowser;

  info(`Navigate to ${TEST_URL}`);
  await loadURL(browser, TEST_URL);

  const contexts = browser.browsingContext.getAllBrowsingContextsInSubtree();
  is(contexts.length, 2, "Top context has 1 child");

  // For a top-level browsing context the content browser is returned.
  const topContext = contexts[0];
  is(
    TabManager.getNavigableForBrowsingContext(topContext),
    browser,
    "Top-Level browsing context has the content browser as navigable"
  );

  // For child browsing contexts the browsing context itself is returned.
  const childContext = contexts[1];
  is(
    TabManager.getNavigableForBrowsingContext(childContext),
    childContext,
    "Child browsing context has itself as navigable"
  );

  const invalidValues = [undefined, null, 1, "test", {}, []];
  for (const invalidValue of invalidValues) {
    Assert.throws(
      () => TabManager.getNavigableForBrowsingContext(invalidValue),
      /Expected browsingContext to be a CanonicalBrowsingContext/
    );
  }
});

add_task(async function test_getTabForBrowsingContext() {
  const tab = await TabManager.addTab();
  try {
    const browser = tab.linkedBrowser;

    info(`Navigate to ${TEST_URL}`);
    await loadURL(browser, TEST_URL);

    const contexts = browser.browsingContext.getAllBrowsingContextsInSubtree();
    is(TabManager.getTabForBrowsingContext(contexts[0]), tab);
    is(TabManager.getTabForBrowsingContext(contexts[1]), tab);
    is(TabManager.getTabForBrowsingContext(null), null);
  } finally {
    gBrowser.removeTab(tab);
  }
});
