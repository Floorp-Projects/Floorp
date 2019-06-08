/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var gManagerWindow;
var gDocument;
var gCategoryUtilities;
var gProvider;

// This test is testing XUL about:addons UI (the HTML about:addons has its
// own test files for these test cases).
SpecialPowers.pushPrefEnv({
  set: [["extensions.htmlaboutaddons.enabled", false]],
});

async function setup_manager(...args) {
  let aWindow = await open_manager(...args);
  gManagerWindow = aWindow;
  gDocument = gManagerWindow.document;
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);
}

async function test() {
  requestLongerTimeout(2);
  waitForExplicitFinish();

  gProvider = new MockProvider();

  gProvider.createAddons([{
    id: "addon2@tests.mozilla.org",
    name: "Uninstall doesn't need restart 1",
    type: "extension",
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE,
  }, {
    id: "addon3@tests.mozilla.org",
    name: "Uninstall doesn't need restart 2",
    type: "extension",
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE,
  }, {
    id: "addon4@tests.mozilla.org",
    name: "Uninstall doesn't need restart 3",
    type: "extension",
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE,
  }, {
    id: "addon5@tests.mozilla.org",
    name: "Uninstall doesn't need restart 4",
    type: "extension",
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE,
  }, {
    id: "addon6@tests.mozilla.org",
    name: "Uninstall doesn't need restart 5",
    type: "extension",
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE,
  }, {
    id: "addon7@tests.mozilla.org",
    name: "Uninstall doesn't need restart 6",
    type: "extension",
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE,
  }, {
    id: "addon8@tests.mozilla.org",
    name: "Uninstall doesn't need restart 7",
    type: "extension",
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE,
  }, {
    id: "addon9@tests.mozilla.org",
    name: "Uninstall doesn't need restart 8",
    type: "extension",
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE,
  }]);

  await setup_manager(null);
  run_next_test();
}

async function end_test() {
  await close_manager(gManagerWindow);
  finish();
}

function get_item_in_list(aId, aList) {
  var item = aList.firstChild;
  while (item) {
    if ("mAddon" in item && item.mAddon.id == aId) {
      aList.ensureElementIsVisible(item);
      return item;
    }
    item = item.nextSibling;
  }
  return null;
}

