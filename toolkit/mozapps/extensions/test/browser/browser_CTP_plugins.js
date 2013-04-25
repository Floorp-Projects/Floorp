/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const gHttpTestRoot = "http://127.0.0.1:8888/" + RELATIVE_DIR + "/";
let gManagerWindow;
let gTestPluginId;
let gPluginBrowser;

function test() {
  waitForExplicitFinish();
  Services.prefs.setBoolPref("plugins.click_to_play", true);
  let pluginTag = getTestPluginTag();
  pluginTag.enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;
  open_manager("addons://list/plugin", part1);
}

function part1(aWindow) {
  gManagerWindow = aWindow;
  AddonManager.getAddonsByTypes(["plugin"], part2);
}

function part2(aPlugins) {
  for (let plugin of aPlugins) {
    if (plugin.name == "Test Plug-in") {
      gTestPluginId = plugin.id;
      break;
    }
  }
  ok(gTestPluginId, "part2: Test Plug-in should exist");
  AddonManager.getAddonByID(gTestPluginId, part3);
}

function part3(aTestPlugin) {
  let pluginEl = get_addon_element(gManagerWindow, gTestPluginId);
  pluginEl.parentNode.ensureElementIsVisible(pluginEl);
  let enableButton = gManagerWindow.document.getAnonymousElementByAttribute(pluginEl, "anonid", "enable-btn");
  is_element_hidden(enableButton, "part3: enable button should not be visible");
  let disableButton = gManagerWindow.document.getAnonymousElementByAttribute(pluginEl, "anonid", "enable-btn");
  is_element_hidden(disableButton, "part3: disable button should not be visible");
  let menu = gManagerWindow.document.getAnonymousElementByAttribute(pluginEl, "anonid", "tristate-menulist");
  is_element_visible(menu, "part3: tristate menu should be visible");
  let askToActivateItem = gManagerWindow.document.getAnonymousElementByAttribute(pluginEl, "anonid", "ask-to-activate-menuitem");
  is(menu.selectedItem, askToActivateItem, "part3: tristate menu should have 'Ask To Activate' selected");

  gBrowser.selectedTab = gBrowser.addTab();
  gPluginBrowser = gBrowser.selectedBrowser;
  gPluginBrowser.addEventListener("PluginBindingAttached", part4, true, true);
  gPluginBrowser.contentWindow.location = gHttpTestRoot + "plugin_test.html";
}

function part4() {
  ok(PopupNotifications.getNotification("click-to-play-plugins", gPluginBrowser), "part4: should have a click-to-play notification");
  gPluginBrowser.removeEventListener("PluginBindingAttached", part4);
  gBrowser.removeCurrentTab();

  let pluginEl = get_addon_element(gManagerWindow, gTestPluginId);
  let menu = gManagerWindow.document.getAnonymousElementByAttribute(pluginEl, "anonid", "tristate-menulist");
  let alwaysActivateItem = gManagerWindow.document.getAnonymousElementByAttribute(pluginEl, "anonid", "always-activate-menuitem");
  menu.selectedItem = alwaysActivateItem;
  alwaysActivateItem.doCommand();
  gBrowser.selectedTab = gBrowser.addTab();
  gPluginBrowser = gBrowser.selectedBrowser;
  gPluginBrowser.addEventListener("load", part5, true);
  gPluginBrowser.contentWindow.location = gHttpTestRoot + "plugin_test.html";
}

function part5() {
  let testPlugin = gPluginBrowser.contentDocument.getElementById("test");
  ok(testPlugin, "part5: should have a plugin element in the page");
  let objLoadingContent = testPlugin.QueryInterface(Ci.nsIObjectLoadingContent);
  let condition = function() objLoadingContent.activated;
  waitForCondition(condition, part6, "part5: waited too long for plugin to activate");
}

function part6() {
  let testPlugin = gPluginBrowser.contentDocument.getElementById("test");
  ok(testPlugin, "part6: should have a plugin element in the page");
  let objLoadingContent = testPlugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "part6: plugin should be activated");
  gPluginBrowser.removeEventListener("load", part5);
  gBrowser.removeCurrentTab();

  let pluginEl = get_addon_element(gManagerWindow, gTestPluginId);
  let menu = gManagerWindow.document.getAnonymousElementByAttribute(pluginEl, "anonid", "tristate-menulist");
  let neverActivateItem = gManagerWindow.document.getAnonymousElementByAttribute(pluginEl, "anonid", "never-activate-menuitem");
  menu.selectedItem = neverActivateItem;
  neverActivateItem.doCommand();
  gBrowser.selectedTab = gBrowser.addTab();
  gPluginBrowser = gBrowser.selectedBrowser;
  gPluginBrowser.addEventListener("PluginBindingAttached", part7, true, true);
  gPluginBrowser.contentWindow.location = gHttpTestRoot + "plugin_test.html";
}

