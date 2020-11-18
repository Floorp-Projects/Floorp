/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests the detail view of plugins

var gManagerWindow;

async function getTestPluginAddon() {
  const plugins = await AddonManager.getAddonsByTypes(["plugin"]);
  return plugins.find(plugin => plugin.name === "Test Plug-in");
}

add_task(async function test_inline_plugin_prefs() {
  gManagerWindow = await open_manager("addons://list/plugin");
  let testPlugin = await getTestPluginAddon();
  ok(testPlugin, "Test Plug-in should exist");

  let pluginEl = get_addon_element(gManagerWindow, testPlugin.id);
  is(
    testPlugin.optionsType,
    AddonManager.OPTIONS_TYPE_INLINE_BROWSER,
    "Options should be inline type"
  );

  let optionsBrowserPromise = BrowserTestUtils.waitForEvent(
    pluginEl.ownerDocument,
    "load",
    true,
    event => {
      let { target } = event;
      return (
        target.currentURI && target.currentURI.spec === testPlugin.optionsURL
      );
    }
  ).then(event => event.target);

  pluginEl.querySelector("panel-item[action='preferences']").click();

  info("Waiting for inline options page to be ready");
  let doc = (await optionsBrowserPromise).contentDocument;

  await BrowserTestUtils.waitForCondition(() => {
    // Wait until pluginPrefs.js has initialized the plugin pref view.
    return doc.getElementById("pluginLibraries").textContent;
  }, "Waiting for pluginPrefs to finish rendering");

  let pluginLibraries = doc.getElementById("pluginLibraries");
  ok(pluginLibraries, "Plugin file name row should be displayed");
  // the file name depends on the platform
  is(
    pluginLibraries.textContent,
    testPlugin.pluginLibraries[0],
    "Plugin file name should be displayed"
  );

  let pluginMimeTypes = doc.getElementById("pluginMimeTypes");
  ok(pluginMimeTypes, "Plugin mime type row should be displayed");
  is(
    pluginMimeTypes.textContent,
    "application/x-test (Test \u2122 mimetype: tst)",
    "Plugin mime type should be displayed"
  );

  await close_manager(gManagerWindow);
});
