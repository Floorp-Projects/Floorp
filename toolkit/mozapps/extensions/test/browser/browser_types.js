/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that registering new types works

var gManagerWindow;
var gCategoryUtilities;

var gProvider = {
};

var gTypes = [
  new AddonManagerPrivate.AddonType("type1", null, "Type 1",
                                    AddonManager.VIEW_TYPE_LIST, 4500),
  new AddonManagerPrivate.AddonType("missing1", null, "Missing 1"),
  new AddonManagerPrivate.AddonType("type2", null, "Type 1",
                                    AddonManager.VIEW_TYPE_LIST, 5100,
                                    AddonManager.TYPE_UI_HIDE_EMPTY),
  {
    id: "type3",
    name: "Type 3",
    uiPriority: 5200,
    viewType: AddonManager.VIEW_TYPE_LIST,
  },
];

// This test is testing XUL about:addons UI features that are not supported in the
// HTML about:addons.
SpecialPowers.pushPrefEnv({
  set: [["extensions.htmlaboutaddons.enabled", false]],
});

function go_back(aManager) {
  gBrowser.goBack();
}

function go_forward(aManager) {
  gBrowser.goForward();
}

function check_state(canGoBack, canGoForward) {
  is(gBrowser.canGoBack, canGoBack, "canGoBack should be correct");
  is(gBrowser.canGoForward, canGoForward, "canGoForward should be correct");
}

function test() {
  waitForExplicitFinish();

  run_next_test();
}

function end_test() {
  finish();
}

// Add a new type, open the manager and make sure it is in the right place
add_test(async function() {
  AddonManagerPrivate.registerProvider(gProvider, gTypes);

  let aWindow = await open_manager(null);
  gManagerWindow = aWindow;
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  ok(gCategoryUtilities.get("type1"), "Type 1 should be present");
  ok(gCategoryUtilities.get("type2"), "Type 2 should be present");
  ok(!gCategoryUtilities.get("missing1", true), "Missing 1 should be absent");

  is(gCategoryUtilities.get("type1").previousSibling.getAttribute("value"),
     "addons://list/extension", "Type 1 should be in the right place");
  is(gCategoryUtilities.get("type2").previousSibling.getAttribute("value"),
     "addons://list/theme", "Type 2 should be in the right place");

  ok(gCategoryUtilities.isTypeVisible("type1"), "Type 1 should be visible");
  ok(!gCategoryUtilities.isTypeVisible("type2"), "Type 2 should be hidden");

  run_next_test();
});

// Select the type, close the manager and remove it then open the manager and
// check we're back to the default view
add_test(async function() {
  await gCategoryUtilities.openType("type1");
  await close_manager(gManagerWindow);
  AddonManagerPrivate.unregisterProvider(gProvider);

  let aWindow = await open_manager(null);
  gManagerWindow = aWindow;
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  ok(!gCategoryUtilities.get("type1", true), "Type 1 should be absent");
  ok(!gCategoryUtilities.get("type2", true), "Type 2 should be absent");
  ok(!gCategoryUtilities.get("missing1", true), "Missing 1 should be absent");

  is(gCategoryUtilities.selectedCategory, "discover", "Should be back to the default view");

  close_manager(gManagerWindow, run_next_test);
});

// Add a type while the manager is still open and check it appears
add_test(async function() {
  let aWindow = await open_manager("addons://list/extension");
  gManagerWindow = aWindow;
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  ok(!gCategoryUtilities.get("type1", true), "Type 1 should be absent");
  ok(!gCategoryUtilities.get("type2", true), "Type 2 should be absent");
  ok(!gCategoryUtilities.get("missing1", true), "Missing 1 should be absent");

  AddonManagerPrivate.registerProvider(gProvider, gTypes);

  ok(gCategoryUtilities.get("type1"), "Type 1 should be present");
  ok(gCategoryUtilities.get("type2"), "Type 2 should be present");
  ok(!gCategoryUtilities.get("missing1", true), "Missing 1 should be absent");

  is(gCategoryUtilities.get("type1").previousSibling.getAttribute("value"),
     "addons://list/extension", "Type 1 should be in the right place");
  is(gCategoryUtilities.get("type2").previousSibling.getAttribute("value"),
     "addons://list/theme", "Type 2 should be in the right place");

  ok(gCategoryUtilities.isTypeVisible("type1"), "Type 1 should be visible");
  ok(!gCategoryUtilities.isTypeVisible("type2"), "Type 2 should be hidden");

  run_next_test();
});

