/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const FIRST_DOC = toDataURL("first");
const SECOND_DOC = toDataURL("second");

add_task(async function testBringToFrontUpdatesSelectedTab({ client }) {
  const tab = gBrowser.selectedTab;

  await loadURL(FIRST_DOC);

  info("Open another tab that should become the front tab");
  const otherTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    SECOND_DOC
  );

  try {
    is(gBrowser.selectedTab, otherTab, "Selected tab is now the new tab");

    const { Page } = client;
    info(
      "Call Page.bringToFront() and check that the test tab becomes the selected tab"
    );
    await Page.bringToFront();
    is(gBrowser.selectedTab, tab, "Selected tab is the target tab again");
    is(tab.ownerGlobal, getFocusedNavigator(), "The initial window is focused");
  } finally {
    BrowserTestUtils.removeTab(otherTab);
  }
});

add_task(async function testBringToFrontUpdatesFocusedWindow({ client }) {
  const tab = gBrowser.selectedTab;

  await loadURL(FIRST_DOC);

  is(tab.ownerGlobal, getFocusedNavigator(), "The initial window is focused");

  const otherWindow = await BrowserTestUtils.openNewBrowserWindow();

  try {
    is(otherWindow, getFocusedNavigator(), "The new window is focused");

    const { Page } = client;
    info(
      "Call Page.bringToFront() and check that the tab window is focused again"
    );
    await Page.bringToFront();
    is(
      tab.ownerGlobal,
      getFocusedNavigator(),
      "The initial window is focused again"
    );
  } finally {
    await BrowserTestUtils.closeWindow(otherWindow);
  }
});

function getFocusedNavigator() {
  return Services.wm.getMostRecentWindow("navigator:browser");
}
