/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests the list view

var tempScope = {};
ChromeUtils.import("resource://gre/modules/LightweightThemeManager.jsm", tempScope);
var LightweightThemeManager = tempScope.LightweightThemeManager;
 ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

var gProvider;
var gManagerWindow;
var gCategoryUtilities;

var gApp = document.getElementById("bundle_brand").getString("brandShortName");
var gVersion = Services.appinfo.version;
var gDate = new Date(2010, 7, 16);
var infoURL = Services.urlFormatter.formatURLPref("app.support.baseURL") + "unsigned-addons";

const EXPECTED_ADDONS = 11;

var gLWTheme = {
                id: "4",
                version: "1",
                name: "Bling",
                description: "SO MUCH BLING!",
                author: "Pixel Pusher",
                homepageURL: "http://mochi.test:8888/data/index.html",
                headerURL: "http://mochi.test:8888/data/header.png",
                previewURL: "http://mochi.test:8888/data/preview.png",
                iconURL: "http://mochi.test:8888/data/icon.png"
              };

add_task(async function() {
  gProvider = new MockProvider();

  gProvider.createAddons([{
    id: "addon1@tests.mozilla.org",
    name: "Test add-on",
    version: "1.0",
    description: "A test add-on",
    longDescription: " A longer description",
    updateDate: gDate
  }, {
    id: "addon2@tests.mozilla.org",
    name: "Test add-on 2",
    version: "2.0",
    longDescription: " A longer description",
    _userDisabled: true,
    isActive: false,
  }, {
    id: "addon3@tests.mozilla.org",
    name: "Test add-on 3",
    longDescription: " A longer description",
    isActive: false,
    isCompatible: false,
    appDisabled: true,
    permissions: AddonManager.PERM_CAN_ENABLE |
                 AddonManager.PERM_CAN_DISABLE |
                 AddonManager.PERM_CAN_UPGRADE
  }, {
    id: "addon5@tests.mozilla.org",
    blocklistURL: "http://example.com/addon5@tests.mozilla.org",
    name: "Test add-on 5",
    isActive: false,
    blocklistState: Ci.nsIBlocklistService.STATE_BLOCKED,
    appDisabled: true
  }, {
    id: "addon6@tests.mozilla.org",
    blocklistURL: "http://example.com/addon6@tests.mozilla.org",
    name: "Test add-on 6",
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE
  }, {
    id: "addon8@tests.mozilla.org",
    blocklistURL: "http://example.com/addon8@tests.mozilla.org",
    name: "Test add-on 8",
    blocklistState: Ci.nsIBlocklistService.STATE_VULNERABLE_UPDATE_AVAILABLE,
  }, {
    id: "addon9@tests.mozilla.org",
    blocklistURL: "http://example.com/addon9@tests.mozilla.org",
    name: "Test add-on 9",
    blocklistState: Ci.nsIBlocklistService.STATE_VULNERABLE_NO_UPDATE,
  }, {
    id: "addon10@tests.mozilla.org",
    name: "Test add-on 10",
    signedState: AddonManager.SIGNEDSTATE_MISSING,
  }, {
    id: "addon11@tests.mozilla.org",
    name: "Test add-on 11",
    signedState: AddonManager.SIGNEDSTATE_MISSING,
    isActive: false,
    isCompatible: false,
    appDisabled: true,
  }, {
    id: "addon12@tests.mozilla.org",
    name: "Test add-on 12",
    signedState: AddonManager.SIGNEDSTATE_PRELIMINARY,
    foreignInstall: true,
  }, {
    id: "addon13@tests.mozilla.org",
    name: "Test add-on 13",
    signedState: AddonManager.SIGNEDSTATE_SIGNED,
    foreignInstall: true,
  }, {
    id: "addon15@tests.mozilla.org",
    name: "Test add-on 15",
    hidden: true,
  }]);

  gManagerWindow = await open_manager(null);
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);
});

function get_test_items() {
  var tests = "@tests.mozilla.org";

  var items = {};
  var item = gManagerWindow.document.getElementById("addon-list").firstChild;

  while (item) {
    if (item.mAddon.id.substring(item.mAddon.id.length - tests.length) == tests &&
        !BrowserTestUtils.is_hidden(item))
      items[item.mAddon.name] = item;
    item = item.nextSibling;
  }

  return items;
}

