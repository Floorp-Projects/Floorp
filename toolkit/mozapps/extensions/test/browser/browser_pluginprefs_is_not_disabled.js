/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests plugin prefs being enabled

function getTestPlugin(aPlugins) {
  let [testPlugin] = aPlugins.filter(plugin => plugin.name === "Test Plug-in");
  Assert.ok(testPlugin, "Test Plug-in should exist");
  return testPlugin;
}

add_task(async function taskCheckPluginPrefsEnabled() {
  const [gManagerWindow, plugins] = await Promise.all([open_manager(), AddonManager.getAddonsByTypes(["plugin"])]);
  const testPlugin = getTestPlugin(plugins);
  const testPluginTag = getTestPluginTag();
  Assert.ok(testPluginTag, "Test Plug-in tag should exist");
  const initialTestPluginState = testPluginTag.enabledState;

  Assert.ok(gManagerWindow.gViewController.commands.cmd_showItemPreferences.isEnabled(testPlugin), "Test Plug-in preferences should be enabled");
  testPluginTag.enabledState = Ci.nsIPluginTag.STATE_DISABLED;
  Assert.ok(gManagerWindow.gViewController.commands.cmd_showItemPreferences.isEnabled(testPlugin), "Test Plug-in preferences should be enabled");

  testPluginTag.enabledState = initialTestPluginState;
  await close_manager(gManagerWindow);
});
