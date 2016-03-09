/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that state menu is displayed correctly (enabled or disabled) in the add-on manager
// when the preference is unlocked / locked
var {classes: Cc, interfaces: Ci} = Components;
const gIsWindows = ("@mozilla.org/windows-registry-key;1" in Cc);
const gIsOSX = ("nsILocalFileMac" in Ci);
const gIsLinux = ("@mozilla.org/gnome-gconf-service;1" in Cc) ||
  ("@mozilla.org/gio-service;1" in Cc);

var gManagerWindow;
var gCategoryUtilities;
var gPluginElement;

function getTestPluginPref() {
  let prefix = "plugin.state.";
  if (gIsWindows)
    return prefix + "nptest";
  else if (gIsLinux)
    return prefix + "libnptest";
  else
    return prefix + "test";
}

registerCleanupFunction(() => {
  Services.prefs.unlockPref(getTestPluginPref());
  Services.prefs.clearUserPref(getTestPluginPref());
});

function getPlugins() {
  return new Promise(resolve => {
    AddonManager.getAddonsByTypes(["plugin"], plugins => resolve(plugins));
  });
}

function getTestPlugin(aPlugins) {
  let testPluginId;

  for (let plugin of aPlugins) {
    if (plugin.name == "Test Plug-in") {
      testPluginId = plugin.id;
      break;
    }
  }

  Assert.ok(testPluginId, "Test Plug-in should exist");

  let pluginElement = get_addon_element(gManagerWindow, testPluginId);
  pluginElement.parentNode.ensureElementIsVisible(pluginElement);

  return pluginElement;
}

function checkStateMenu(locked) {
  Assert.equal(Services.prefs.prefIsLocked(getTestPluginPref()), locked,
    "Preference lock state should be correct.");
  let menuList = gManagerWindow.document.getAnonymousElementByAttribute(gPluginElement, "anonid", "state-menulist");
  //  State menu should always have a selected item which must be visible
  let selectedMenuItem = menuList.querySelector(".addon-control[selected=\"true\"]");

  is_element_visible(menuList, "State menu should be visible.");
  Assert.equal(menuList.disabled, locked,
    "State menu should" + (locked === true ? "" : " not") + " be disabled.");

  is_element_visible(selectedMenuItem, "State menu's selected item should be visible.");
}

function checkStateMenuDetail(locked) {
  Assert.equal(Services.prefs.prefIsLocked(getTestPluginPref()), locked,
    "Preference should be " + (locked === true ? "" : "un") + "locked.");

  // open details menu
  let details = gManagerWindow.document.getAnonymousElementByAttribute(gPluginElement, "anonid", "details-btn");
  is_element_visible(details, "Details link should be visible.");
  EventUtils.synthesizeMouseAtCenter(details, {}, gManagerWindow);

  return new Promise(resolve => {
    wait_for_view_load(gManagerWindow, function() {
      let menuList = gManagerWindow.document.getElementById("detail-state-menulist");
      is_element_visible(menuList, "Details state menu should be visible.");
      Assert.equal(menuList.disabled, locked,
        "Details state menu enabled state should be correct.");
      resolve();
    });
  });
}

add_task(function* initializeState() {
  Services.prefs.setIntPref(getTestPluginPref(), Ci.nsIPluginTag.STATE_ENABLED);
  Services.prefs.unlockPref(getTestPluginPref());
  gManagerWindow = yield open_manager();
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);
  yield gCategoryUtilities.openType("plugin");

  let plugins = yield getPlugins();
  gPluginElement = getTestPlugin(plugins);
});

// Tests that plugin state menu is enabled if the preference is unlocked
add_task(function* taskCheckStateMenuIsEnabled() {
  checkStateMenu(false);
  yield checkStateMenuDetail(false);
});

// Lock the preference and then reload the plugin category
add_task(function* reinitializeState() {
  // lock the preference
  Services.prefs.lockPref(getTestPluginPref());
  yield gCategoryUtilities.openType("plugin");
  // Retrieve the test plugin element
  let plugins = yield getPlugins();
  gPluginElement = getTestPlugin(plugins);
});

// Tests that plugin state menu is disabled if the preference is locked
add_task(function* taskCheckStateMenuIsDisabled() {
  checkStateMenu(true);
  yield checkStateMenuDetail(true);
});

add_task(function* testCleanup() {
  yield close_manager(gManagerWindow);
});
