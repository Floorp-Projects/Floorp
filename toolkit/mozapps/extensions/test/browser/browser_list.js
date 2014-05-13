/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests the list view

let tempScope = {};
Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm", tempScope);
let LightweightThemeManager = tempScope.LightweightThemeManager;


var gProvider;
var gManagerWindow;
var gCategoryUtilities;

var gApp = document.getElementById("bundle_brand").getString("brandShortName");
var gVersion = Services.appinfo.version;
var gBlocklistURL = Services.urlFormatter.formatURLPref("extensions.blocklist.detailsURL");
var gPluginURL = Services.urlFormatter.formatURLPref("plugins.update.url");
var gDate = new Date(2010, 7, 16);

var gLWTheme = {
                id: "4",
                version: "1",
                name: "Bling",
                description: "SO MUCH BLING!",
                author: "Pixel Pusher",
                homepageURL: "http://mochi.test:8888/data/index.html",
                headerURL: "http://mochi.test:8888/data/header.png",
                footerURL: "http://mochi.test:8888/data/footer.png",
                previewURL: "http://mochi.test:8888/data/preview.png",
                iconURL: "http://mochi.test:8888/data/icon.png"
              };


function test() {
  waitForExplicitFinish();

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
    id: "addon4@tests.mozilla.org",
    blocklistURL: "http://example.com/addon4@tests.mozilla.org",
    name: "Test add-on 4",
    _userDisabled: true,
    isActive: false,
    blocklistState: Ci.nsIBlocklistService.STATE_SOFTBLOCKED
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
    id: "addon7@tests.mozilla.org",
    blocklistURL: "http://example.com/addon7@tests.mozilla.org",
    name: "Test add-on 7",
    blocklistState: Ci.nsIBlocklistService.STATE_OUTDATED,
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
  }]);

  open_manager(null, function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);
    run_next_test();
  });
}

function end_test() {
  close_manager(gManagerWindow, function() {
    finish();
  });
}

