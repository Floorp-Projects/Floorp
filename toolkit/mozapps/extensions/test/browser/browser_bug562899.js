/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Simulates quickly switching between different list views to verify that only
// the last selected is displayed

const {PromiseTestUtils} = ChromeUtils.import("resource://testing-common/PromiseTestUtils.jsm");

PromiseTestUtils.whitelistRejectionsGlobally(/this\._errorLink/);

var gManagerWindow;
var gCategoryUtilities;

// This test is testing XUL about:addons UI (the HTML about:addons has its
// own test files and these test cases should be added in Bug 1552170).
SpecialPowers.pushPrefEnv({
  set: [["extensions.htmlaboutaddons.enabled", false]],
});

async function test() {
  waitForExplicitFinish();

  let aWindow = await open_manager(null);
  gManagerWindow = aWindow;
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);
  run_next_test();
}

async function end_test() {
  await close_manager(gManagerWindow);
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
