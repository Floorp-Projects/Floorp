/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Simulates quickly switching between different list views to verify that only
// the last selected is displayed

var tempScope = {};
ChromeUtils.import("resource://gre/modules/LightweightThemeManager.jsm", tempScope);
var LightweightThemeManager = tempScope.LightweightThemeManager;

var gManagerWindow;
var gCategoryUtilities;

async function test() {
  waitForExplicitFinish();

  // Add a lightweight theme so at least one theme exists
  LightweightThemeManager.currentTheme = {
    id: "test",
    name: "Test lightweight theme",
    headerURL: "http://example.com/header.png"
  };

  let aWindow = await open_manager(null);
  gManagerWindow = aWindow;
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);
  run_next_test();
}

async function end_test() {
  await close_manager(gManagerWindow);
  LightweightThemeManager.forgetUsedTheme("test");
  finish();
}

// Tests that loading a second view before the first has not finished loading
// does not merge the results
add_test(async function() {
  var themeCount = null;
  var pluginCount = null;
  var themeItem = gCategoryUtilities.get("theme");
  var pluginItem = gCategoryUtilities.get("plugin");
  var list = gManagerWindow.document.getElementById("addon-list");

  await gCategoryUtilities.open(themeItem);
  themeCount = list.childNodes.length;
  ok(themeCount > 0, "Test is useless if there are no themes");

  await gCategoryUtilities.open(pluginItem);
  pluginCount = list.childNodes.length;
  ok(pluginCount > 0, "Test is useless if there are no plugins");

  gCategoryUtilities.open(themeItem);

  await gCategoryUtilities.open(pluginItem);
  is(list.childNodes.length, pluginCount, "Should only see the plugins");

  var item = list.firstChild;
  while (item) {
    is(item.getAttribute("type"), "plugin", "All items should be plugins");
    item = item.nextSibling;
  }

  // Tests that switching to, from, to the same pane in quick succession
  // still only shows the right number of results

  gCategoryUtilities.open(themeItem);
  gCategoryUtilities.open(pluginItem);
  await gCategoryUtilities.open(themeItem);
  is(list.childNodes.length, themeCount, "Should only see the theme");

  item = list.firstChild;
  while (item) {
    is(item.getAttribute("type"), "theme", "All items should be theme");
    item = item.nextSibling;
  }

  run_next_test();
});
