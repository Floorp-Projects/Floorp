/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that upgrading bootstrapped add-ons behaves correctly while the
// manager is open

var gManagerWindow;

add_task(async function test() {
  waitForExplicitFinish();

  gManagerWindow = await open_manager("addons://list/extension");
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
  return new Promise(async resolve => {
    let aInstall = await AddonManager.getInstallForURL(TESTROOT + "addons/" + aXpi + ".xpi", "application/x-xpinstall");
    aInstall.addListener({
      onInstallEnded(aInstall) {
        resolve();
      }
    });
    aInstall.install();
  });
}

var check_addon = async function(aAddon, aVersion) {
  is(get_list_item_count(), 1, "Should be one item in the list");
  is(aAddon.version, aVersion, "Add-on should have the right version");

  let item = get_addon_element(gManagerWindow, "bug596336-1@tests.mozilla.org");
  ok(!!item, "Should see the add-on in the list");

  // Force XBL to apply
  item.clientTop;

  let { version } = await get_tooltip_info(item);
  is(version, aVersion, "Version should be correct");

  if (aAddon.userDisabled)
    is_element_visible(get_class_node(item, "disabled-postfix"), "Disabled postfix should be hidden");
  else
    is_element_hidden(get_class_node(item, "disabled-postfix"), "Disabled postfix should be hidden");
};

// Install version 1 then upgrade to version 2 with the manager open
add_task(async function() {
  await install_addon("browser_bug596336_1");
  let [aAddon] = await promiseAddonsByIDs(["bug596336-1@tests.mozilla.org"]);
  await check_addon(aAddon, "1.0");
  ok(!aAddon.userDisabled, "Add-on should not be disabled");

  await install_addon("browser_bug596336_2");
  [aAddon] = await promiseAddonsByIDs(["bug596336-1@tests.mozilla.org"]);
  await check_addon(aAddon, "2.0");
  ok(!aAddon.userDisabled, "Add-on should not be disabled");

  await aAddon.uninstall();

  is(get_list_item_count(), 0, "Should be no items in the list");
});

// Install version 1 mark it as disabled then upgrade to version 2 with the
// manager open
add_task(async function() {
  await install_addon("browser_bug596336_1");
  let [aAddon] = await promiseAddonsByIDs(["bug596336-1@tests.mozilla.org"]);
  await aAddon.disable();
  await check_addon(aAddon, "1.0");
  ok(aAddon.userDisabled, "Add-on should be disabled");

  await install_addon("browser_bug596336_2");
  [aAddon] = await promiseAddonsByIDs(["bug596336-1@tests.mozilla.org"]);
  await check_addon(aAddon, "2.0");
  ok(aAddon.userDisabled, "Add-on should be disabled");

  await aAddon.uninstall();

  is(get_list_item_count(), 0, "Should be no items in the list");
});

// Install version 1 click the remove button and then upgrade to version 2 with
// the manager open
add_task(async function() {
  await install_addon("browser_bug596336_1");
  let [aAddon] = await promiseAddonsByIDs(["bug596336-1@tests.mozilla.org"]);
  await check_addon(aAddon, "1.0");
  ok(!aAddon.userDisabled, "Add-on should not be disabled");

  let item = get_addon_element(gManagerWindow, "bug596336-1@tests.mozilla.org");
  EventUtils.synthesizeMouseAtCenter(get_node(item, "remove-btn"), { }, gManagerWindow);

  // Force XBL to apply
  item.clientTop;

  ok(!!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should be pending uninstall");
  is_element_visible(get_class_node(item, "pending"), "Pending message should be visible");

  await install_addon("browser_bug596336_2");
  [aAddon] = await promiseAddonsByIDs(["bug596336-1@tests.mozilla.org"]);
  await check_addon(aAddon, "2.0");
  ok(!aAddon.userDisabled, "Add-on should not be disabled");

  await aAddon.uninstall();

  is(get_list_item_count(), 0, "Should be no items in the list");
});

// Install version 1, disable it, click the remove button and then upgrade to
// version 2 with the manager open
add_task(async function() {
  await install_addon("browser_bug596336_1");
  let [aAddon] = await promiseAddonsByIDs(["bug596336-1@tests.mozilla.org"]);
  await aAddon.disable();
  await check_addon(aAddon, "1.0");
  ok(aAddon.userDisabled, "Add-on should be disabled");

  let item = get_addon_element(gManagerWindow, "bug596336-1@tests.mozilla.org");
  EventUtils.synthesizeMouseAtCenter(get_node(item, "remove-btn"), { }, gManagerWindow);

  // Force XBL to apply
  item.clientTop;

  ok(!!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should be pending uninstall");
  is_element_visible(get_class_node(item, "pending"), "Pending message should be visible");

  await install_addon("browser_bug596336_2");
  [aAddon] = await promiseAddonsByIDs(["bug596336-1@tests.mozilla.org"]);
  await check_addon(aAddon, "2.0");
  ok(aAddon.userDisabled, "Add-on should be disabled");

  await aAddon.uninstall();

  is(get_list_item_count(), 0, "Should be no items in the list");
});

add_task(function end_test() {
  close_manager(gManagerWindow, finish);
});
