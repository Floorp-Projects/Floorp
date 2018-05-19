/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that sorting of add-ons works correctly
// (this test uses the list view, even though it no longer has sort buttons - see bug 623207)

var gManagerWindow;
var gProvider;

async function test() {
  waitForExplicitFinish();

  gProvider = new MockProvider();
  gProvider.createAddons([{
    //  enabledInstalled group
    //    * Enabled
    //    * Incompatible but enabled because compatibility checking is off
    //    * Waiting to be installed
    //    * Waiting to be enabled
    id: "test1@tests.mozilla.org",
    name: "Test add-on",
    description: "foo",
    updateDate: new Date(2010, 4, 2, 0, 0, 0),
    pendingOperations: AddonManager.PENDING_NONE,
  }, {
    id: "test2@tests.mozilla.org",
    name: "a first add-on",
    description: "foo",
    updateDate: new Date(2010, 4, 1, 23, 59, 59),
    pendingOperations: AddonManager.PENDING_UPGRADE,
    isActive: true,
    isCompatible: false,
  }, {
    id: "test3@tests.mozilla.org",
    name: "\u010Cesk\u00FD slovn\u00EDk", // Český slovník
    description: "foo",
    updateDate: new Date(2010, 4, 2, 0, 0, 1),
    pendingOperations: AddonManager.PENDING_INSTALL,
    isActive: false,
  }, {
    id: "test4@tests.mozilla.org",
    name: "canadian dictionary",
    updateDate: new Date(1970, 0, 1, 0, 0, 0),
    description: "foo",
    isActive: true,
  }, {
    id: "test5@tests.mozilla.org",
    name: "croatian dictionary",
    description: "foo",
    updateDate: new Date(2012, 12, 12, 0, 0, 0),
    pendingOperations: AddonManager.PENDING_ENABLE,
    isActive: false,
  }, {
    //  pendingDisable group
    //    * Waiting to be disabled
    id: "test6@tests.mozilla.org",
    name: "orange Add-on",
    description: "foo",
    updateDate: new Date(2010, 4, 2, 0, 0, 0),
    isCompatible: false,
    isActive: true,
    pendingOperations: AddonManager.PENDING_DISABLE,
  }, {
    id: "test7@tests.mozilla.org",
    name: "Blue Add-on",
    description: "foo",
    updateDate: new Date(2010, 4, 1, 23, 59, 59),
    isActive: true,
    pendingOperations: AddonManager.PENDING_DISABLE,
  }, {
    id: "test8@tests.mozilla.org",
    name: "Green Add-on",
    description: "foo",
    updateDate: new Date(2010, 4, 3, 0, 0, 1),
    pendingOperations: AddonManager.PENDING_DISABLE,
  }, {
    id: "test9@tests.mozilla.org",
    name: "red Add-on",
    updateDate: new Date(2011, 4, 1, 0, 0, 0),
    description: "foo",
    isCompatible: false,
    pendingOperations: AddonManager.PENDING_DISABLE,
  }, {
    id: "test10@tests.mozilla.org",
    name: "Purple Add-on",
    description: "foo",
    updateDate: new Date(2012, 12, 12, 0, 0, 0),
    isCompatible: false,
    pendingOperations: AddonManager.PENDING_DISABLE,
  }, {
    //  pendingUninstall group
    //    * Waiting to be removed
    id: "test11@tests.mozilla.org",
    name: "amber Add-on",
    description: "foo",
    updateDate: new Date(1978, 4, 2, 0, 0, 0),
    isActive: false,
    appDisabled: true,
    pendingOperations: AddonManager.PENDING_UNINSTALL,
  }, {
    id: "test12@tests.mozilla.org",
    name: "Salmon Add-on - pending disable",
    description: "foo",
    updateDate: new Date(2054, 4, 1, 23, 59, 59),
    isActive: true,
    pendingOperations: AddonManager.PENDING_UNINSTALL,
  }, {
    id: "test13@tests.mozilla.org",
    name: "rose Add-on",
    description: "foo",
    updateDate: new Date(2010, 4, 2, 0, 0, 1),
    isActive: false,
    userDisabled: true,
    pendingOperations: AddonManager.PENDING_UNINSTALL,
  }, {
    id: "test14@tests.mozilla.org",
    name: "Violet Add-on",
    updateDate: new Date(2010, 5, 1, 0, 0, 0),
    description: "foo",
    isActive: false,
    appDisabled: true,
    pendingOperations: AddonManager.PENDING_UNINSTALL,
  }, {
    id: "test15@tests.mozilla.org",
    name: "white Add-on",
    description: "foo",
    updateDate: new Date(2010, 4, 12, 0, 0, 0),
    isActive: false,
    userDisabled: true,
    pendingOperations: AddonManager.PENDING_UNINSTALL,
  }, {
    //  disabledIncompatibleBlocked group
    //    * Disabled
    //    * Incompatible
    //    * Blocklisted
    id: "test16@tests.mozilla.org",
    name: "grimsby Add-on",
    description: "foo",
    updateDate: new Date(2010, 4, 1, 0, 0, 0),
    isActive: false,
    appDisabled: true,
  }, {
    id: "test17@tests.mozilla.org",
    name: "beamsville Add-on",
    description: "foo",
    updateDate: new Date(2010, 4, 8, 23, 59, 59),
    isActive: false,
    userDisabled: true,
  }, {
    id: "test18@tests.mozilla.org",
    name: "smithville Add-on",
    description: "foo",
    updateDate: new Date(2010, 4, 3, 0, 0, 1),
    isActive: false,
    userDisabled: true,
    blocklistState: Ci.nsIBlocklistService.STATE_OUTDATED,
  }, {
    id: "test19@tests.mozilla.org",
    name: "dunnville Add-on",
    updateDate: new Date(2010, 4, 2, 0, 0, 0),
    description: "foo",
    isActive: false,
    appDisabled: true,
    isCompatible: false,
    blocklistState: Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
  }, {
    id: "test20@tests.mozilla.org",
    name: "silverdale Add-on",
    description: "foo",
    updateDate: new Date(2010, 4, 12, 0, 0, 0),
    isActive: false,
    appDisabled: true,
    blocklistState: Ci.nsIBlocklistService.STATE_BLOCKED,
  }]);


  let aWindow = await open_manager("addons://list/extension");
  gManagerWindow = aWindow;
  run_next_test();
}

