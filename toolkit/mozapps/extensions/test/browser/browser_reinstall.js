/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that upgrading bootstrapped add-ons behaves correctly while the
// manager is open

const {AddonTestUtils} = ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm", {});

const ID = "reinstall@tests.mozilla.org";

let gManagerWindow, xpi1, xpi2;

function get_list_item_count() {
  return get_test_items_in_list(gManagerWindow).length;
}

function get_node(parent, anonid) {
  return parent.ownerDocument.getAnonymousElementByAttribute(parent, "anonid", anonid);
}

function get_class_node(parent, cls) {
  return parent.ownerDocument.getAnonymousElementByAttribute(parent, "class", cls);
}

async function install_addon(xpi) {
  let install = await AddonManager.getInstallForFile(xpi, "application/x-xpinstall");
  return install.install();
}

var check_addon = async function(aAddon, aVersion) {
  is(get_list_item_count(), 1, "Should be one item in the list");
  is(aAddon.version, aVersion, "Add-on should have the right version");

  let item = get_addon_element(gManagerWindow, ID);
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

add_task(async function setup() {
  gManagerWindow = await open_manager("addons://list/extension");

  xpi1 = await AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: "1.0",
      applications: {gecko: {id: ID}},
    },
  });

  xpi2 = await AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: "2.0",
      applications: {gecko: {id: ID}},
    },
  });
});

// Install version 1 then upgrade to version 2 with the manager open
add_task(async function() {
  await install_addon(xpi1);
  let addon = await promiseAddonByID(ID);
  await check_addon(addon, "1.0");
  ok(!addon.userDisabled, "Add-on should not be disabled");

  await install_addon(xpi2);
  addon = await promiseAddonByID(ID);
  await check_addon(addon, "2.0");
  ok(!addon.userDisabled, "Add-on should not be disabled");

  await addon.uninstall();

  is(get_list_item_count(), 0, "Should be no items in the list");
});

// Install version 1 mark it as disabled then upgrade to version 2 with the
// manager open
add_task(async function() {
  await install_addon(xpi1);
  let addon = await promiseAddonByID(ID);
  await addon.disable();
  await check_addon(addon, "1.0");
  ok(addon.userDisabled, "Add-on should be disabled");

  await install_addon(xpi2);
  addon = await promiseAddonByID(ID);
  await check_addon(addon, "2.0");
  ok(addon.userDisabled, "Add-on should be disabled");

  await addon.uninstall();

  is(get_list_item_count(), 0, "Should be no items in the list");
});

// Install version 1 click the remove button and then upgrade to version 2 with
// the manager open
add_task(async function() {
  await install_addon(xpi1);
  let addon = await promiseAddonByID(ID);
  await check_addon(addon, "1.0");
  ok(!addon.userDisabled, "Add-on should not be disabled");

  let item = get_addon_element(gManagerWindow, ID);
  EventUtils.synthesizeMouseAtCenter(get_node(item, "remove-btn"), { }, gManagerWindow);

  // Force XBL to apply
  item.clientTop;

  ok(!!(addon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should be pending uninstall");
  is_element_visible(get_class_node(item, "pending"), "Pending message should be visible");

  await install_addon(xpi2);
  addon = await promiseAddonByID(ID);
  await check_addon(addon, "2.0");
  ok(!addon.userDisabled, "Add-on should not be disabled");

  await addon.uninstall();

  is(get_list_item_count(), 0, "Should be no items in the list");
});

// Install version 1, disable it, click the remove button and then upgrade to
// version 2 with the manager open
add_task(async function() {
  await install_addon(xpi1);
  let addon = await promiseAddonByID(ID);
  await addon.disable();
  await check_addon(addon, "1.0");
  ok(addon.userDisabled, "Add-on should be disabled");

  let item = get_addon_element(gManagerWindow, ID);
  EventUtils.synthesizeMouseAtCenter(get_node(item, "remove-btn"), { }, gManagerWindow);

  // Force XBL to apply
  item.clientTop;

  ok(!!(addon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should be pending uninstall");
  is_element_visible(get_class_node(item, "pending"), "Pending message should be visible");

  await install_addon(xpi2);
  addon = await promiseAddonByID(ID);
  await check_addon(addon, "2.0");
  ok(addon.userDisabled, "Add-on should be disabled");

  await addon.uninstall();

  is(get_list_item_count(), 0, "Should be no items in the list");
});

add_task(function end_test() {
  return close_manager(gManagerWindow);
});