function get_test_items() {
  var tests = "@tests.mozilla.org";

  var items = {};
  var item = gManagerWindow.document.getElementById("addon-list").firstChild;

  while (item) {
    if (item.mAddon.id.substring(item.mAddon.id.length - tests.length) == tests)
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
add_test(function() {
  gCategoryUtilities.openType("extension", function() {
    let items = get_test_items();
    is(Object.keys(items).length, 9, "Should be nine add-ons installed");

    info("Addon 1");
    let addon = items["Test add-on"];
    addon.parentNode.ensureElementIsVisible(addon);
    is(get_node(addon, "name").value, "Test add-on", "Name should be correct");
    is_element_visible(get_node(addon, "version"), "Version should be visible");
    is(get_node(addon, "version").value, "1.0", "Version should be correct");
    is_element_visible(get_node(addon, "description"), "Description should be visible");
    is(get_node(addon, "description").value, "A test add-on", "Description should be correct");
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

    info("Disabling");
    EventUtils.synthesizeMouseAtCenter(get_node(addon, "disable-btn"), {}, gManagerWindow);
    is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
    is_element_visible(get_node(addon, "enable-btn"), "Enable button should be visible");
    is_element_hidden(get_node(addon, "disable-btn"), "Disable button should be hidden");
    is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

    is_element_hidden(get_node(addon, "warning"), "Warning message should be visible");
    is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
    is_element_hidden(get_node(addon, "error"), "Error message should be hidden");
    is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");
    is_element_visible(get_node(addon, "pending"), "Pending message should be visible");
    is(get_node(addon, "pending").textContent, "Test add-on will be disabled after you restart " + gApp + ".", "Pending message should be correct");

    info("Addon 2");
    addon = items["Test add-on 2"];
    addon.parentNode.ensureElementIsVisible(addon);
    is(get_node(addon, "name").value, "Test add-on 2", "Name should be correct");
    is_element_visible(get_node(addon, "version"), "Version should be visible");
    is(get_node(addon, "version").value, "2.0", "Version should be correct");
    is_element_hidden(get_node(addon, "description"), "Description should be hidden");
    is_element_visible(get_class_node(addon, "disabled-postfix"), "Disabled postfix should be visible");
    is_element_hidden(get_class_node(addon, "update-postfix"), "Update postfix should be hidden");
    is(get_node(addon, "date-updated").value, "Unknown", "Date should be correct");

    is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
    is_element_visible(get_node(addon, "enable-btn"), "Enable button should be visible");
    is_element_hidden(get_node(addon, "disable-btn"), "Disable button should be hidden");
    is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

    is_element_hidden(get_node(addon, "warning"), "Warning message should be hidden");
    is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
    is_element_hidden(get_node(addon, "error"), "Error message should be hidden");
    is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");
    is_element_hidden(get_node(addon, "pending"), "Pending message should be hidden");

    info("Enabling");
    EventUtils.synthesizeMouseAtCenter(get_node(addon, "enable-btn"), {}, gManagerWindow);
    is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
    is_element_hidden(get_node(addon, "enable-btn"), "Enable button should be hidden");
    is_element_visible(get_node(addon, "disable-btn"), "Disable button should be visible");
    is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

    is_element_hidden(get_node(addon, "warning"), "Warning message should be hidden");
    is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
    is_element_hidden(get_node(addon, "error"), "Error message should be hidden");
    is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");
    is_element_visible(get_node(addon, "pending"), "Pending message should be visible");
    is(get_node(addon, "pending").textContent, "Test add-on 2 will be enabled after you restart " + gApp + ".", "Pending message should be correct");

    info("Addon 3");
    addon = items["Test add-on 3"];
    addon.parentNode.ensureElementIsVisible(addon);
    is(get_node(addon, "name").value, "Test add-on 3", "Name should be correct");
    is_element_hidden(get_node(addon, "version"), "Version should be hidden");

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

    info("Addon 4");
    addon = items["Test add-on 4"];
    addon.parentNode.ensureElementIsVisible(addon);
    is(get_node(addon, "name").value, "Test add-on 4", "Name should be correct");

    is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
    is_element_visible(get_node(addon, "enable-btn"), "Enable button should be visible");
    is_element_hidden(get_node(addon, "disable-btn"), "Disable button should be hidden");
    is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

    is_element_visible(get_node(addon, "warning"), "Warning message should be visible");
    is(get_node(addon, "warning").textContent, "Test add-on 4 is known to cause security or stability issues.", "Warning message should be correct");
    is_element_visible(get_node(addon, "warning-link"), "Warning link should be visible");
    is(get_node(addon, "warning-link").value, "More Information", "Warning link text should be correct");
    is(get_node(addon, "warning-link").href, "http://example.com/addon4@tests.mozilla.org", "Warning link should be correct");
    is_element_hidden(get_node(addon, "error"), "Error message should be hidden");
    is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");
    is_element_hidden(get_node(addon, "pending"), "Pending message should be hidden");

    info("Enabling");
    EventUtils.synthesizeMouseAtCenter(get_node(addon, "enable-btn"), {}, gManagerWindow);
    is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
    is_element_hidden(get_node(addon, "enable-btn"), "Enable button should be hidden");
    is_element_visible(get_node(addon, "disable-btn"), "Disable button should be visible");
    is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

    is_element_hidden(get_node(addon, "warning"), "Warning message should be hidden");
    is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
    is_element_hidden(get_node(addon, "error"), "Error message should be hidden");
    is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");
    is_element_visible(get_node(addon, "pending"), "Pending message should be visible");
    is(get_node(addon, "pending").textContent, "Test add-on 4 will be enabled after you restart " + gApp + ".", "Pending message should be correct");

    info("Addon 5");
    addon = items["Test add-on 5"];
    addon.parentNode.ensureElementIsVisible(addon);
    is(get_node(addon, "name").value, "Test add-on 5", "Name should be correct");

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
    is(get_node(addon, "name").value, "Test add-on 6", "Name should be correct");
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

    info("Addon 7");
    addon = items["Test add-on 7"];
    addon.parentNode.ensureElementIsVisible(addon);
    is(get_node(addon, "name").value, "Test add-on 7", "Name should be correct");

    is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
    is_element_hidden(get_node(addon, "enable-btn"), "Enable button should be hidden");
    is_element_visible(get_node(addon, "disable-btn"), "Disable button should be visible");
    is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

    is_element_visible(get_node(addon, "warning"), "Warning message should be hidden");
    is(get_node(addon, "warning").textContent, "An important update is available for Test add-on 7.", "Warning message should be correct");
    is_element_visible(get_node(addon, "warning-link"), "Warning link should be visible");
    is(get_node(addon, "warning-link").value, "Update Now", "Warning link text should be correct");
    is(get_node(addon, "warning-link").href, gPluginURL, "Warning link should be correct");
    is_element_hidden(get_node(addon, "error"), "Error message should be hidden");
    is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");
    is_element_hidden(get_node(addon, "pending"), "Pending message should be hidden");

    info("Disabling");
    EventUtils.synthesizeMouseAtCenter(get_node(addon, "disable-btn"), {}, gManagerWindow);
    is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
    is_element_visible(get_node(addon, "enable-btn"), "Enable button should be visible");
    is_element_hidden(get_node(addon, "disable-btn"), "Disable button should be hidden");
    is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

    is_element_hidden(get_node(addon, "warning"), "Warning message should be visible");
    is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
    is_element_hidden(get_node(addon, "error"), "Error message should be hidden");
    is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");
    is_element_visible(get_node(addon, "pending"), "Pending message should be visible");
    is(get_node(addon, "pending").textContent, "Test add-on 7 will be disabled after you restart " + gApp + ".", "Pending message should be correct");

    info("Addon 8");
    addon = items["Test add-on 8"];
    addon.parentNode.ensureElementIsVisible(addon);
    is(get_node(addon, "name").value, "Test add-on 8", "Name should be correct");

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
    is(get_node(addon, "name").value, "Test add-on 9", "Name should be correct");

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

    run_next_test();
  });
});

// Check the add-ons are now in the right state
add_test(function() {
  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon6@tests.mozilla.org"],
                               function([a1, a2, a4, a6]) {
    is(a1.pendingOperations, AddonManager.PENDING_DISABLE, "Add-on 1 should be pending disable");
    is(a2.pendingOperations, AddonManager.PENDING_ENABLE, "Add-on 2 should be pending enable");
    is(a4.pendingOperations, AddonManager.PENDING_ENABLE, "Add-on 4 should be pending enable");

    run_next_test();
  });
});

// Reload the list to make sure the changes are still pending and that undoing
// works
add_test(function() {
  gCategoryUtilities.openType("plugin", function() {
    gCategoryUtilities.openType("extension", function() {
      let items = get_test_items();
      is(Object.keys(items).length, 9, "Should be nine add-ons installed");

      info("Addon 1");
      let addon = items["Test add-on"];
      addon.parentNode.ensureElementIsVisible(addon);
      is(get_node(addon, "name").value, "Test add-on", "Name should be correct");
      is_element_visible(get_node(addon, "version"), "Version should be visible");
      is(get_node(addon, "version").value, "1.0", "Version should be correct");
      is_element_visible(get_node(addon, "description"), "Description should be visible");
      is(get_node(addon, "description").value, "A test add-on", "Description should be correct");
      is_element_hidden(get_class_node(addon, "disabled-postfix"), "Disabled postfix should be hidden");
      is_element_hidden(get_class_node(addon, "update-postfix"), "Update postfix should be hidden");
      is(get_node(addon, "date-updated").value, formatDate(gDate), "Update date should be correct");

      is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
      is_element_visible(get_node(addon, "enable-btn"), "Enable button should be visible");
      is_element_hidden(get_node(addon, "disable-btn"), "Disable button should be hidden");
      is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

      is_element_hidden(get_node(addon, "warning"), "Warning message should be hidden");
      is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
      is_element_hidden(get_node(addon, "error"), "Error message should be hidden");
      is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");
      is_element_visible(get_node(addon, "pending"), "Pending message should be visible");
      is(get_node(addon, "pending").textContent, "Test add-on will be disabled after you restart " + gApp + ".", "Pending message should be correct");

      info("Undoing");
      EventUtils.synthesizeMouseAtCenter(get_node(addon, "undo-btn"), {}, gManagerWindow);
      is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
      is_element_hidden(get_node(addon, "enable-btn"), "Enable button should be hidden");
      is_element_visible(get_node(addon, "disable-btn"), "Disable button should be visible");
      is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

      is_element_hidden(get_node(addon, "warning"), "Warning message should be hidden");
      is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
      is_element_hidden(get_node(addon, "error"), "Error message should be hidden");
      is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");
      is_element_hidden(get_node(addon, "pending"), "Pending message should be hidden");

      info("Addon 2");
      addon = items["Test add-on 2"];
      addon.parentNode.ensureElementIsVisible(addon);
      is(get_node(addon, "name").value, "Test add-on 2", "Name should be correct");
      is_element_visible(get_node(addon, "version"), "Version should be visible");
      is(get_node(addon, "version").value, "2.0", "Version should be correct");
      is_element_hidden(get_node(addon, "description"), "Description should be hidden");
      is_element_visible(get_class_node(addon, "disabled-postfix"), "Disabled postfix should be visible");
      is_element_hidden(get_class_node(addon, "update-postfix"), "Update postfix should be hidden");
      is(get_node(addon, "date-updated").value, "Unknown", "Date should be correct");

      is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
      is_element_hidden(get_node(addon, "enable-btn"), "Enable button should be hidden");
      is_element_visible(get_node(addon, "disable-btn"), "Disable button should be visible");
      is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

      is_element_hidden(get_node(addon, "warning"), "Warning message should be hidden");
      is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
      is_element_hidden(get_node(addon, "error"), "Error message should be hidden");
      is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");
      is_element_visible(get_node(addon, "pending"), "Pending message should be visible");
      is(get_node(addon, "pending").textContent, "Test add-on 2 will be enabled after you restart " + gApp + ".", "Pending message should be correct");

      info("Undoing");
      EventUtils.synthesizeMouseAtCenter(get_node(addon, "undo-btn"), {}, gManagerWindow);
      is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
      is_element_visible(get_node(addon, "enable-btn"), "Enable button should be visible");
      is_element_hidden(get_node(addon, "disable-btn"), "Disable button should be hidden");
      is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

      is_element_hidden(get_node(addon, "warning"), "Warning message should be hidden");
      is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
      is_element_hidden(get_node(addon, "error"), "Error message should be hidden");
      is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");
      is_element_hidden(get_node(addon, "pending"), "Pending message should be hidden");

      info("Addon 4");
      addon = items["Test add-on 4"];
      addon.parentNode.ensureElementIsVisible(addon);
      is(get_node(addon, "name").value, "Test add-on 4", "Name should be correct");

      is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
      is_element_hidden(get_node(addon, "enable-btn"), "Enable button should be hidden");
      is_element_visible(get_node(addon, "disable-btn"), "Disable button should be visible");
      is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

      is_element_hidden(get_node(addon, "warning"), "Warning message should be hidden");
      is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
      is_element_hidden(get_node(addon, "error"), "Error message should be hidden");
      is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");
      is_element_visible(get_node(addon, "pending"), "Pending message should be visible");
      is(get_node(addon, "pending").textContent, "Test add-on 4 will be enabled after you restart " + gApp + ".", "Pending message should be correct");

      info("Undoing");
      EventUtils.synthesizeMouseAtCenter(get_node(addon, "undo-btn"), {}, gManagerWindow);
      is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
      is_element_visible(get_node(addon, "enable-btn"), "Enable button should be visible");
      is_element_hidden(get_node(addon, "disable-btn"), "Disable button should be hidden");
      is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

      is_element_visible(get_node(addon, "warning"), "Warning message should be visible");
      is(get_node(addon, "warning").textContent, "Test add-on 4 is known to cause security or stability issues.", "Warning message should be correct");
      is_element_visible(get_node(addon, "warning-link"), "Warning link should be visible");
      is(get_node(addon, "warning-link").value, "More Information", "Warning link text should be correct");
      is(get_node(addon, "warning-link").href, "http://example.com/addon4@tests.mozilla.org", "Warning link should be correct");
      is_element_hidden(get_node(addon, "error"), "Error message should be hidden");
      is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");
      is_element_hidden(get_node(addon, "pending"), "Pending message should be hidden");

      info("Addon 6");
      addon = items["Test add-on 6"];
      addon.parentNode.ensureElementIsVisible(addon);
      is(get_node(addon, "name").value, "Test add-on 6", "Name should be correct");
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

      info("Addon 7");
      addon = items["Test add-on 7"];
      addon.parentNode.ensureElementIsVisible(addon);
      is(get_node(addon, "name").value, "Test add-on 7", "Name should be correct");

      is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
      is_element_visible(get_node(addon, "enable-btn"), "Enable button should be visible");
      is_element_hidden(get_node(addon, "disable-btn"), "Disable button should be hidden");
      is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

      is_element_hidden(get_node(addon, "warning"), "Warning message should be visible");
      is_element_hidden(get_node(addon, "warning-link"), "Warning link should be hidden");
      is_element_hidden(get_node(addon, "error"), "Error message should be hidden");
      is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");
      is_element_visible(get_node(addon, "pending"), "Pending message should be visible");
      is(get_node(addon, "pending").textContent, "Test add-on 7 will be disabled after you restart " + gApp + ".", "Pending message should be correct");

      info("Undoing");
      EventUtils.synthesizeMouseAtCenter(get_node(addon, "undo-btn"), {}, gManagerWindow);
      is_element_hidden(get_node(addon, "preferences-btn"), "Preferences button should be hidden");
      is_element_hidden(get_node(addon, "enable-btn"), "Enable button should be hidden");
      is_element_visible(get_node(addon, "disable-btn"), "Disable button should be visible");
      is_element_visible(get_node(addon, "remove-btn"), "Remove button should be visible");

      is_element_visible(get_node(addon, "warning"), "Warning message should be hidden");
      is(get_node(addon, "warning").textContent, "An important update is available for Test add-on 7.", "Warning message should be correct");
      is_element_visible(get_node(addon, "warning-link"), "Warning link should be visible");
      is(get_node(addon, "warning-link").value, "Update Now", "Warning link text should be correct");
      is(get_node(addon, "warning-link").href, gPluginURL, "Warning link should be correct");
      is_element_hidden(get_node(addon, "error"), "Error message should be hidden");
      is_element_hidden(get_node(addon, "error-link"), "Error link should be hidden");
      is_element_hidden(get_node(addon, "pending"), "Pending message should be hidden");

      run_next_test();
    });
  });
});

// Check the add-ons are now in the right state
add_test(function() {
  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon4@tests.mozilla.org"],
                               function([a1, a2, a4]) {
    is(a1.pendingOperations, 0, "Add-on 1 should not have any pending operations");
    is(a2.pendingOperations, 0, "Add-on 1 should not have any pending operations");
    is(a4.pendingOperations, 0, "Add-on 1 should not have any pending operations");

    run_next_test();
  });
});

