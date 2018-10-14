/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests the detail view of plugins

var gManagerWindow;

async function test() {
  waitForExplicitFinish();

  let aWindow = await open_manager("addons://list/plugin");
  gManagerWindow = aWindow;

  run_next_test();
}

async function end_test() {
  await close_manager(gManagerWindow);
  finish();
}

add_test(async function() {
  let plugins = await AddonManager.getAddonsByTypes(["plugin"]);
  let testPluginId;
  for (let plugin of plugins) {
    if (plugin.name == "Test Plug-in") {
      testPluginId = plugin.id;
      break;
    }
  }
  ok(testPluginId, "Test Plug-in should exist");

  let testPlugin = await AddonManager.getAddonByID(testPluginId);
  let pluginEl = get_addon_element(gManagerWindow, testPluginId);
  is(pluginEl.mAddon.optionsType, AddonManager.OPTIONS_TYPE_INLINE_BROWSER, "Options should be inline type");
  pluginEl.parentNode.ensureElementIsVisible(pluginEl);

  let button = gManagerWindow.document.getAnonymousElementByAttribute(pluginEl, "anonid", "preferences-btn");
  is_element_visible(button, "Preferences button should be visible");

  button = gManagerWindow.document.getAnonymousElementByAttribute(pluginEl, "anonid", "details-btn");
  EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);

  Services.obs.addObserver(async function observer(subject, topic, data) {
    Services.obs.removeObserver(observer, topic);

    // Wait for PluginProvider to do its stuff.
    await new Promise(executeSoon);

    let doc = gManagerWindow.document.getElementById("addon-options").contentDocument;

    let pluginLibraries = doc.getElementById("pluginLibraries");
    ok(pluginLibraries, "Plugin file name row should be displayed");
    // the file name depends on the platform
    is(pluginLibraries.textContent, testPlugin.pluginLibraries, "Plugin file name should be displayed");

    let pluginMimeTypes = doc.getElementById("pluginMimeTypes");
    ok(pluginMimeTypes, "Plugin mime type row should be displayed");
    is(pluginMimeTypes.textContent, "application/x-test (Test \u2122 mimetype: tst)", "Plugin mime type should be displayed");

    run_next_test();
  }, AddonManager.OPTIONS_NOTIFICATION_DISPLAYED);
});