function get_node(parent, anonid) {
  return parent.ownerDocument.getAnonymousElementByAttribute(parent, "anonid", anonid);
}

function get_class_node(parent, cls) {
  return parent.ownerDocument.getAnonymousElementByAttribute(parent, "class", cls);
}

// Check that the list appears to have displayed correctly and trigger some
// changes
add_task(async function() {
  await gCategoryUtilities.openType("extension");
  let items = get_test_items();
  is(Object.keys(items).length, EXPECTED_ADDONS, "Should be the right number of add-ons installed");

  info("Addon 3");
  let addon = items["Test add-on 3"];
  addon.parentNode.ensureElementIsVisible(addon);
  let { name, version } = await get_tooltip_info(addon);
  is(get_node(addon, "name").value, "Test add-on 3", "Name should be correct");
  is(name, "Test add-on 3", "Tooltip name should be correct");
  is(version, undefined, "Tooltip version should be hidden");

  is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
  is_element_hidden(get_node(addon, "enable-btn"), "Enable button should be hidden");
  is_element_hidden(get_node(addon, "disable-btn"), "Disable button should be hidden");
  is_element_hidden(get_node(addon, "remove-btn"), "Remove button should be hidden");

  is_element_visible(get_node(addon, "warning"), "Warning message should be visible");
  is(get_node(addon, "warning").textContent, "Test add-on 3 is incompatible with " + gApp + " " + gVersion + ".", "Warning message should be correct");
  is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
  is_element_hidden(get_node(addon, "error"), "Error message should be hidden");
  is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");
  is_element_hidden(get_node(addon, "pending"), "Pending message should be hidden");

  info("Addon 5");
  addon = items["Test add-on 5"];
  addon.parentNode.ensureElementIsVisible(addon);
  await TestUtils.waitForCondition(() => !BrowserTestUtils.is_hidden(get_node(addon, "error-link")));
  ({ name, version } = await get_tooltip_info(addon));
  is(get_node(addon, "name").value, "Test add-on 5", "Name should be correct");
  is(name, "Test add-on 5", "Tooltip name should be correct");

  is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
  is_element_hidden(get_node(addon, "enable-btn"), "Enable button should be hidden");
  is_element_hidden(get_node(addon, "disable-btn"), "Disable button should be hidden");
  is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

  is_element_hidden(get_node(addon, "warning"), "Warning message should be hidden");
  is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
  is_element_visible(get_node(addon, "error"), "Error message should be visible");
  is(get_node(addon, "error").textContent, "Test add-on 5 has been disabled due to security or stability issues.", "Error message should be correct");
  is_element_visible(get_node(addon, "error-link"), "Error link should be visible");
  is(get_node(addon, "error-link").value, "More Information", "Error link text should be correct");
  is(get_node(addon, "error-link").href, "http://example.com/addon5@tests.mozilla.org", "Error link should be correct");
  is_element_hidden(get_node(addon, "pending"), "Pending message should be hidden");

  info("Addon 6");
  addon = items["Test add-on 6"];
  addon.parentNode.ensureElementIsVisible(addon);
  ({ name, version } = await get_tooltip_info(addon));
  is(get_node(addon, "name").value, "Test add-on 6", "Name should be correct");
  is(name, "Test add-on 6", "Tooltip name should be correct");
  is_element_hidden(get_class_node(addon, "disabled-postfix"), "Disabled postfix should be hidden");

  is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
  is_element_hidden(get_node(addon, "enable-btn"), "Enable button should be hidden");
  is_element_visible(get_node(addon, "disable-btn"), "Disable button should be visible");
  is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

  is_element_hidden(get_node(addon, "warning"), "Warning message should be hidden");
  is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
  is_element_hidden(get_node(addon, "error"), "Error message should be visible");
  is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");
  is_element_hidden(get_node(addon, "pending"), "Pending message should be hidden");

  info("Disabling");
  EventUtils.synthesizeMouseAtCenter(get_node(addon, "disable-btn"), {}, gManagerWindow);
  await new Promise(executeSoon);
  is_element_visible(get_class_node(addon, "disabled-postfix"), "Disabled postfix should be visible");

  is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
  is_element_visible(get_node(addon, "enable-btn"), "Enable button should be visible");
  is_element_hidden(get_node(addon, "disable-btn"), "Disable button should be hidden");
  is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

  is_element_hidden(get_node(addon, "warning"), "Warning message should be hidden");
  is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
  is_element_hidden(get_node(addon, "error"), "Error message should be visible");
  is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");
  is_element_hidden(get_node(addon, "pending"), "Pending message should be hidden");

  info("Addon 8");
  addon = items["Test add-on 8"];
  addon.parentNode.ensureElementIsVisible(addon);
  await TestUtils.waitForCondition(() => !BrowserTestUtils.is_hidden(get_node(addon, "error-link")));
  ({ name, version } = await get_tooltip_info(addon));
  is(get_node(addon, "name").value, "Test add-on 8", "Name should be correct");
  is(name, "Test add-on 8", "Tooltip name should be correct");

  is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
  is_element_hidden(get_node(addon, "enable-btn"), "Enable button should be hidden");
  is_element_visible(get_node(addon, "disable-btn"), "Disable button should be visible");
  is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

  is_element_hidden(get_node(addon, "warning"), "Warning message should be hidden");
  is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
  is_element_visible(get_node(addon, "error"), "Error message should be visible");
  is(get_node(addon, "error").textContent, "Test add-on 8 is known to be vulnerable and should be updated.", "Error message should be correct");
  is_element_visible(get_node(addon, "error-link"), "Error link should be visible");
  is(get_node(addon, "error-link").value, "Update Now", "Error link text should be correct");
  is(get_node(addon, "error-link").href, "http://example.com/addon8@tests.mozilla.org", "Error link should be correct");
  is_element_hidden(get_node(addon, "pending"), "Pending message should be hidden");

  info("Addon 9");
  addon = items["Test add-on 9"];
  addon.parentNode.ensureElementIsVisible(addon);
  await TestUtils.waitForCondition(() => !BrowserTestUtils.is_hidden(get_node(addon, "error-link")));
  ({ name, version } = await get_tooltip_info(addon));
  is(get_node(addon, "name").value, "Test add-on 9", "Name should be correct");
  is(name, "Test add-on 9", "Tooltip name should be correct");

  is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
  is_element_hidden(get_node(addon, "enable-btn"), "Enable button should be hidden");
  is_element_visible(get_node(addon, "disable-btn"), "Disable button should be visible");
  is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

  is_element_hidden(get_node(addon, "warning"), "Warning message should be hidden");
  is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
  is_element_visible(get_node(addon, "error"), "Error message should be visible");
  is(get_node(addon, "error").textContent, "Test add-on 9 is known to be vulnerable. Use with caution.", "Error message should be correct");
  is_element_visible(get_node(addon, "error-link"), "Error link should be visible");
  is(get_node(addon, "error-link").value, "More Information", "Error link text should be correct");
  is(get_node(addon, "error-link").href, "http://example.com/addon9@tests.mozilla.org", "Error link should be correct");
  is_element_hidden(get_node(addon, "pending"), "Pending message should be hidden");

  // These tests are only appropriate when signing can be turned off
  if (!AppConstants.MOZ_REQUIRE_SIGNING) {
    info("Addon 10");
    addon = items["Test add-on 10"];
    addon.parentNode.ensureElementIsVisible(addon);
    ({ name, version } = await get_tooltip_info(addon));
    is(get_node(addon, "name").value, "Test add-on 10", "Name should be correct");
    is(name, "Test add-on 10", "Tooltip name should be correct");

    is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
    is_element_hidden(get_node(addon, "enable-btn"), "Enable button should be hidden");
    is_element_visible(get_node(addon, "disable-btn"), "Disable button should be visible");
    is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

    is_element_visible(get_node(addon, "warning"), "Warning message should be visible");
    is(get_node(addon, "warning").textContent, "Test add-on 10 could not be verified for use in " + gApp + ". Proceed with caution.", "Warning message should be correct");
    is_element_visible(get_node(addon, "warning-link"), "Warning link should be visible");
    is(get_node(addon, "warning-link").value, "More Information", "Warning link text should be correct");
    is(get_node(addon, "warning-link").href, infoURL, "Warning link should be correct");
    is_element_hidden(get_node(addon, "error"), "Error message should be hidden");
    is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");
    is_element_hidden(get_node(addon, "pending"), "Pending message should be hidden");

    info("Addon 11");
    addon = items["Test add-on 11"];
    addon.parentNode.ensureElementIsVisible(addon);
    ({ name, version } = await get_tooltip_info(addon));
    is(get_node(addon, "name").value, "Test add-on 11", "Name should be correct");
    is(name, "Test add-on 11", "Tooltip name should be correct");

    is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
    is_element_hidden(get_node(addon, "enable-btn"), "Enable button should be hidden");
    is_element_hidden(get_node(addon, "disable-btn"), "Disable button should be hidden");
    is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

    is_element_visible(get_node(addon, "warning"), "Warning message should be visible");
    is(get_node(addon, "warning").textContent, "Test add-on 11 is incompatible with " + gApp + " " + gVersion + ".", "Warning message should be correct");
    is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
    is_element_hidden(get_node(addon, "error"), "Error message should be hidden");
    is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");
    is_element_hidden(get_node(addon, "pending"), "Pending message should be hidden");

    info("Filter for disabled unsigned extensions shouldn't appear because signing checks are off");
    let filterButton = gManagerWindow.document.getElementById("show-disabled-unsigned-extensions");
    let showAllButton = gManagerWindow.document.getElementById("show-all-extensions");
    let signingInfoUI = gManagerWindow.document.getElementById("disabled-unsigned-addons-info");
    is_element_hidden(filterButton, "Button for showing disabled unsigned extensions should be hidden");
    is_element_hidden(showAllButton, "Button for showing all extensions should be hidden");
    is_element_hidden(signingInfoUI, "Signing info UI should be hidden");
  }
});