function part7() {
  ok(!PopupNotifications.getNotification("click-to-play-plugins", gPluginBrowser), "part7: should not have a click-to-play notification");
  let testPlugin = gPluginBrowser.contentDocument.getElementById("test");
  ok(testPlugin, "part7: should have a plugin element in the page");
  let objLoadingContent = testPlugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "part7: plugin should not be activated");

  gPluginBrowser.removeEventListener("PluginBindingAttached", part7);
  gBrowser.removeCurrentTab();

  let pluginEl = get_addon_element(gManagerWindow, gTestPluginId);
  let details = gManagerWindow.document.getAnonymousElementByAttribute(pluginEl, "anonid", "details-btn");
  is_element_visible(details, "part7: details link should be visible");
  EventUtils.synthesizeMouseAtCenter(details, {}, gManagerWindow);
  wait_for_view_load(gManagerWindow, part8);
}

function part8() {
  let enableButton = gManagerWindow.document.getElementById("detail-enable-btn");
  is_element_hidden(enableButton, "part8: detail enable button should be hidden");
  let disableButton = gManagerWindow.document.getElementById("detail-disable-btn");
  is_element_hidden(disableButton, "part8: detail disable button should be hidden");
  let menu = gManagerWindow.document.getElementById("detail-tristate-menulist");
  is_element_visible(menu, "part8: detail tristate menu should be visible");
  let neverActivateItem = gManagerWindow.document.getElementById("detail-never-activate-menuitem")
  is(menu.selectedItem, neverActivateItem, "part8: tristate menu should have 'Never Activate' selected");

  let alwaysActivateItem = gManagerWindow.document.getElementById("detail-always-activate-menuitem");
  menu.selectedItem = alwaysActivateItem;
  alwaysActivateItem.doCommand();
  gBrowser.selectedTab = gBrowser.addTab();
  gPluginBrowser = gBrowser.selectedBrowser;
  gPluginBrowser.addEventListener("load", part9, true);
  gPluginBrowser.contentWindow.location = gHttpTestRoot + "plugin_test.html";
}

function part9() {
  let testPlugin = gPluginBrowser.contentDocument.getElementById("test");
  ok(testPlugin, "part9: should have a plugin element in the page");
  let objLoadingContent = testPlugin.QueryInterface(Ci.nsIObjectLoadingContent);
  let condition = function() objLoadingContent.activated;
  waitForCondition(condition, part10, "part9: waited too long for plugin to activate");
}

function part10() {
  let testPlugin = gPluginBrowser.contentDocument.getElementById("test");
  ok(testPlugin, "part10: should have a plugin element in the page");
  let objLoadingContent = testPlugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "part10: plugin should be activated");
  gPluginBrowser.removeEventListener("load", part9);
  gBrowser.removeCurrentTab();

  let menu = gManagerWindow.document.getElementById("detail-tristate-menulist");
  let askToActivateItem = gManagerWindow.document.getElementById("detail-ask-to-activate-menuitem")
  menu.selectedItem = askToActivateItem;
  askToActivateItem.doCommand();
  gBrowser.selectedTab = gBrowser.addTab();
  gPluginBrowser = gBrowser.selectedBrowser;
  gPluginBrowser.addEventListener("PluginBindingAttached", part11, true, true);
  gPluginBrowser.contentWindow.location = gHttpTestRoot + "plugin_test.html";
}

function part11() {
  ok(PopupNotifications.getNotification("click-to-play-plugins", gPluginBrowser), "part11: should have a click-to-play notification");
  gPluginBrowser.removeEventListener("PluginBindingAttached", part11);
  gBrowser.removeCurrentTab();

  let pluginTag = getTestPluginTag();
  pluginTag.blocklisted = true; // causes appDisabled to be set
  close_manager(gManagerWindow, function() {
    open_manager("addons://list/plugin", part12);
  });
}

function part12(aWindow) {
  gManagerWindow = aWindow;
  let pluginEl = get_addon_element(gManagerWindow, gTestPluginId);
  pluginEl.parentNode.ensureElementIsVisible(pluginEl);
  let menu = gManagerWindow.document.getAnonymousElementByAttribute(pluginEl, "anonid", "tristate-menulist");
  is_element_hidden(menu, "part12: tristate menu should be hidden");

  let details = gManagerWindow.document.getAnonymousElementByAttribute(pluginEl, "anonid", "details-btn");
  EventUtils.synthesizeMouseAtCenter(details, {}, gManagerWindow);
  wait_for_view_load(gManagerWindow, part13);
}

function part13() {
  let menu = gManagerWindow.document.getElementById("detail-tristate-menulist");
  is_element_hidden(menu, "part13: detail tristate menu should be hidden");

  let pluginTag = getTestPluginTag();
  pluginTag.blocklisted = false;
  run_next_test();
}

function end_test() {
  Services.prefs.clearUserPref("plugins.click_to_play");
  let pluginTag = getTestPluginTag();
  pluginTag.enabledState = Ci.nsIPluginTag.STATE_ENABLED;
  close_manager(gManagerWindow, function() {
    finish();
  });
}