// Remove the type while it is beng viewed and check it is replaced with the
// default view
add_test(async function() {
  await gCategoryUtilities.openType("type1");
  await gCategoryUtilities.openType("plugin");
  go_back(gManagerWindow);
  await wait_for_view_load(gManagerWindow);
  is(gCategoryUtilities.selectedCategory, "type1", "Should be showing the custom view");
  check_state(true, true);

  AddonManagerPrivate.unregisterProvider(gProvider);

  ok(!gCategoryUtilities.get("type1", true), "Type 1 should be absent");
  ok(!gCategoryUtilities.get("type2", true), "Type 2 should be absent");
  ok(!gCategoryUtilities.get("missing1", true), "Missing 1 should be absent");

  is(gCategoryUtilities.selectedCategory, "discover", "Should be back to the default view");
  check_state(true, true);

  go_back(gManagerWindow);
  await wait_for_view_load(gManagerWindow);
  is(gCategoryUtilities.selectedCategory, "extension", "Should be showing the extension view");
  check_state(false, true);

  go_forward(gManagerWindow);
  await wait_for_view_load(gManagerWindow);
  is(gCategoryUtilities.selectedCategory, "discover", "Should be back to the default view");
  check_state(true, true);

  go_forward(gManagerWindow);
  await wait_for_view_load(gManagerWindow);
  is(gCategoryUtilities.selectedCategory, "plugin", "Should be back to the plugins view");
  check_state(true, false);

  go_back(gManagerWindow);
  await wait_for_view_load(gManagerWindow);
  is(gCategoryUtilities.selectedCategory, "discover", "Should be back to the default view");
  check_state(true, true);

  close_manager(gManagerWindow, run_next_test);
});

// Test that when going back to a now missing category we skip it
add_test(async function() {
  let aWindow = await open_manager("addons://list/extension");
  gManagerWindow = aWindow;
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  AddonManagerPrivate.registerProvider(gProvider, gTypes);

  ok(gCategoryUtilities.get("type1"), "Type 1 should be present");
  ok(gCategoryUtilities.isTypeVisible("type1"), "Type 1 should be visible");

  await gCategoryUtilities.openType("type1");
  await gCategoryUtilities.openType("plugin");
  AddonManagerPrivate.unregisterProvider(gProvider);

  ok(!gCategoryUtilities.get("type1", true), "Type 1 should not be present");

  go_back(gManagerWindow);

  await wait_for_view_load(gManagerWindow);
  is(gCategoryUtilities.selectedCategory, "extension", "Should be back to the first view");
  check_state(false, true);

  close_manager(gManagerWindow, run_next_test);
});

// Test that when going forward to a now missing category we skip it
add_test(async function() {
  let aWindow = await open_manager("addons://list/extension");
  gManagerWindow = aWindow;
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  AddonManagerPrivate.registerProvider(gProvider, gTypes);

  ok(gCategoryUtilities.get("type1"), "Type 1 should be present");
  ok(gCategoryUtilities.isTypeVisible("type1"), "Type 1 should be visible");

  await gCategoryUtilities.openType("type1");
  await gCategoryUtilities.openType("plugin");
  go_back(gManagerWindow);
  await wait_for_view_load(gManagerWindow);
  go_back(gManagerWindow);
  await wait_for_view_load(gManagerWindow);
  is(gCategoryUtilities.selectedCategory, "extension", "Should be back to the extension view");

  AddonManagerPrivate.unregisterProvider(gProvider);

  ok(!gCategoryUtilities.get("type1", true), "Type 1 should not be present");

  go_forward(gManagerWindow);

  await wait_for_view_load(gManagerWindow);
  is(gCategoryUtilities.selectedCategory, "plugin", "Should be back to the plugin view");
  check_state(true, false);

  close_manager(gManagerWindow, run_next_test);
});

// Test that when going back to a now missing category and we can't go back any
// any further then we just display the default view
add_test(async function() {
  AddonManagerPrivate.registerProvider(gProvider, gTypes);

  let aWindow = await open_manager("addons://list/type1");
  gManagerWindow = aWindow;
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);
  is(gCategoryUtilities.selectedCategory, "type1", "Should be at the custom view");

  ok(gCategoryUtilities.get("type1"), "Type 1 should be present");
  ok(gCategoryUtilities.isTypeVisible("type1"), "Type 1 should be visible");

  await gCategoryUtilities.openType("extension");
  AddonManagerPrivate.unregisterProvider(gProvider);

  ok(!gCategoryUtilities.get("type1", true), "Type 1 should not be present");

  go_back(gManagerWindow);

  await wait_for_view_load(gManagerWindow);
  is(gCategoryUtilities.selectedCategory, "discover", "Should be at the default view");
  check_state(false, true);

  close_manager(gManagerWindow, run_next_test);
});

// Test that when going forward to a now missing category and we can't go
// forward any further then we just display the default view
add_test(async function() {
  AddonManagerPrivate.registerProvider(gProvider, gTypes);

  let aWindow = await open_manager("addons://list/extension");
  gManagerWindow = aWindow;
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  ok(gCategoryUtilities.get("type1"), "Type 1 should be present");
  ok(gCategoryUtilities.isTypeVisible("type1"), "Type 1 should be visible");

  await gCategoryUtilities.openType("type1");
  go_back(gManagerWindow);

  await wait_for_view_load(gManagerWindow);
  is(gCategoryUtilities.selectedCategory, "extension", "Should be at the extension view");

  AddonManagerPrivate.unregisterProvider(gProvider);

  ok(!gCategoryUtilities.get("type1", true), "Type 1 should not be present");

  go_forward(gManagerWindow);

  await wait_for_view_load(gManagerWindow);
  is(gCategoryUtilities.selectedCategory, "discover", "Should be at the default view");
  check_state(true, false);

  close_manager(gManagerWindow, run_next_test);
});