// Reload the list to make sure the changes are still pending and that undoing
// works
add_task(async function() {
  await gCategoryUtilities.openType("plugin");
  await gCategoryUtilities.openType("extension");

  let items = get_test_items();
  is(Object.keys(items).length, EXPECTED_ADDONS, "Should be the right number of add-ons installed");

  info("Addon 6");
  let addon = items["Test add-on 6"];
  addon.parentNode.ensureElementIsVisible(addon);
  let { name } = await get_tooltip_info(addon);
  is(get_node(addon, "name").value, "Test add-on 6", "Name should be correct");
  is(name, "Test add-on 6", "Tooltip name should be correct");
  is_element_visible(get_class_node(addon, "disabled-postfix"), "Disabled postfix should be visible");

  is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
  is_element_visible(get_node(addon, "enable-btn"), "Enable button should be visible");
  is_element_hidden(get_node(addon, "disable-btn"), "Disable button should be hidden");
  is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

  is_element_hidden(get_node(addon, "warning"), "Warning message should be hidden");
  is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
  is_element_hidden(get_node(addon, "error"), "Error message should be visible");
  is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");
  is_element_hidden(get_node(addon, "pending"), "Pending message should be hidden");

  info("Enabling");
  EventUtils.synthesizeMouseAtCenter(get_node(addon, "enable-btn"), {}, gManagerWindow);
  await new Promise(executeSoon);
  is_element_hidden(get_class_node(addon, "disabled-postfix"), "Disabled postfix should be hidden");

  is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
  is_element_hidden(get_node(addon, "enable-btn"), "Enable button should be hidden");
  is_element_visible(get_node(addon, "disable-btn"), "Disable button should be visible");
  is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

  is_element_hidden(get_node(addon, "warning"), "Warning message should be hidden");
  is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
  is_element_hidden(get_node(addon, "error"), "Error message should be visible");
  is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");
  is_element_hidden(get_node(addon, "pending"), "Pending message should be hidden");
});