async function end_test() {
  await close_manager(gManagerWindow);
  finish();
}

function set_order(aSortBy, aAscending) {
  var list = gManagerWindow.document.getElementById("addon-list");
  var elements = [];
  var node = list.firstChild;
  while (node) {
    elements.push(node);
    node = node.nextSibling;
  }
  gManagerWindow.sortElements(elements, ["uiState", aSortBy], aAscending);
  for (let element of elements)
    list.appendChild(element);
}

function check_order(aExpectedOrder) {
  var order = [];
  var list = gManagerWindow.document.getElementById("addon-list");
  var node = list.firstChild;
  while (node) {
    var id = node.getAttribute("value");
    if (id && id.endsWith("@tests.mozilla.org"))
      order.push(node.getAttribute("value"));
    node = node.nextSibling;
  }

  is(order.toSource(), aExpectedOrder.toSource(), "Should have seen the right order");
}

// Tests that ascending name ordering was the default
add_test(function() {

  check_order([
    "test2@tests.mozilla.org",
    "test4@tests.mozilla.org",
    "test3@tests.mozilla.org",
    "test5@tests.mozilla.org",
    "test1@tests.mozilla.org",
    "test7@tests.mozilla.org",
    "test8@tests.mozilla.org",
    "test6@tests.mozilla.org",
    "test10@tests.mozilla.org",
    "test9@tests.mozilla.org",
    "test11@tests.mozilla.org",
    "test13@tests.mozilla.org",
    "test12@tests.mozilla.org",
    "test14@tests.mozilla.org",
    "test15@tests.mozilla.org",
    "test17@tests.mozilla.org",
    "test19@tests.mozilla.org",
    "test16@tests.mozilla.org",
    "test20@tests.mozilla.org",
    "test18@tests.mozilla.org",
  ]);
  run_next_test();
});

