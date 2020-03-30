/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const THEME_ID = "default-theme@mozilla.org";

function assertViewHas(win, selector, msg) {
  ok(win.document.querySelector(selector), msg);
}
function assertListView(win, type) {
  assertViewHas(win, `addon-list[type="${type}"]`, `On ${type} list`);
}

add_task(async function testClickingSidebarEntriesChangesView() {
  let win = await loadInitialView("extension");
  let doc = win.document;
  let themeCategory = doc.querySelector("#categories > [name=theme]");
  let extensionCategory = doc.querySelector("#categories > [name=extension]");

  assertListView(win, "extension");

  let loaded = waitForViewLoad(win);
  themeCategory.click();
  await loaded;

  assertListView(win, "theme");

  loaded = waitForViewLoad(win);
  getAddonCard(win, THEME_ID).click();
  await loaded;

  ok(!doc.querySelector("addon-list"), "No more addon-list");
  assertViewHas(
    win,
    `addon-card[addon-id="${THEME_ID}"][expanded]`,
    "Detail view now"
  );

  loaded = waitForViewLoad(win);
  EventUtils.synthesizeMouseAtCenter(themeCategory, {}, win);
  await loaded;

  assertListView(win, "theme");

  loaded = waitForViewLoad(win);
  EventUtils.synthesizeMouseAtCenter(extensionCategory, {}, win);
  await loaded;

  assertListView(win, "extension");

  await closeView(win);
});

add_task(async function testClickingSidebarPaddingNoChange() {
  let win = await loadInitialView("theme");
  let { managerWindow } = win;
  let categoryUtils = new CategoryUtilities(managerWindow);
  let themeCategory = categoryUtils.get("theme");

  let loadDetailView = async () => {
    let loaded = waitForViewLoad(win);
    getAddonCard(win, THEME_ID).click();
    await loaded;

    is(
      managerWindow.gViewController.currentViewId,
      `addons://detail/${THEME_ID}`,
      "The detail view loaded"
    );
  };

  // Confirm that clicking the button directly works.
  await loadDetailView();
  let loaded = waitForViewLoad(win);
  EventUtils.synthesizeMouseAtCenter(themeCategory, {}, win);
  await loaded;
  is(
    managerWindow.gViewController.currentViewId,
    `addons://list/theme`,
    "The detail view loaded"
  );

  // Confirm that clicking on the padding beside it does nothing.
  await loadDetailView();
  EventUtils.synthesizeMouse(themeCategory, -5, -5, {}, win);
  ok(!managerWindow.gViewController.isLoading, "No view is loading");

  await closeView(win);
});

add_task(async function testKeyboardUsage() {
  let win = await loadInitialView("extension");
  let categories = win.document.getElementById("categories");
  let extensionCategory = categories.getButtonByName("extension");
  let themeCategory = categories.getButtonByName("theme");
  let pluginCategory = categories.getButtonByName("plugin");

  let waitForAnimationFrame = () =>
    new Promise(resolve => win.requestAnimationFrame(resolve));
  let sendKey = (key, e = {}) => {
    EventUtils.synthesizeKey(key, e, win);
    return waitForAnimationFrame();
  };
  let sendTabKey = e => sendKey("VK_TAB", e);
  let isFocusInCategories = () =>
    categories.contains(win.document.activeElement);

  ok(!isFocusInCategories(), "Focus is not in the category list");

  // Tab into the HTML browser.
  await sendTabKey();
  // Tab to the first focusable element.
  await sendTabKey();

  ok(isFocusInCategories(), "Focus is in the category list");
  is(
    win.document.activeElement,
    extensionCategory,
    "The extension button is focused"
  );

  // Tab out of the categories list.
  await sendTabKey();
  ok(!isFocusInCategories(), "Focus is out of the category list");

  // Tab back into the list.
  await sendTabKey({ shiftKey: true });
  is(win.document.activeElement, extensionCategory, "Back on Extensions");

  // We're on the extension list.
  assertListView(win, "extension");

  // Switch to theme list.
  let loaded = waitForViewLoad(win);
  await sendKey("VK_DOWN");
  is(win.document.activeElement, themeCategory, "Themes is focused");
  await loaded;

  assertListView(win, "theme");

  loaded = waitForViewLoad(win);
  await sendKey("VK_DOWN");
  is(win.document.activeElement, pluginCategory, "Plugins is focused");
  await loaded;

  assertListView(win, "plugin");

  await sendKey("VK_DOWN");
  is(win.document.activeElement, pluginCategory, "Plugins is still focused");
  ok(!win.managerWindow.gViewController.isLoading, "No view is loading");

  loaded = waitForViewLoad(win);
  await sendKey("VK_UP");
  await loaded;
  loaded = waitForViewLoad(win);
  await sendKey("VK_UP");
  await loaded;
  is(win.document.activeElement, extensionCategory, "Extensions is focused");
  assertListView(win, "extension");

  await closeView(win);
});