// Check that upgrades with onExternalInstall take effect immediately
add_task(async function() {
  gProvider.createAddons([{
    id: "addon1@tests.mozilla.org",
    name: "Test add-on replacement",
    version: "2.0",
    description: "A test add-on with a new description",
    updateDate: gDate,
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE
  }, {
    id: "addon14@tests.mozilla.org",
    name: "Test add-on 14",
    hidden: true,
  }]);

  let items = get_test_items();
  is(Object.keys(items).length, EXPECTED_ADDONS, "Should be the right number of add-ons installed");

  let addon = items["Test add-on replacement"];
  addon.parentNode.ensureElementIsVisible(addon);
  let { name, version } = await get_tooltip_info(addon);
  is(get_node(addon, "name").value, "Test add-on replacement", "Name should be correct");
  is(name, "Test add-on replacement", "Tooltip name should be correct");
  is(version, "2.0", "Tooltip version should be correct");
  is_element_visible(get_node(addon, "description"), "Description should be visible");
  is(get_node(addon, "description").value, "A test add-on with a new description", "Description should be correct");
  is_element_hidden(get_class_node(addon, "disabled-postfix"), "Disabled postfix should be hidden");
  is_element_hidden(get_class_node(addon, "update-postfix"), "Update postfix should be hidden");
  is(get_node(addon, "date-updated").value, formatDate(gDate), "Update date should be correct");

  is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
  is_element_hidden(get_node(addon, "enable-btn"), "Enable button should be hidden");
  is_element_visible(get_node(addon, "disable-btn"), "Disable button should be visible");
  is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

  is_element_hidden(get_node(addon, "warning"), "Warning message should be hidden");
  is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
  is_element_hidden(get_node(addon, "error"), "Error message should be hidden");
  is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");
  is_element_hidden(get_node(addon, "pending"), "Pending message should be hidden");
});

