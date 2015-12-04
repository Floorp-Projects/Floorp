/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that upgrading bootstrapped add-ons behaves correctly while the
// manager is open

var gManagerWindow;
var gCategoryUtilities;

add_task(function* test() {
  waitForExplicitFinish();

  gManagerWindow = yield open_manager("addons://list/extension");
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);
});

function get_list_item_count() {
  return get_test_items_in_list(gManagerWindow).length;
}

function get_node(parent, anonid) {
  return parent.ownerDocument.getAnonymousElementByAttribute(parent, "anonid", anonid);
}

function get_class_node(parent, cls) {
  return parent.ownerDocument.getAnonymousElementByAttribute(parent, "class", cls);
}

function install_addon(aXpi) {
  return new Promise(resolve => {
    AddonManager.getInstallForURL(TESTROOT + "addons/" + aXpi + ".xpi",
                                  function(aInstall) {
      aInstall.addListener({
        onInstallEnded: function(aInstall) {
          resolve();
        }
      });
      aInstall.install();
    }, "application/x-xpinstall");
  });
}

var check_addon = Task.async(function*(aAddon, aVersion) {
  is(get_list_item_count(), 1, "Should be one item in the list");
  is(aAddon.version, aVersion, "Add-on should have the right version");

  let item = get_addon_element(gManagerWindow, "addon1@tests.mozilla.org");
  ok(!!item, "Should see the add-on in the list");

  // Force XBL to apply
  item.clientTop;

  let { version } = yield get_tooltip_info(item);
  is(version, aVersion, "Version should be correct");

  if (aAddon.userDisabled)
    is_element_visible(get_class_node(item, "disabled-postfix"), "Disabled postfix should be hidden");
  else
    is_element_hidden(get_class_node(item, "disabled-postfix"), "Disabled postfix should be hidden");
});

// Install version 1 then upgrade to version 2 with the manager open
add_task(function*() {
  yield install_addon("browser_bug596336_1");
  let [aAddon] = yield promiseAddonsByIDs(["addon1@tests.mozilla.org"]);
  yield check_addon(aAddon, "1.0");
  ok(!aAddon.userDisabled, "Add-on should not be disabled");

  yield install_addon("browser_bug596336_2");
  [aAddon] = yield promiseAddonsByIDs(["addon1@tests.mozilla.org"]);
  yield check_addon(aAddon, "2.0");
  ok(!aAddon.userDisabled, "Add-on should not be disabled");

  aAddon.uninstall();

  is(get_list_item_count(), 0, "Should be no items in the list");
});

// Install version 1 mark it as disabled then upgrade to version 2 with the
// manager open
add_task(function*() {
  yield install_addon("browser_bug596336_1");
  let [aAddon] = yield promiseAddonsByIDs(["addon1@tests.mozilla.org"]);
  aAddon.userDisabled = true;
  yield check_addon(aAddon, "1.0");
  ok(aAddon.userDisabled, "Add-on should be disabled");

  yield install_addon("browser_bug596336_2");
  [aAddon] = yield promiseAddonsByIDs(["addon1@tests.mozilla.org"]);
  yield check_addon(aAddon, "2.0");
  ok(aAddon.userDisabled, "Add-on should be disabled");

  aAddon.uninstall();

  is(get_list_item_count(), 0, "Should be no items in the list");
});

// Install version 1 click the remove button and then upgrade to version 2 with
// the manager open
add_task(function*() {
  yield install_addon("browser_bug596336_1");
  let [aAddon] = yield promiseAddonsByIDs(["addon1@tests.mozilla.org"]);
  yield check_addon(aAddon, "1.0");
  ok(!aAddon.userDisabled, "Add-on should not be disabled");

  let item = get_addon_element(gManagerWindow, "addon1@tests.mozilla.org");
  EventUtils.synthesizeMouseAtCenter(get_node(item, "remove-btn"), { }, gManagerWindow);

  // Force XBL to apply
  item.clientTop;

  ok(aAddon.userDisabled, "Add-on should be disabled");
  ok(!aAddon.pendingUninstall, "Add-on should not be pending uninstall");
  is_element_visible(get_class_node(item, "pending"), "Pending message should be visible");

  yield install_addon("browser_bug596336_2");
  [aAddon] = yield promiseAddonsByIDs(["addon1@tests.mozilla.org"]);
  yield check_addon(aAddon, "2.0");
  ok(!aAddon.userDisabled, "Add-on should not be disabled");

  aAddon.uninstall();

  is(get_list_item_count(), 0, "Should be no items in the list");
});

// Install version 1, disable it, click the remove button and then upgrade to
// version 2 with the manager open
add_task(function*() {
  yield install_addon("browser_bug596336_1");
  let [aAddon] = yield promiseAddonsByIDs(["addon1@tests.mozilla.org"]);
  aAddon.userDisabled = true;
  yield check_addon(aAddon, "1.0");
  ok(aAddon.userDisabled, "Add-on should be disabled");

  let item = get_addon_element(gManagerWindow, "addon1@tests.mozilla.org");
  EventUtils.synthesizeMouseAtCenter(get_node(item, "remove-btn"), { }, gManagerWindow);

  // Force XBL to apply
  item.clientTop;

  ok(aAddon.userDisabled, "Add-on should be disabled");
  ok(!aAddon.pendingUninstall, "Add-on should not be pending uninstall");
  is_element_visible(get_class_node(item, "pending"), "Pending message should be visible");

  yield install_addon("browser_bug596336_2");
  [aAddon] = yield promiseAddonsByIDs(["addon1@tests.mozilla.org"]);
  yield check_addon(aAddon, "2.0");
  ok(aAddon.userDisabled, "Add-on should be disabled");

  aAddon.uninstall();

  is(get_list_item_count(), 0, "Should be no items in the list");
});

add_task(function end_test() {
  close_manager(gManagerWindow, finish);
});