// Check that upgrades with onExternalInstall take effect immediately
add_test(function() {
  gProvider.createAddons([{
    id: "addon1@tests.mozilla.org",
    name: "Test add-on replacement",
    version: "2.0",
    description: "A test add-on with a new description",
    updateDate: gDate,
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE
  }]);

  let items = get_test_items();
  is(Object.keys(items).length, 9, "Should be nine add-ons installed");

  let addon = items["Test add-on replacement"];
  addon.parentNode.ensureElementIsVisible(addon);
  is(get_node(addon, "name").value, "Test add-on replacement", "Name should be correct");
  is_element_visible(get_node(addon, "version"), "Version should be visible");
  is(get_node(addon, "version").value, "2.0", "Version should be correct");
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

  run_next_test();
});

// Check that focus changes correctly move around the selected list item
add_test(function() {
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

  var fm = Cc["@mozilla.org/focus-manager;1"].
           getService(Ci.nsIFocusManager);

  let addon = items["Test add-on 6"];
  EventUtils.synthesizeMouseAtCenter(addon, { }, gManagerWindow);
  is(fm.focusedElement, addon.parentNode, "Focus should have moved to the list");

  EventUtils.synthesizeKey("VK_TAB", { }, gManagerWindow);
  is(fm.focusedElement, get_node(addon, "details-btn"), "Focus should have moved to the more button");

  EventUtils.synthesizeKey("VK_TAB", { }, gManagerWindow);
  is(fm.focusedElement, get_node(addon, "disable-btn"), "Focus should have moved to the disable button");

  EventUtils.synthesizeKey("VK_TAB", { }, gManagerWindow);
  is(fm.focusedElement, get_node(addon, "remove-btn"), "Focus should have moved to the remove button");

  EventUtils.synthesizeKey("VK_TAB", { }, gManagerWindow);
  ok(!is_node_in_list(fm.focusedElement), "Focus should be outside the list");

  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true }, gManagerWindow);
  is(fm.focusedElement, get_node(addon, "remove-btn"), "Focus should have moved to the remove button");

  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true }, gManagerWindow);
  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true }, gManagerWindow);
  is(fm.focusedElement, get_node(addon, "details-btn"), "Focus should have moved to the more button");

  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true }, gManagerWindow);
  is(fm.focusedElement, addon.parentNode, "Focus should have moved to the list");

  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true }, gManagerWindow);
  ok(!is_node_in_list(fm.focusedElement), "Focus should be outside the list");

  try {
    Services.prefs.clearUserPref("accessibility.tabfocus_applies_to_xul");
  }
  catch (e) { }

  run_next_test();
});


