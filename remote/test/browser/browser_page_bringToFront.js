/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const FIRST_DOC = toDataURL("first");
const SECOND_DOC = toDataURL("second");

add_task(async function testBringToFrontUpdatesSelectedTab() {
  const { client, tab } = await setupForURL(FIRST_DOC);
  is(gBrowser.selectedTab, tab, "Selected tab is the target tab");

  info("Open another tab that should become the front tab");
  const otherTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    SECOND_DOC
  );
  is(gBrowser.selectedTab, otherTab, "Selected tab is now the new tab");

  const { Page } = client;
  info(
    "Call Page.bringToFront() and check that the test tab becomes the selected tab"
  );
  await Page.bringToFront();
  is(gBrowser.selectedTab, tab, "Selected tab is the target tab again");
  is(tab.ownerGlobal, getFocusedNavigator(), "The initial window is focused");

  await client.close();
  ok(true, "The client is closed");

  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(otherTab);

  await RemoteAgent.close();
});

add_task(async function testBringToFrontUpdatesFocusedWindow() {
  const { client, tab } = await setupForURL(FIRST_DOC);
  is(gBrowser.selectedTab, tab, "Selected tab is the target tab");

  is(tab.ownerGlobal, getFocusedNavigator(), "The initial window is focused");

  const otherWindow = await BrowserTestUtils.openNewBrowserWindow();
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

  await client.close();
  ok(true, "The client is closed");

  BrowserTestUtils.removeTab(tab);
  await RemoteAgent.close();

  info("Close the additional window");
  await BrowserTestUtils.closeWindow(otherWindow);
});

function getFocusedNavigator() {
  return Services.wm.getMostRecentWindow("navigator:browser");
}