// Tests that switching to date ordering works
add_test(function() {
  set_order("updateDate", false);

  // When we're ascending with updateDate, it's from newest
  // to oldest.

  check_order([
    "test5@tests.mozilla.org",
    "test3@tests.mozilla.org",
    "test1@tests.mozilla.org",
    "test2@tests.mozilla.org",
    "test4@tests.mozilla.org",
    "test10@tests.mozilla.org",
    "test9@tests.mozilla.org",
    "test8@tests.mozilla.org",
    "test6@tests.mozilla.org",
    "test7@tests.mozilla.org",
    "test12@tests.mozilla.org",
    "test14@tests.mozilla.org",
    "test15@tests.mozilla.org",
    "test13@tests.mozilla.org",
    "test11@tests.mozilla.org",
    "test20@tests.mozilla.org",
    "test17@tests.mozilla.org",
    "test18@tests.mozilla.org",
    "test19@tests.mozilla.org",
    "test16@tests.mozilla.org",
  ]);

  set_order("updateDate", true);

  check_order([
    "test4@tests.mozilla.org",
    "test2@tests.mozilla.org",
    "test1@tests.mozilla.org",
    "test3@tests.mozilla.org",
    "test5@tests.mozilla.org",
    "test7@tests.mozilla.org",
    "test6@tests.mozilla.org",
    "test8@tests.mozilla.org",
    "test9@tests.mozilla.org",
    "test10@tests.mozilla.org",
    "test11@tests.mozilla.org",
    "test13@tests.mozilla.org",
    "test15@tests.mozilla.org",
    "test14@tests.mozilla.org",
    "test12@tests.mozilla.org",
    "test16@tests.mozilla.org",
    "test19@tests.mozilla.org",
    "test18@tests.mozilla.org",
    "test17@tests.mozilla.org",
    "test20@tests.mozilla.org",
  ]);

  run_next_test();
});

// Tests that switching to name ordering works
add_test(function() {
  set_order("name", true);

  check_order([
    "test2@tests.mozilla.org",
    "test4@tests.mozilla.org",
    "test3@tests.mozilla.org",
    "test5@tests.mozilla.org",
    "test1@tests.mozilla.org",
    "test7@tests.mozilla.org",
    "test8@tests.mozilla.org",
    "test6@tests.mozilla.org",
    "test10@tests.mozilla.org",
    "test9@tests.mozilla.org",
    "test11@tests.mozilla.org",
    "test13@tests.mozilla.org",
    "test12@tests.mozilla.org",
    "test14@tests.mozilla.org",
    "test15@tests.mozilla.org",
    "test17@tests.mozilla.org",
    "test19@tests.mozilla.org",
    "test16@tests.mozilla.org",
    "test20@tests.mozilla.org",
    "test18@tests.mozilla.org",
  ]);

  set_order("name", false);

  check_order([
    "test1@tests.mozilla.org",
    "test5@tests.mozilla.org",
    "test3@tests.mozilla.org",
    "test4@tests.mozilla.org",
    "test2@tests.mozilla.org",
    "test9@tests.mozilla.org",
    "test10@tests.mozilla.org",
    "test6@tests.mozilla.org",
    "test8@tests.mozilla.org",
    "test7@tests.mozilla.org",
    "test15@tests.mozilla.org",
    "test14@tests.mozilla.org",
    "test12@tests.mozilla.org",
    "test13@tests.mozilla.org",
    "test11@tests.mozilla.org",
    "test18@tests.mozilla.org",
    "test20@tests.mozilla.org",
    "test16@tests.mozilla.org",
    "test19@tests.mozilla.org",
    "test17@tests.mozilla.org",
  ]);

  run_next_test();
});