add_test(function() {
  info("Enabling lightweight theme");
  LightweightThemeManager.currentTheme = gLWTheme;
  
  gManagerWindow.loadView("addons://list/theme");
  wait_for_view_load(gManagerWindow, function() {
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

    AddonManager.getAddonByID("4@personas.mozilla.org", function(aAddon) {
      aAddon.uninstall();
      run_next_test();
    });
  });
});

// Check that onPropertyChanges for appDisabled updates the UI
add_test(function() {
  info("Checking that onPropertyChanges for appDisabled updates the UI");

  AddonManager.getAddonByID("addon2@tests.mozilla.org", function(aAddon) {
    aAddon.userDisabled = true;
    aAddon.isCompatible = true;
    aAddon.appDisabled = false;

    gManagerWindow.loadView("addons://list/extension");
    wait_for_view_load(gManagerWindow, function() {
      var el = get_addon_element(gManagerWindow, "addon2@tests.mozilla.org");

      is(el.getAttribute("active"), "false", "Addon should not be marked as active");
      is_element_hidden(get_node(el, "warning"), "Warning message should not be visible");

      info("Making addon incompatible and appDisabled");
      aAddon.isCompatible = false;
      aAddon.appDisabled = true;

      is(el.getAttribute("active"), "false", "Addon should not be marked as active");
      is_element_visible(get_node(el, "warning"), "Warning message should be visible");
      is(get_node(el, "warning").textContent, "Test add-on 2 is incompatible with " + gApp + " " + gVersion + ".", "Warning message should be correct");

      run_next_test();
    });
  });
});
