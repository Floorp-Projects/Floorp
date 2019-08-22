/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that upgrading bootstrapped add-ons behaves correctly while the
// manager is open

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

const ID = "reinstall@tests.mozilla.org";
const testIdSuffix = "@tests.mozilla.org";

let gManagerWindow, xpi1, xpi2;

function get_test_items_in_list(aManager) {
  let item = aManager.document.getElementById("addon-list").firstChild;
  let items = [];

  while (item) {
    if (item.localName != "richlistitem") {
      item = item.nextSibling;
      continue;
    }

    if (
      !item.mAddon ||
      item.mAddon.id.substring(item.mAddon.id.length - testIdSuffix.length) ==
        testIdSuffix
    ) {
      items.push(item);
    }
    item = item.nextSibling;
  }

  return items;
}

function htmlDoc() {
  return gManagerWindow.document.getElementById("html-view-browser")
    .contentDocument;
}

function get_list_item_count() {
  if (gManagerWindow.useHtmlViews) {
    return htmlDoc().querySelectorAll(`addon-card[addon-id$="${testIdSuffix}"]`)
      .length;
  }
  return get_test_items_in_list(gManagerWindow).length;
}

function get_node(parent, anonid) {
  return parent.ownerDocument.getAnonymousElementByAttribute(
    parent,
    "anonid",
    anonid
  );
}

function removeItem(item) {
  let button;
  if (gManagerWindow.useHtmlViews) {
    button = item.querySelector('[action="remove"]');
    button.click();
  } else {
    button = get_node(item, "remove-btn");
    EventUtils.synthesizeMouseAtCenter(button, {}, button.ownerGlobal);
  }
}

function get_class_node(parent, cls) {
  return parent.ownerDocument.getAnonymousElementByAttribute(
    parent,
    "class",
    cls
  );
}

function hasPendingMessage(item, msg) {
  if (gManagerWindow.useHtmlViews) {
    let messageBar = htmlDoc().querySelector(
      `message-bar[addon-id="${item.addon.id}"`
    );
    is_element_visible(messageBar, msg);
  } else {
    is_element_visible(get_class_node(item, "pending"), msg);
  }
}

async function install_addon(xpi) {
  let install = await AddonManager.getInstallForFile(
    xpi,
    "application/x-xpinstall"
  );
  return install.install();
}

async function check_addon(aAddon, aVersion) {
  is(get_list_item_count(), 1, "Should be one item in the list");
  is(aAddon.version, aVersion, "Add-on should have the right version");

  let item = get_addon_element(gManagerWindow, ID);
  ok(!!item, "Should see the add-on in the list");

  // Force XBL to apply
  item.clientTop;

  let { version } = await get_tooltip_info(item, gManagerWindow);
  is(version, aVersion, "Version should be correct");

  if (gManagerWindow.useHtmlViews) {
    const l10nAttrs = item.ownerDocument.l10n.getAttributes(item.addonNameEl);
    if (aAddon.userDisabled) {
      Assert.deepEqual(
        l10nAttrs,
        { id: "addon-name-disabled", args: { name: aAddon.name } },
        "localized addon name is marked as disabled"
      );
    } else {
      Assert.deepEqual(
        l10nAttrs,
        { id: null, args: null },
        "localized addon name is not marked as disabled"
      );
    }

    return;
  }

  if (aAddon.userDisabled) {
    is_element_visible(
      get_class_node(item, "disabled-postfix"),
      "Disabled postfix should be hidden"
    );
  } else {
    is_element_hidden(
      get_class_node(item, "disabled-postfix"),
      "Disabled postfix should be hidden"
    );
  }
}

async function wait_for_addon_item_added(addonId) {
  if (gManagerWindow.useHtmlViews) {
    await BrowserTestUtils.waitForEvent(
      htmlDoc().querySelector("addon-list"),
      "add"
    );
    const item = get_addon_element(gManagerWindow, addonId);
    ok(item, `Found addon card for ${addonId}`);
  }
}

async function wait_for_addon_item_removed(addonId) {
  if (gManagerWindow.useHtmlViews) {
    await BrowserTestUtils.waitForEvent(
      htmlDoc().querySelector("addon-list"),
      "remove"
    );
    const item = get_addon_element(gManagerWindow, addonId);
    ok(!item, `There shouldn't be an addon card for ${addonId}`);
  }
}

async function wait_for_addon_item_updated(addonId) {
  if (gManagerWindow.useHtmlViews) {
    await BrowserTestUtils.waitForEvent(
      get_addon_element(gManagerWindow, addonId),
      "update"
    );
  }
}

// Install version 1 then upgrade to version 2 with the manager open
async function test_upgrade_v1_to_v2() {
  let promiseItemAdded = wait_for_addon_item_added(ID);
  await install_addon(xpi1);
  await promiseItemAdded;

  let addon = await promiseAddonByID(ID);
  await check_addon(addon, "1.0");
  ok(!addon.userDisabled, "Add-on should not be disabled");

  let promiseItemUpdated = wait_for_addon_item_updated(ID);
  await install_addon(xpi2);
  await promiseItemUpdated;

  addon = await promiseAddonByID(ID);
  await check_addon(addon, "2.0");
  ok(!addon.userDisabled, "Add-on should not be disabled");

  let promiseItemRemoved = wait_for_addon_item_removed(ID);
  await addon.uninstall();
  await promiseItemRemoved;

  is(get_list_item_count(), 0, "Should be no items in the list");
}

