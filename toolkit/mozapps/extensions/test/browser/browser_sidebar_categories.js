/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const THEME_ID = "default-theme@mozilla.org";

add_task(async function testClickingSidebarEntriesChangesView() {
  let win = await loadInitialView("extension");
  let doc = win.document;
  let { managerWindow } = win;
  let themeCategory = managerWindow.document.getElementById("category-theme");

  let assertViewHas = (selector, msg) => ok(doc.querySelector(selector), msg);
  let assertListView = type =>
    assertViewHas(`addon-list[type="${type}"]`, `On ${type} list`);

  assertListView("extension");

  let loaded = waitForViewLoad(win);
  themeCategory.click();
  await loaded;

  assertListView("theme");

  loaded = waitForViewLoad(win);
  getAddonCard(win, THEME_ID).click();
  await loaded;

  ok(!doc.querySelector("addon-list"), "No more addon-list");
  assertViewHas(
    `addon-card[addon-id="${THEME_ID}"][expanded]`,
    "Detail view now"
  );

  loaded = waitForViewLoad(win);
  EventUtils.synthesizeMouseAtCenter(
    themeCategory.firstElementChild,
    {},
    managerWindow
  );
  await loaded;

  assertListView("theme");

  loaded = waitForViewLoad(win);
  EventUtils.synthesizeKey("VK_UP", {}, managerWindow);
  await loaded;

  assertListView("extension");

  await closeView(win);
});