// Tests that uninstalling a restartless add-on from the list view can be undone
add_test(async function() {
  var ID = "addon2@tests.mozilla.org";
  var list = gDocument.getElementById("addon-list");

  // Select the extensions category
  await gCategoryUtilities.openType("extension");
  is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

  let aAddon = await AddonManager.getAddonByID(ID);
  ok(aAddon.isActive, "Add-on should be active");
  ok(!(aAddon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_UNINSTALL), "Add-on should not require a restart to uninstall");
  ok(!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should not be pending uninstall");

  var item = get_item_in_list(ID, list);
  isnot(item, null, "Should have found the add-on in the list");

  var button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
  isnot(button, null, "Should have a remove button");
  ok(!button.disabled, "Button should not be disabled");

  EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

  // Force XBL to apply
  item.clientTop;

  is(item.getAttribute("pending"), "uninstall", "Add-on should be uninstalling");

  ok(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL, "Add-on should be pending uninstall");
  ok(!aAddon.isActive, "Add-on should be inactive");

  button = gDocument.getAnonymousElementByAttribute(item, "anonid", "undo-btn");
  isnot(button, null, "Should have an undo button");

  EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

  // Force XBL to apply
  item.clientTop;

  ok(aAddon.isActive, "Add-on should be active");
  button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
  isnot(button, null, "Should have a remove button");
  ok(!button.disabled, "Button should not be disabled");

  run_next_test();
});

// Tests that uninstalling a disabled restartless add-on from the list view can
// be undone and doesn't re-enable
add_test(async function() {
  var ID = "addon2@tests.mozilla.org";
  var list = gDocument.getElementById("addon-list");

  // Select the extensions category
  await gCategoryUtilities.openType("extension");
  is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

  let aAddon = await AddonManager.getAddonByID(ID);
  await aAddon.disable();

  ok(!aAddon.isActive, "Add-on should be inactive");
  ok(!(aAddon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_UNINSTALL), "Add-on should not require a restart to uninstall");
  ok(!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should not be pending uninstall");

  var item = get_item_in_list(ID, list);
  isnot(item, null, "Should have found the add-on in the list");

  var button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
  isnot(button, null, "Should have a remove button");
  ok(!button.disabled, "Button should not be disabled");

  EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

  // Force XBL to apply
  item.clientTop;

  is(item.getAttribute("pending"), "uninstall", "Add-on should be uninstalling");

  ok(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL, "Add-on should be pending uninstall");
  ok(!aAddon.isActive, "Add-on should be inactive");

  button = gDocument.getAnonymousElementByAttribute(item, "anonid", "undo-btn");
  isnot(button, null, "Should have an undo button");

  EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

  // Force XBL to apply
  item.clientTop;

  ok(!aAddon.isActive, "Add-on should be inactive");
  button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
  isnot(button, null, "Should have a remove button");
  ok(!button.disabled, "Button should not be disabled");

  await aAddon.enable();
  ok(aAddon.isActive, "Add-on should be active");

  run_next_test();
});

async function test_uninstall_details(aAddon, ID) {
  is(get_current_view(gManagerWindow).id, "detail-view", "Should be in the detail view");

  var button = gDocument.getElementById("detail-uninstall-btn");
  isnot(button, null, "Should have a remove button");
  ok(!button.disabled, "Button should not be disabled");

  EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

  await wait_for_view_load(gManagerWindow);
  is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

  var list = gDocument.getElementById("addon-list");
  var item = get_item_in_list(ID, list);
  isnot(item, null, "Should have found the add-on in the list");
  is(item.getAttribute("pending"), "uninstall", "Add-on should be uninstalling");

  ok(!!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should be pending uninstall");
  ok(!aAddon.isActive, "Add-on should be inactive");

  // Force XBL to apply
  item.clientTop;

  button = gDocument.getAnonymousElementByAttribute(item, "anonid", "undo-btn");
  isnot(button, null, "Should have an undo button");

  EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

  // Force XBL to apply
  item.clientTop;

  ok(aAddon.isActive, "Add-on should be active");
  button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
  isnot(button, null, "Should have a remove button");
  ok(!button.disabled, "Button should not be disabled");

  run_next_test();
}

// Tests that uninstalling a restartless add-on from the details view switches
// back to the list view and can be undone
add_test(async function() {
  var ID = "addon2@tests.mozilla.org";
  var list = gDocument.getElementById("addon-list");

  // Select the extensions category
  await gCategoryUtilities.openType("extension");
  is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

  let aAddon = await AddonManager.getAddonByID(ID);
  ok(aAddon.isActive, "Add-on should be active");
  ok(!(aAddon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_UNINSTALL), "Add-on should not require a restart to uninstall");
  ok(!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should not be pending uninstall");

  var item = get_item_in_list(ID, list);
  isnot(item, null, "Should have found the add-on in the list");

  item.click();
  await wait_for_view_load(gManagerWindow);

  // Test the uninstall.
  return test_uninstall_details(aAddon, ID);
});

// Tests that uninstalling a restartless add-on from directly loading the
// details view switches back to the list view and can be undone
add_test(async function() {
  // Close this about:addons and open a new one in a new tab.
  await close_manager(gManagerWindow);

  // Load the detail view directly.
  var ID = "addon2@tests.mozilla.org";
  await setup_manager(`addons://detail/${ID}`);

  let aAddon = await AddonManager.getAddonByID(ID);
  ok(aAddon.isActive, "Add-on should be active");
  ok(!(aAddon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_UNINSTALL), "Add-on should not require a restart to uninstall");
  ok(!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should not be pending uninstall");

  // Test the uninstall.
  return test_uninstall_details(aAddon, ID);
});

// Tests that uninstalling a restartless add-on from the details view switches
// back to the list view and can be undone and doesn't re-enable
add_test(async function() {
  var ID = "addon2@tests.mozilla.org";
  var list = gDocument.getElementById("addon-list");

  // Select the extensions category
  await gCategoryUtilities.openType("extension");
  is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

  let aAddon = await AddonManager.getAddonByID(ID);
  await aAddon.disable();

  ok(!aAddon.isActive, "Add-on should be inactive");
  ok(!(aAddon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_UNINSTALL), "Add-on should not require a restart to uninstall");
  ok(!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should not be pending uninstall");

  var item = get_item_in_list(ID, list);
  isnot(item, null, "Should have found the add-on in the list");

  item.click();
  await wait_for_view_load(gManagerWindow);
  is(get_current_view(gManagerWindow).id, "detail-view", "Should be in the detail view");

  var button = gDocument.getElementById("detail-uninstall-btn");
  isnot(button, null, "Should have a remove button");
  ok(!button.disabled, "Button should not be disabled");

  EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

  await wait_for_view_load(gManagerWindow);
  is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

  item = get_item_in_list(ID, list);
  isnot(item, null, "Should have found the add-on in the list");
  is(item.getAttribute("pending"), "uninstall", "Add-on should be uninstalling");

  ok(!!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should be pending uninstall");
  ok(!aAddon.isActive, "Add-on should be inactive");

  // Force XBL to apply
  item.clientTop;

  button = gDocument.getAnonymousElementByAttribute(item, "anonid", "undo-btn");
  isnot(button, null, "Should have an undo button");

  EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

  // Force XBL to apply
  item.clientTop;

  ok(!aAddon.isActive, "Add-on should be inactive");
  button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
  isnot(button, null, "Should have a remove button");
  ok(!button.disabled, "Button should not be disabled");

  await aAddon.enable();
  ok(aAddon.isActive, "Add-on should be active");

  run_next_test();
});

// Tests that switching away from the list view finalises the uninstall of
// multiple restartless add-ons
add_test(async function() {
  var ID = "addon2@tests.mozilla.org";
  var ID2 = "addon6@tests.mozilla.org";
  var list = gDocument.getElementById("addon-list");

  // Select the extensions category
  await gCategoryUtilities.openType("extension");
  is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

  let aAddon = await AddonManager.getAddonByID(ID);
  ok(aAddon.isActive, "Add-on should be active");
  ok(!(aAddon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_UNINSTALL), "Add-on should not require a restart to uninstall");
  ok(!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should not be pending uninstall");

  var item = get_item_in_list(ID, list);
  isnot(item, null, "Should have found the add-on in the list");

  var button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
  isnot(button, null, "Should have a remove button");
  ok(!button.disabled, "Button should not be disabled");

  EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

  // Force XBL to apply
  item.clientTop;

  is(item.getAttribute("pending"), "uninstall", "Add-on should be uninstalling");
  ok(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL, "Add-on should be pending uninstall");
  ok(!aAddon.isActive, "Add-on should be inactive");

  button = gDocument.getAnonymousElementByAttribute(item, "anonid", "undo-btn");
  isnot(button, null, "Should have an undo button");

  item = get_item_in_list(ID2, list);
  isnot(item, null, "Should have found the add-on in the list");

  button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
  isnot(button, null, "Should have a remove button");
  ok(!button.disabled, "Button should not be disabled");

  EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

  await gCategoryUtilities.openType("plugin");
  is(gCategoryUtilities.selectedCategory, "plugin", "View should have changed to extension");

  let [bAddon, bAddon2] = await AddonManager.getAddonsByIDs([ID, ID2]);
  is(bAddon, null, "Add-on should no longer be installed");
  is(bAddon2, null, "Second add-on should no longer be installed");

  await gCategoryUtilities.openType("extension");
  is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

  item = get_item_in_list(ID, list);
  is(item, null, "Should not have found the add-on in the list");
  item = get_item_in_list(ID2, list);
  is(item, null, "Should not have found the second add-on in the list");

  run_next_test();
});

// Tests that closing the manager from the list view finalises the uninstall of
// multiple restartless add-ons
add_test(async function() {
  var ID = "addon4@tests.mozilla.org";
  var ID2 = "addon8@tests.mozilla.org";
  var list = gDocument.getElementById("addon-list");

  // Select the extensions category
  await gCategoryUtilities.openType("extension");
  is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

  let aAddon = await AddonManager.getAddonByID(ID);
  ok(aAddon.isActive, "Add-on should be active");
  ok(!(aAddon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_UNINSTALL), "Add-on should not require a restart to uninstall");
  ok(!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should not be pending uninstall");

  var item = get_item_in_list(ID, list);
  isnot(item, null, "Should have found the add-on in the list");

  var button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
  isnot(button, null, "Should have a remove button");
  ok(!button.disabled, "Button should not be disabled");

  EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

  // Force XBL to apply
  item.clientTop;

  is(item.getAttribute("pending"), "uninstall", "Add-on should be uninstalling");

  ok(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL, "Add-on should be pending uninstall");
  ok(!aAddon.isActive, "Add-on should be inactive");

  button = gDocument.getAnonymousElementByAttribute(item, "anonid", "undo-btn");
  isnot(button, null, "Should have an undo button");

  item = get_item_in_list(ID2, list);
  isnot(item, null, "Should have found the add-on in the list");

  button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
  isnot(button, null, "Should have a remove button");
  ok(!button.disabled, "Button should not be disabled");

  EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

  await close_manager(gManagerWindow);
  let [bAddon, bAddon2] = await AddonManager.getAddonsByIDs([ID, ID2]);
  is(bAddon, null, "Add-on should no longer be installed");
  is(bAddon2, null, "Second add-on should no longer be installed");

  await setup_manager(null);
  list = gDocument.getElementById("addon-list");

  is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

  item = get_item_in_list(ID, list);
  is(item, null, "Should not have found the add-on in the list");
  item = get_item_in_list(ID2, list);
  is(item, null, "Should not have found the second add-on in the list");

  run_next_test();
});