// Install version 1 mark it as disabled then upgrade to version 2 with the
// manager open
async function test_upgrade_disabled_v1_to_v2() {
  let promiseItemAdded = wait_for_addon_item_added(ID);
  await install_addon(xpi1);
  await promiseItemAdded;

  let promiseItemUpdated = wait_for_addon_item_updated(ID);
  let addon = await promiseAddonByID(ID);
  await addon.disable();
  await promiseItemUpdated;

  await check_addon(addon, "1.0");
  ok(addon.userDisabled, "Add-on should be disabled");

  promiseItemUpdated = wait_for_addon_item_updated(ID);
  await install_addon(xpi2);
  await promiseItemUpdated;

  addon = await promiseAddonByID(ID);
  await check_addon(addon, "2.0");
  ok(addon.userDisabled, "Add-on should be disabled");

  let promiseItemRemoved = wait_for_addon_item_removed(ID);
  await addon.uninstall();
  await promiseItemRemoved;

  is(get_list_item_count(), 0, "Should be no items in the list");
}

// Install version 1 click the remove button and then upgrade to version 2 with
// the manager open
async function test_upgrade_pending_uninstall_v1_to_v2() {
  let promiseItemAdded = wait_for_addon_item_added(ID);
  await install_addon(xpi1);
  await promiseItemAdded;

  let addon = await promiseAddonByID(ID);
  await check_addon(addon, "1.0");
  ok(!addon.userDisabled, "Add-on should not be disabled");

  let item = get_addon_element(gManagerWindow, ID);

  let promiseItemRemoved = wait_for_addon_item_removed(ID);
  removeItem(item);

  // Force XBL to apply
  item.clientTop;

  await promiseItemRemoved;

  ok(
    !!(addon.pendingOperations & AddonManager.PENDING_UNINSTALL),
    "Add-on should be pending uninstall"
  );
  hasPendingMessage(item, "Pending message should be visible");

  promiseItemAdded = wait_for_addon_item_added(ID);
  await install_addon(xpi2);
  await promiseItemAdded;

  addon = await promiseAddonByID(ID);
  await check_addon(addon, "2.0");
  ok(!addon.userDisabled, "Add-on should not be disabled");

  promiseItemRemoved = wait_for_addon_item_removed(ID);
  await addon.uninstall();
  await promiseItemRemoved;

  is(get_list_item_count(), 0, "Should be no items in the list");
}

// Install version 1, disable it, click the remove button and then upgrade to
// version 2 with the manager open
async function test_upgrade_pending_uninstall_disabled_v1_to_v2() {
  let promiseItemAdded = wait_for_addon_item_added(ID);
  await install_addon(xpi1);
  await promiseItemAdded;

  let promiseItemUpdated = wait_for_addon_item_updated(ID);
  let addon = await promiseAddonByID(ID);
  await addon.disable();
  await promiseItemUpdated;

  await check_addon(addon, "1.0");
  ok(addon.userDisabled, "Add-on should be disabled");

  let item = get_addon_element(gManagerWindow, ID);

  let promiseItemRemoved = wait_for_addon_item_removed(ID);
  removeItem(item);

  // Force XBL to apply
  item.clientTop;

  await promiseItemRemoved;
  ok(
    !!(addon.pendingOperations & AddonManager.PENDING_UNINSTALL),
    "Add-on should be pending uninstall"
  );
  hasPendingMessage(item, "Pending message should be visible");

  promiseItemAdded = wait_for_addon_item_added(ID);
  await install_addon(xpi2);
  addon = await promiseAddonByID(ID);

  await promiseItemAdded;
  await check_addon(addon, "2.0");
  ok(addon.userDisabled, "Add-on should be disabled");

  promiseItemRemoved = wait_for_addon_item_removed(ID);
  await addon.uninstall();
  await promiseItemRemoved;

  is(get_list_item_count(), 0, "Should be no items in the list");
}

async function test_upgrades(useHtmlViews) {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", useHtmlViews]],
  });

  // Close existing about:addons tab if a test failure has
  // prevented it from being closed.
  if (gManagerWindow) {
    await close_manager(gManagerWindow);
  }

  gManagerWindow = await open_manager("addons://list/extension");
  is(
    gManagerWindow.useHtmlViews,
    useHtmlViews,
    "Got about:addons window in the expected mode"
  );

  await test_upgrade_v1_to_v2();
  await test_upgrade_disabled_v1_to_v2();
  await test_upgrade_pending_uninstall_v1_to_v2();
  await test_upgrade_pending_uninstall_disabled_v1_to_v2();

  await close_manager(gManagerWindow);
  gManagerWindow = null;

  // No popPrefEnv because of bug 1557397.
}

add_task(async function setup() {
  xpi1 = await AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: "1.0",
      applications: { gecko: { id: ID } },
    },
  });

  xpi2 = await AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: "2.0",
      applications: { gecko: { id: ID } },
    },
  });

  // Accept all prompts.
  mockPromptService()._response = 0;
});

add_task(function run_tests_on_XUL_aboutaddons() {
  return test_upgrades(false);
});

add_task(function run_tests_on_HTML_aboutaddons() {
  return test_upgrades(true);
});
