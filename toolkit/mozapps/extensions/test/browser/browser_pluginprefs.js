/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests the detail view of plugins

var gManagerWindow;

function test() {
  waitForExplicitFinish();

  open_manager("addons://list/plugin", function(aWindow) {
    gManagerWindow = aWindow;

    run_next_test();
  });
}

function end_test() {
  close_manager(gManagerWindow, function() {
    finish();
  });
}

add_test(function() {
  AddonManager.getAddonsByTypes(["plugin"], function(plugins) {
    let testPluginId;
    for (let plugin of plugins) {
      if (plugin.name == "Test Plug-in") {
        testPluginId = plugin.id;
        break;
      }
    }
    ok(testPluginId, "Test Plug-in should exist")

    AddonManager.getAddonByID(testPluginId, function(testPlugin) {
      let pluginEl = get_addon_element(gManagerWindow, testPluginId);
      is(pluginEl.mAddon.optionsType, AddonManager.OPTIONS_TYPE_INLINE, "Options should be inline type");
      pluginEl.parentNode.ensureElementIsVisible(pluginEl);

      let button = gManagerWindow.document.getAnonymousElementByAttribute(pluginEl, "anonid", "preferences-btn");
      is_element_visible(button, "Preferences button should be visible");

      button = gManagerWindow.document.getAnonymousElementByAttribute(pluginEl, "anonid", "details-btn");
      EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);

      wait_for_view_load(gManagerWindow, function() {
        let pluginLibraries = gManagerWindow.document.getElementById("pluginLibraries");
        ok(pluginLibraries, "Plugin file name row should be displayed");
        // the file name depends on the platform
        ok(pluginLibraries.textContent, testPlugin.pluginLibraries, "Plugin file name should be displayed");

        let pluginMimeTypes = gManagerWindow.document.getElementById("pluginMimeTypes");
        ok(pluginMimeTypes, "Plugin mime type row should be displayed");
        ok(pluginMimeTypes.textContent, "application/x-test (tst)", "Plugin mime type should be displayed");

        run_next_test();
      });
    });
  });
});