// Check that focus changes correctly move around the selected list item
add_task(async function() {
  function is_node_in_list(aNode) {
    var list = gManagerWindow.document.getElementById("addon-list");

    while (aNode && aNode != list)
      aNode = aNode.parentNode;

    if (aNode)
      return true;
    return false;
  }

  // Ignore the OSX full keyboard access setting
  Services.prefs.setBoolPref("accessibility.tabfocus_applies_to_xul", false);

  let items = get_test_items();
  is(Object.keys(items).length, EXPECTED_ADDONS, "Should be the right number of add-ons installed");

  let addon = items["Test add-on 6"];
  addon.parentNode.ensureElementIsVisible(addon);
  EventUtils.synthesizeMouseAtCenter(addon, { }, gManagerWindow);
  is(Services.focus.focusedElement, addon.parentNode, "Focus should have moved to the list");

  EventUtils.synthesizeKey("VK_TAB", { }, gManagerWindow);
  is(Services.focus.focusedElement, get_node(addon, "details-btn"), "Focus should have moved to the more button");

  EventUtils.synthesizeKey("VK_TAB", { }, gManagerWindow);
  is(Services.focus.focusedElement, get_node(addon, "disable-btn"), "Focus should have moved to the disable button");

  EventUtils.synthesizeKey("VK_TAB", { }, gManagerWindow);
  is(Services.focus.focusedElement, get_node(addon, "remove-btn"), "Focus should have moved to the remove button");

  EventUtils.synthesizeKey("VK_TAB", { }, gManagerWindow);
  ok(!is_node_in_list(Services.focus.focusedElement), "Focus should be outside the list");

  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true }, gManagerWindow);
  is(Services.focus.focusedElement, get_node(addon, "remove-btn"), "Focus should have moved to the remove button");

  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true }, gManagerWindow);
  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true }, gManagerWindow);
  is(Services.focus.focusedElement, get_node(addon, "details-btn"), "Focus should have moved to the more button");

  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true }, gManagerWindow);
  is(Services.focus.focusedElement, addon.parentNode, "Focus should have moved to the list");

  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true }, gManagerWindow);
  ok(!is_node_in_list(Services.focus.focusedElement), "Focus should be outside the list");

  try {
    Services.prefs.clearUserPref("accessibility.tabfocus_applies_to_xul");
  } catch (e) { }
});


