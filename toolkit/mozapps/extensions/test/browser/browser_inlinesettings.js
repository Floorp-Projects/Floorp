/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests various aspects of the details view

var gManagerWindow;
var gCategoryUtilities;
var gProvider;

function installAddon(aCallback) {
  AddonManager.getInstallForURL(TESTROOT + "addons/browser_inlinesettings1.xpi",
                                function(aInstall) {
    aInstall.addListener({
      onInstallEnded: function() {
        executeSoon(aCallback);
      }
    });
    aInstall.install();
  }, "application/x-xpinstall");
}

function test() {
  waitForExplicitFinish();

  gProvider = new MockProvider();

  gProvider.createAddons([{
    id: "inlinesettings2@tests.mozilla.org",
    name: "Inline Settings (Regular)",
    version: "1",
    optionsURL: CHROMEROOT + "options.xul",
    optionsType: AddonManager.OPTIONS_TYPE_INLINE
  },{
    id: "noninlinesettings@tests.mozilla.org",
    name: "Non-Inline Settings",
    version: "1",
    optionsURL: CHROMEROOT + "addon_prefs.xul"
  }]);

  installAddon(function () {
    open_manager("addons://list/extension", function(aWindow) {
      gManagerWindow = aWindow;
      gCategoryUtilities = new CategoryUtilities(gManagerWindow);

      run_next_test();
    });
  });
}

function end_test() {
  close_manager(gManagerWindow, function() {
    AddonManager.getAddonByID("inlinesettings1@tests.mozilla.org", function(aAddon) {
      aAddon.uninstall();
      finish();
    });
  });
}

// Addon with options.xul
add_test(function() {
  var addon = get_addon_element(gManagerWindow, "inlinesettings1@tests.mozilla.org");
  is(addon.mAddon.optionsType, AddonManager.OPTIONS_TYPE_INLINE, "Options should be inline type");
  addon.parentNode.ensureElementIsVisible(addon);

  var button = gManagerWindow.document.getAnonymousElementByAttribute(addon, "anonid", "preferences-btn");
  is_element_visible(button, "Preferences button should be visible");

  run_next_test();
});

// Addon with inline preferences as optionsURL
add_test(function() {
  var addon = get_addon_element(gManagerWindow, "inlinesettings2@tests.mozilla.org");
  is(addon.mAddon.optionsType, AddonManager.OPTIONS_TYPE_INLINE, "Options should be inline type");
  addon.parentNode.ensureElementIsVisible(addon);

  var button = gManagerWindow.document.getAnonymousElementByAttribute(addon, "anonid", "preferences-btn");
  is_element_visible(button, "Preferences button should be visible");

  run_next_test();
});

// Addon with non-inline preferences as optionsURL
add_test(function() {
  var addon = get_addon_element(gManagerWindow, "noninlinesettings@tests.mozilla.org");
  is(addon.mAddon.optionsType, AddonManager.OPTIONS_TYPE_DIALOG, "Options should be dialog type");
  addon.parentNode.ensureElementIsVisible(addon);

  var button = gManagerWindow.document.getAnonymousElementByAttribute(addon, "anonid", "preferences-btn");
  is_element_visible(button, "Preferences button should be visible");

  run_next_test();
});

