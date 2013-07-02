/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests various aspects of the details view

var gManagerWindow;
var gCategoryUtilities;

function installAddon(aCallback) {
  AddonManager.getInstallForURL(TESTROOT + "addons/browser_inlinesettings1_custom.xpi",
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

// Addon with options.xul, with custom <setting> binding
add_test(function() {
  var addon = get_addon_element(gManagerWindow, "inlinesettings1@tests.mozilla.org");
  is(addon.mAddon.optionsType, AddonManager.OPTIONS_TYPE_INLINE, "Options should be inline type");
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
    is(gManagerWindow.gViewController.currentViewId,
       "addons://detail/inlinesettings1%40tests.mozilla.org/preferences",
       "Current view should scroll to preferences");

    var grid = gManagerWindow.document.getElementById("detail-grid");
    var settings = grid.querySelectorAll("rows > setting");
    is(settings.length, 1, "Grid should have settings children");

    ok(settings[0].hasAttribute("first-row"), "First visible row should have first-row attribute");

    var style = window.getComputedStyle(settings[0], null);
    is(style.getPropertyValue("background-color"), "rgb(0, 0, 255)", "Background color should be set");
    is(style.getPropertyValue("display"), "-moz-grid-line", "Display should be set");
    is(style.getPropertyValue("-moz-binding"), 'url("chrome://inlinesettings/content/binding.xml#custom")', "Binding should be set");

    var label = gManagerWindow.document.getAnonymousElementByAttribute(settings[0], "anonid", "label");
    is(label.textContent, "Custom", "Localized string should be shown");

    var input = gManagerWindow.document.getAnonymousElementByAttribute(settings[0], "anonid", "input");
    isnot(input, null, "Binding should be applied");
    is(input.value, "Woah!", "Binding should be applied");

    button = gManagerWindow.document.getElementById("detail-prefs-btn");
    is_element_hidden(button, "Preferences button should not be visible");

    gCategoryUtilities.openType("extension", run_next_test);
  });
});