add_task(async function() {
  info("Enabling lightweight theme");
  LightweightThemeManager.currentTheme = gLWTheme;

  gManagerWindow.loadView("addons://list/theme");
  await new Promise(resolve => wait_for_view_load(gManagerWindow, resolve));

  var addon = get_addon_element(gManagerWindow, "4@personas.mozilla.org");

  is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
  is_element_hidden(get_node(addon, "enable-btn"), "Enable button should be hidden");
  is_element_visible(get_node(addon, "disable-btn"), "Disable button should be visible");
  is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

  info("Disabling lightweight theme");
  LightweightThemeManager.currentTheme = null;

  is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
  is_element_visible(get_node(addon, "enable-btn"), "Enable button should be hidden");
  is_element_hidden(get_node(addon, "disable-btn"), "Disable button should be visible");
  is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

  let [aAddon] = await promiseAddonsByIDs(["4@personas.mozilla.org"]);
  aAddon.uninstall();
});

// Check that onPropertyChanges for appDisabled updates the UI
add_task(async function() {
  info("Checking that onPropertyChanges for appDisabled updates the UI");

  let [aAddon] = await promiseAddonsByIDs(["addon2@tests.mozilla.org"]);
  await aAddon.disable();
  aAddon.isCompatible = true;
  aAddon.appDisabled = false;

  gManagerWindow.loadView("addons://list/extension");
  await new Promise(resolve => wait_for_view_load(gManagerWindow, resolve));
  var el = get_addon_element(gManagerWindow, "addon2@tests.mozilla.org");

  is(el.getAttribute("active"), "false", "Addon should not be marked as active");
  is_element_hidden(get_node(el, "warning"), "Warning message should not be visible");

  info("Making addon incompatible and appDisabled");
  aAddon.isCompatible = false;
  aAddon.appDisabled = true;

  is(el.getAttribute("active"), "false", "Addon should not be marked as active");
  is_element_visible(get_node(el, "warning"), "Warning message should be visible");
  is(get_node(el, "warning").textContent, "Test add-on 2 is incompatible with " + gApp + " " + gVersion + ".", "Warning message should be correct");
});

