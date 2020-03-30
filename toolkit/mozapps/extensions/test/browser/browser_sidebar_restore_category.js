/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that the selected category is persisted across loads of the manager

add_task(async function testCategoryRestore() {
  let win = await loadInitialView("extension");
  let utils = new CategoryUtilities(win.managerWindow);

  // Open the plugins category
  await utils.openType("plugin");

  // Re-open the manager
  await closeView(win);
  win = await loadInitialView();
  utils = new CategoryUtilities(win.managerWindow);

  is(
    utils.selectedCategory,
    "plugin",
    "Should have shown the plugins category"
  );

  // Open the extensions category
  await utils.openType("extension");

  // Re-open the manager
  await closeView(win);
  win = await loadInitialView();
  utils = new CategoryUtilities(win.managerWindow);

  is(
    utils.selectedCategory,
    "extension",
    "Should have shown the extensions category"
  );

  await closeView(win);
});

add_task(async function testInvalidAddonType() {
  let win = await loadInitialView("invalid");

  let categoryUtils = new CategoryUtilities(win.managerWindow);
  is(
    categoryUtils.getSelectedViewId(),
    win.managerWindow.gViewDefault,
    "default view is selected"
  );
  is(
    win.managerWindow.gViewController.currentViewId,
    win.managerWindow.gViewDefault,
    "default view is shown"
  );

  await closeView(win);
});

add_task(async function testInvalidViewId() {
  let win = await loadInitialView("addons://invalid/view");

  let categoryUtils = new CategoryUtilities(win.managerWindow);
  is(
    categoryUtils.getSelectedViewId(),
    win.managerWindow.gViewDefault,
    "default view is selected"
  );
  is(
    win.managerWindow.gViewController.currentViewId,
    win.managerWindow.gViewDefault,
    "default view is shown"
  );

  await closeView(win);
});