// Addon with options.xul, also a test for the setting.xml bindings
add_test(function() {
  var addon = get_addon_element(gManagerWindow, "inlinesettings1@tests.mozilla.org");
  addon.parentNode.ensureElementIsVisible(addon);

  var button = gManagerWindow.document.getAnonymousElementByAttribute(addon, "anonid", "preferences-btn");
  EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);

  wait_for_view_load(gManagerWindow, function() {
    var grid = gManagerWindow.document.getElementById("detail-grid");
    var settings = grid.querySelectorAll("rows > setting");
    is(settings.length, 4, "Grid should have settings children");

    // Force bindings to apply
    settings[0].clientTop;

    Services.prefs.setBoolPref("extensions.inlinesettings1.bool", false);
    var input = gManagerWindow.document.getAnonymousElementByAttribute(settings[0], "anonid", "input");
    isnot(input.checked, true, "Checkbox should have initial value");
    EventUtils.synthesizeMouseAtCenter(input, { clickCount: 1 }, gManagerWindow);
    is(input.checked, true, "Checkbox should have updated value");
    is(Services.prefs.getBoolPref("extensions.inlinesettings1.bool"), true, "Bool pref should have been updated");
    EventUtils.synthesizeMouseAtCenter(input, { clickCount: 1 }, gManagerWindow);
    isnot(input.checked, true, "Checkbox should have updated value");
    is(Services.prefs.getBoolPref("extensions.inlinesettings1.bool"), false, "Bool pref should have been updated");

    Services.prefs.setIntPref("extensions.inlinesettings1.boolint", 0);
    var input = gManagerWindow.document.getAnonymousElementByAttribute(settings[1], "anonid", "input");
    isnot(input.checked, true, "Checkbox should have initial value");
    EventUtils.synthesizeMouseAtCenter(input, { clickCount: 1 }, gManagerWindow);
    is(input.checked, true, "Checkbox should have updated value");
    is(Services.prefs.getIntPref("extensions.inlinesettings1.boolint"), 1, "BoolInt pref should have been updated");
    EventUtils.synthesizeMouseAtCenter(input, { clickCount: 1 }, gManagerWindow);
    isnot(input.checked, true, "Checkbox should have updated value");
    is(Services.prefs.getIntPref("extensions.inlinesettings1.boolint"), 2, "BoolInt pref should have been updated");

    Services.prefs.setIntPref("extensions.inlinesettings1.integer", 0);
    var input = gManagerWindow.document.getAnonymousElementByAttribute(settings[2], "anonid", "input");
    is(input.value, "0", "Number box should have initial value");
    input.select();
    EventUtils.synthesizeKey("1", {}, gManagerWindow);
    EventUtils.synthesizeKey("3", {}, gManagerWindow);
    is(input.value, "13", "Number box should have updated value");
    is(Services.prefs.getIntPref("extensions.inlinesettings1.integer"), 13, "Integer pref should have been updated");
    EventUtils.synthesizeKey("VK_DOWN", {}, gManagerWindow);
    is(input.value, "12", "Number box should have updated value");
    is(Services.prefs.getIntPref("extensions.inlinesettings1.integer"), 12, "Integer pref should have been updated");

    Services.prefs.setCharPref("extensions.inlinesettings1.string", "foo");
    var input = gManagerWindow.document.getAnonymousElementByAttribute(settings[3], "anonid", "input");
    is(input.value, "foo", "Text box should have initial value");
    input.select();
    EventUtils.synthesizeKey("b", {}, gManagerWindow);
    EventUtils.synthesizeKey("a", {}, gManagerWindow);
    EventUtils.synthesizeKey("r", {}, gManagerWindow);
    is(input.value, "bar", "Text box should have updated value");
    is(Services.prefs.getCharPref("extensions.inlinesettings1.string"), "bar", "String pref should have been updated");

    var button = gManagerWindow.document.getElementById("detail-prefs-btn");
    is_element_hidden(button, "Preferences button should not be visible");

    gCategoryUtilities.openType("extension", run_next_test);
  });
});