// Check that the list displays correctly when signing is required
add_task(async function() {
  await close_manager(gManagerWindow);
  Services.prefs.setBoolPref("xpinstall.signatures.required", true);
  gManagerWindow = await open_manager(null);
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  await gCategoryUtilities.openType("extension");
  let items = get_test_items();
  is(Object.keys(items).length, EXPECTED_ADDONS, "Should be the right number of add-ons installed");

  info("Addon 10");
  let addon = items["Test add-on 10"];
  addon.parentNode.ensureElementIsVisible(addon);
  await TestUtils.waitForCondition(() => !BrowserTestUtils.is_hidden(get_node(addon, "error-link")));
  let { name } = await get_tooltip_info(addon);
  is(get_node(addon, "name").value, "Test add-on 10", "Name should be correct");
  is(name, "Test add-on 10", "Tooltip name should be correct");

  is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
  is_element_hidden(get_node(addon, "enable-btn"), "Enable button should be hidden");
  is_element_visible(get_node(addon, "disable-btn"), "Disable button should be visible");
  is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

  is_element_hidden(get_node(addon, "warning"), "Warning message should be hidden");
  is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
  is_element_visible(get_node(addon, "error"), "Error message should be visible");
  is(get_node(addon, "error").textContent, "Test add-on 10 could not be verified for use in " + gApp + " and has been disabled.", "Error message should be correct");
  is_element_visible(get_node(addon, "error-link"), "Error link should be visible");
  is(get_node(addon, "error-link").value, "More Information", "Error link text should be correct");
  is(get_node(addon, "error-link").href, infoURL, "Error link should be correct");

  info("Addon 11");
  addon = items["Test add-on 11"];
  addon.parentNode.ensureElementIsVisible(addon);
  await TestUtils.waitForCondition(() => !BrowserTestUtils.is_hidden(get_node(addon, "error-link")));
  ({ name } = await get_tooltip_info(addon));
  is(get_node(addon, "name").value, "Test add-on 11", "Name should be correct");
  is(name, "Test add-on 11", "Tooltip name should be correct");

  is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
  is_element_hidden(get_node(addon, "enable-btn"), "Enable button should be hidden");
  is_element_hidden(get_node(addon, "disable-btn"), "Disable button should be hidden");
  is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

  is_element_hidden(get_node(addon, "warning"), "Warning message should be hidden");
  is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
  is_element_visible(get_node(addon, "error"), "Error message should be visible");
  is(get_node(addon, "error").textContent, "Test add-on 11 could not be verified for use in " + gApp + " and has been disabled.", "Error message should be correct");
  is_element_visible(get_node(addon, "error-link"), "Error link should be visible");
  is(get_node(addon, "error-link").value, "More Information", "Error link text should be correct");
  is(get_node(addon, "error-link").href, infoURL, "Error link should be correct");

  info("Addon 12");
  addon = items["Test add-on 12"];
  addon.parentNode.ensureElementIsVisible(addon);
  ({ name } = await get_tooltip_info(addon));
  is(get_node(addon, "name").value, "Test add-on 12", "Name should be correct");
  is(name, "Test add-on 12", "Tooltip name should be correct");

  is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
  is_element_hidden(get_node(addon, "enable-btn"), "Enable button should be hidden");
  is_element_visible(get_node(addon, "disable-btn"), "Disable button should be visible");
  is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

  is_element_hidden(get_node(addon, "warning"), "Warning message should be hidden");
  is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
  is_element_hidden(get_node(addon, "error"), "Error message should be hidden");
  is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");

  info("Addon 13");
  addon = items["Test add-on 13"];
  addon.parentNode.ensureElementIsVisible(addon);
  ({ name } = await get_tooltip_info(addon));
  is(get_node(addon, "name").value, "Test add-on 13", "Name should be correct");
  is(name, "Test add-on 13", "Tooltip name should be correct");

  is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
  is_element_hidden(get_node(addon, "enable-btn"), "Enable button should be hidden");
  is_element_visible(get_node(addon, "disable-btn"), "Disable button should be visible");
  is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

  is_element_hidden(get_node(addon, "warning"), "Warning message should be hidden");
  is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
  is_element_hidden(get_node(addon, "error"), "Error message should be hidden");

  info("Filter for disabled unsigned extensions");
  let filterButton = gManagerWindow.document.getElementById("show-disabled-unsigned-extensions");
  let showAllButton = gManagerWindow.document.getElementById("show-all-extensions");
  let signingInfoUI = gManagerWindow.document.getElementById("disabled-unsigned-addons-info");
  is_element_visible(filterButton, "Button for showing disabled unsigned extensions should be visible");
  is_element_hidden(showAllButton, "Button for showing all extensions should be hidden");
  is_element_hidden(signingInfoUI, "Signing info UI should be hidden");

  filterButton.click();

  await new Promise(resolve => wait_for_view_load(gManagerWindow, resolve));

  is_element_hidden(filterButton, "Button for showing disabled unsigned extensions should be hidden");
  is_element_visible(showAllButton, "Button for showing all extensions should be visible");
  is_element_visible(signingInfoUI, "Signing info UI should be visible");

  items = get_test_items();
  is(Object.keys(items).length, 2, "Two add-ons should be shown");
  is(Object.keys(items)[0], "Test add-on 10", "The disabled unsigned extension should be shown");
  is(Object.keys(items)[1], "Test add-on 11", "The disabled unsigned extension should be shown");

  showAllButton.click();

  await new Promise(resolve => wait_for_view_load(gManagerWindow, resolve));

  items = get_test_items();
  is(Object.keys(items).length, EXPECTED_ADDONS, "All add-ons should be shown again");
  is_element_visible(filterButton, "Button for showing disabled unsigned extensions should be visible again");
  is_element_hidden(showAllButton, "Button for showing all extensions should be hidden again");
  is_element_hidden(signingInfoUI, "Signing info UI should be hidden again");

  Services.prefs.setBoolPref("xpinstall.signatures.required", false);
});

add_task(async function() {
  return close_manager(gManagerWindow);
});