// Test that when going back we skip multiple missing categories
add_test(async function() {
  let aWindow = await open_manager("addons://list/extension");
  gManagerWindow = aWindow;
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  AddonManagerPrivate.registerProvider(gProvider, gTypes);

  ok(gCategoryUtilities.get("type1"), "Type 1 should be present");
  ok(gCategoryUtilities.isTypeVisible("type1"), "Type 1 should be visible");

  await gCategoryUtilities.openType("type1");
  await gCategoryUtilities.openType("type3");
  await gCategoryUtilities.openType("plugin");
  AddonManagerPrivate.unregisterProvider(gProvider);

  ok(!gCategoryUtilities.get("type1", true), "Type 1 should not be present");

  go_back(gManagerWindow);

  await wait_for_view_load(gManagerWindow);
  is(gCategoryUtilities.selectedCategory, "extension", "Should be back to the first view");
  check_state(false, true);

  close_manager(gManagerWindow, run_next_test);
});

// Test that when going forward we skip multiple missing categories
add_test(async function() {
  let aWindow = await open_manager("addons://list/extension");
  gManagerWindow = aWindow;
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  AddonManagerPrivate.registerProvider(gProvider, gTypes);

  ok(gCategoryUtilities.get("type1"), "Type 1 should be present");
  ok(gCategoryUtilities.isTypeVisible("type1"), "Type 1 should be visible");

  await gCategoryUtilities.openType("type1");
  await gCategoryUtilities.openType("type3");
  await gCategoryUtilities.openType("plugin");
  go_back(gManagerWindow);
  await wait_for_view_load(gManagerWindow);
  go_back(gManagerWindow);
  await wait_for_view_load(gManagerWindow);
  go_back(gManagerWindow);
  await wait_for_view_load(gManagerWindow);
  is(gCategoryUtilities.selectedCategory, "extension", "Should be back to the extension view");

  AddonManagerPrivate.unregisterProvider(gProvider);

  ok(!gCategoryUtilities.get("type1", true), "Type 1 should not be present");

  go_forward(gManagerWindow);

  await wait_for_view_load(gManagerWindow);
  is(gCategoryUtilities.selectedCategory, "plugin", "Should be back to the plugin view");
  check_state(true, false);

  close_manager(gManagerWindow, run_next_test);
});

// Test that when going back we skip all missing categories and when we can't go
// back any any further then we just display the default view
add_test(async function() {
  AddonManagerPrivate.registerProvider(gProvider, gTypes);

  let aWindow = await open_manager("addons://list/type1");
  gManagerWindow = aWindow;
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);
  is(gCategoryUtilities.selectedCategory, "type1", "Should be at the custom view");

  ok(gCategoryUtilities.get("type1"), "Type 1 should be present");
  ok(gCategoryUtilities.isTypeVisible("type1"), "Type 1 should be visible");

  await gCategoryUtilities.openType("type3");
  await gCategoryUtilities.openType("extension");
  AddonManagerPrivate.unregisterProvider(gProvider);

  ok(!gCategoryUtilities.get("type1", true), "Type 1 should not be present");

  go_back(gManagerWindow);

  await wait_for_view_load(gManagerWindow);
  is(gCategoryUtilities.selectedCategory, "discover", "Should be at the default view");
  check_state(false, true);

  close_manager(gManagerWindow, run_next_test);
});

// Test that when going forward we skip all missing categories and when we can't
// go back any any further then we just display the default view
add_test(async function() {
  AddonManagerPrivate.registerProvider(gProvider, gTypes);

  let aWindow = await open_manager("addons://list/extension");
  gManagerWindow = aWindow;
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  ok(gCategoryUtilities.get("type1"), "Type 1 should be present");
  ok(gCategoryUtilities.isTypeVisible("type1"), "Type 1 should be visible");

  await gCategoryUtilities.openType("type1");
  await gCategoryUtilities.openType("type3");
  go_back(gManagerWindow);

  await wait_for_view_load(gManagerWindow);
  go_back(gManagerWindow);

  await wait_for_view_load(gManagerWindow);
  is(gCategoryUtilities.selectedCategory, "extension", "Should be at the extension view");

  AddonManagerPrivate.unregisterProvider(gProvider);

  ok(!gCategoryUtilities.get("type1", true), "Type 1 should not be present");

  go_forward(gManagerWindow);

  await wait_for_view_load(gManagerWindow);
  is(gCategoryUtilities.selectedCategory, "discover", "Should be at the default view");
  check_state(true, false);

  close_manager(gManagerWindow, run_next_test);
});