// Addon with inline preferences as optionsURL
add_test(function() {
  var addon = get_addon_element(gManagerWindow, "inlinesettings2@tests.mozilla.org");
  addon.parentNode.ensureElementIsVisible(addon);

  var button = gManagerWindow.document.getAnonymousElementByAttribute(addon, "anonid", "preferences-btn");
  EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);

  wait_for_view_load(gManagerWindow, function() {
    var grid = gManagerWindow.document.getElementById("detail-grid");
    var settings = grid.querySelectorAll("rows > setting");
    is(settings.length, 2, "Grid should have settings children");

    // Force bindings to apply
    settings[0].clientTop;

    var node = settings[0];
    is(node.nodeName, "setting", "Should be a setting node");
    var description = gManagerWindow.document.getAnonymousElementByAttribute(node, "class", "preferences-description");
    is(description.textContent.trim(), "", "Description node should be empty");

    node = node.nextSibling;
    is(node.nodeName, "row", "Setting should be followed by a row node");
    is(node.textContent, "Description Attribute", "Description should be in this row");

    node = settings[1];
    is(node.nodeName, "setting", "Should be a setting node");
    description = gManagerWindow.document.getAnonymousElementByAttribute(node, "class", "preferences-description");
    is(description.textContent.trim(), "", "Description node should be empty");

    node = node.nextSibling;
    is(node.nodeName, "row", "Setting should be followed by a row node");
    is(node.textContent, "Description Text Node", "Description should be in this row");

    var button = gManagerWindow.document.getElementById("detail-prefs-btn");
    is_element_hidden(button, "Preferences button should not be visible");

    gCategoryUtilities.openType("extension", run_next_test);
  });
});

// Addon with non-inline preferences as optionsURL
add_test(function() {
  var addon = get_addon_element(gManagerWindow, "noninlinesettings@tests.mozilla.org");
  addon.parentNode.ensureElementIsVisible(addon);

  var button = gManagerWindow.document.getAnonymousElementByAttribute(addon, "anonid", "details-btn");
  EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);

  wait_for_view_load(gManagerWindow, function() {
    var grid = gManagerWindow.document.getElementById("detail-grid");
    var settings = grid.querySelectorAll("rows > setting");
    is(settings.length, 0, "Grid should not have settings children");

    var button = gManagerWindow.document.getElementById("detail-prefs-btn");
    is_element_visible(button, "Preferences button should be visible");

    gCategoryUtilities.openType("extension", run_next_test);
  });
});

// Addon with options.xul, disabling and enabling should hide and show settings UI
add_test(function() {
  var addon = get_addon_element(gManagerWindow, "inlinesettings1@tests.mozilla.org");
  addon.parentNode.ensureElementIsVisible(addon);

  var button = gManagerWindow.document.getAnonymousElementByAttribute(addon, "anonid", "preferences-btn");
  EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);

  wait_for_view_load(gManagerWindow, function() {
    var grid = gManagerWindow.document.getElementById("detail-grid");
    var settings = grid.querySelectorAll("rows > setting");
    is(settings.length, 4, "Grid should have settings children");

    // disable
    var button = gManagerWindow.document.getElementById("detail-disable-btn");
    EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);

    settings = grid.querySelectorAll("rows > setting");
    is(settings.length, 0, "Grid should not have settings children");

    gCategoryUtilities.openType("extension", function() {
      var addon = get_addon_element(gManagerWindow, "inlinesettings1@tests.mozilla.org");
      addon.parentNode.ensureElementIsVisible(addon);

      var button = gManagerWindow.document.getAnonymousElementByAttribute(addon, "anonid", "preferences-btn");
      is_element_hidden(button, "Preferences button should not be visible");

      button = gManagerWindow.document.getAnonymousElementByAttribute(addon, "anonid", "details-btn");
      EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);

      wait_for_view_load(gManagerWindow, function() {
        var grid = gManagerWindow.document.getElementById("detail-grid");
        var settings = grid.querySelectorAll("rows > setting");
        is(settings.length, 0, "Grid should not have settings children");

        // enable
        var button = gManagerWindow.document.getElementById("detail-enable-btn");
        EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);

        settings = grid.querySelectorAll("rows > setting");
        is(settings.length, 4, "Grid should have settings children");

        gCategoryUtilities.openType("extension", run_next_test);
      });
    });
  });
});
