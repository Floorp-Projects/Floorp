/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test plugin blocking.

const gHttpTestRoot = "http://127.0.0.1:8888/" + RELATIVE_DIR + "/";
const {AddonTestUtils} = ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");

function updateBlocklist(aURL, aCallback) {
  var observer = function() {
    Services.obs.removeObserver(observer, "plugin-blocklist-updated");
    SimpleTest.executeSoon(aCallback);
  };
  Services.obs.addObserver(observer, "plugin-blocklist-updated");
  if (Services.prefs.getBoolPref("extensions.blocklist.useXML", true)) {
    info("Loading plugin data " + aURL + " using xml implementation.");
    Services.prefs.setCharPref("extensions.blocklist.url", aURL);
    var blocklistNotifier = Cc["@mozilla.org/extensions/blocklist;1"]
      .getService(Ci.nsITimerCallback);
    blocklistNotifier.notify(null);
  } else {
    info("Loading plugin data " + aURL + " using remote settings.");
    if (aURL.endsWith("blockNoPlugins.xml")) {
      AddonTestUtils.loadBlocklistRawData({plugins: []});
    } else if (aURL.endsWith("blockPluginHard.xml")) {
      AddonTestUtils.loadBlocklistRawData({plugins: [{
        matchFilename: "libnptest\\.so|nptest\\.dll|Test\\.plugin",
        versionRange: [{"severity": "2"}],
        blockID: "p9999",
      }]});
    } else {
      ok(false, "Should never be asked to update to unknown blocklist data.");
    }
  }
}

var _originalBlocklistURL = null;
function setAndUpdateBlocklist(aURL, aCallback) {
  if (!_originalBlocklistURL) {
    _originalBlocklistURL = Services.prefs.getCharPref("extensions.blocklist.url");
  }
  updateBlocklist(aURL, aCallback);
}

function resetBlocklist() {
  Services.prefs.setCharPref("extensions.blocklist.url", _originalBlocklistURL);
}

function getXULPluginUI(plugin, anonid) {
  if (plugin.openOrClosedShadowRoot &&
      plugin.openOrClosedShadowRoot.isUAWidget()) {
    return plugin.openOrClosedShadowRoot.getElementById(anonid);
  }
  return plugin.ownerDocument.
    getAnonymousElementByAttribute(plugin, "anonid", anonid);
}

function assertPluginActiveState({managerWindow, pluginId, expectedActivateState}) {
  let pluginEl = get_addon_element(managerWindow, pluginId);
  ok(pluginEl, `Got the about:addon entry for "${pluginId}"`);

  if (managerWindow.useHtmlViews) {
    const pluginOptions = pluginEl.querySelector("plugin-options");
    const pluginCheckedItem = pluginOptions.querySelector("panel-item[checked]");
    is(pluginCheckedItem.getAttribute("action"), expectedActivateState,
       `plugin should have ${expectedActivateState} state selected`);
  } else {
    // Assertions for the XUL about:addons views.
    pluginEl.parentNode.ensureElementIsVisible(pluginEl);
    let enableButton = getXULPluginUI(pluginEl, "enable-btn");
    let disableButton = getXULPluginUI(pluginEl, "disable-btn");
    is_element_hidden(enableButton, "enable button should be hidden");
    is_element_hidden(disableButton, "disable button should be hidden");
    let menu = getXULPluginUI(pluginEl, "state-menulist");
    is_element_visible(menu, "state menu should be visible");
    let activateItem = getXULPluginUI(pluginEl, `${expectedActivateState}-menuitem`);
    ok(activateItem, `Got a menu item for the ${expectedActivateState} plugin activate state`);
    is(menu.selectedItem, activateItem, `state menu should have '${expectedActivateState}' selected`);
  }
}

function setPluginActivateState({managerWindow, pluginId, activateState}) {
  let pluginEl = get_addon_element(managerWindow, pluginId);
  ok(pluginEl, `Got the about:addon entry for "${pluginId}"`);

  if (managerWindow.useHtmlViews) {
    // Activate plugin on the HTML about:addons views.
    let activateAction = pluginEl.querySelector(`[action="${activateState}"]`);
    ok(activateAction, `Got element for ${activateState} plugin action`);
    activateAction.click();
  } else {
    // Activate plugin on the XUL about:addons views.
    let activateItem = getXULPluginUI(pluginEl, `${activateState}-menuitem`);
    ok(activateItem, `Got a menu item for the ${activateState} plugin activate state`);
    let menu = getXULPluginUI(pluginEl, "state-menulist");
    menu.selectedItem = activateItem;
    activateItem.doCommand();
  }
}

async function assertPluginAppDisabled({managerWindow, pluginId}) {
  const pluginEl = get_addon_element(managerWindow, pluginId);
  ok(pluginEl, `Got the about:addon entry for "${pluginId}"`);

  if (managerWindow.useHtmlViews) {
    // Open the options menu (needed to check the disabled buttons).
    const pluginOptions = pluginEl.querySelector("plugin-options");
    pluginOptions.querySelector("panel-list").open = true;
    // tests all buttons disabled (besides the checked one and the expand action)
    // are expected to be disabled if locked is true.
    for (const item of pluginOptions.querySelectorAll("panel-item:not([hidden])")) {
      const actionName = item.getAttribute("action");
      if (!item.hasAttribute("checked") && actionName !== "expand" &&
          actionName !== "preferences") {
        ok(item.shadowRoot.querySelector("button").disabled,
           `Plugin action "${actionName}" should be disabled`);
      }
    }
    pluginOptions.querySelector("panel-list").open = false;
  } else {
    // Assertions for the XUL about:addons views.
    let menu = getXULPluginUI(pluginEl, "state-menulist");
    pluginEl.parentNode.ensureElementIsVisible(pluginEl);
    menu = getXULPluginUI(pluginEl, "state-menulist");
    is(menu.disabled, true, "state menu should be disabled");

    EventUtils.synthesizeMouseAtCenter(pluginEl, {}, managerWindow);
    await BrowserTestUtils.waitForEvent(managerWindow.document, "ViewChanged");

    menu = managerWindow.document.getElementById("detail-state-menulist");
    is(menu.disabled, true, "detail state menu should be disabled");
  }
}

async function getTestPluginAddon() {
  const plugins = await AddonManager.getAddonsByTypes(["plugin"]);

  for (const plugin of plugins) {
    if (plugin.name == "Test Plug-in") {
      return plugin;
    }
  }

  return undefined;
}

async function test_CTP_plugins(aboutAddonsType) {
  await SpecialPowers.pushPrefEnv({"set": [
    ["plugins.click_to_play", true],
  ]});
  let pluginTag = getTestPluginTag();
  pluginTag.enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;
  let managerWindow = await open_manager("addons://list/plugin");

  let testPluginAddon = await getTestPluginAddon();
  isnot(testPluginAddon, null, "part2: Test Plug-in should exist");

  let testPluginId = testPluginAddon.id;

  let pluginEl = get_addon_element(managerWindow, testPluginId);
  ok(pluginEl, `Got the about:addon entry for "${testPluginId}"`);

  info("part3: test plugin add-on actions status");
  assertPluginActiveState({
    managerWindow,
    pluginId: testPluginId,
    expectedActivateState: "ask-to-activate",
  });

  let pluginTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, gHttpTestRoot + "plugin_test.html");
  let pluginBrowser = pluginTab.linkedBrowser;

  let condition = () => PopupNotifications.getNotification("click-to-play-plugins", pluginBrowser);
  await BrowserTestUtils.waitForCondition(condition, "part4: should have a click-to-play notification");

  BrowserTestUtils.removeTab(pluginTab);

  setPluginActivateState({
    managerWindow,
    pluginId: testPluginId,
    activateState: "always-activate",
  });

  pluginTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, gHttpTestRoot + "plugin_test.html");

  await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let testPlugin = content.document.getElementById("test");
    ok(testPlugin, "part5: should have a plugin element in the page");
    let objLoadingContent = testPlugin.QueryInterface(Ci.nsIObjectLoadingContent);
    let condition = () => objLoadingContent.activated;
    await ContentTaskUtils.waitForCondition(condition, "part5: waited too long for plugin to activate");
    ok(objLoadingContent.activated, "part6: plugin should be activated");
  });

  BrowserTestUtils.removeTab(pluginTab);

  setPluginActivateState({
    managerWindow,
    pluginId: testPluginId,
    activateState: "never-activate",
  });

  pluginTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, gHttpTestRoot + "plugin_test.html");
  pluginBrowser = pluginTab.linkedBrowser;

  await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let testPlugin = content.document.getElementById("test");
    ok(testPlugin, "part7: should have a plugin element in the page");
    let objLoadingContent = testPlugin.QueryInterface(Ci.nsIObjectLoadingContent);
    ok(!objLoadingContent.activated, "part7: plugin should not be activated");
  });

  BrowserTestUtils.removeTab(pluginTab);

  info("part8: test plugin state is never-activate");
  assertPluginActiveState({
    managerWindow,
    pluginId: testPluginId,
    expectedActivateState: "never-activate",
  });

  info("part9: set plugin state to always-activate");
  setPluginActivateState({
    managerWindow,
    pluginId: testPluginId,
    activateState: "always-activate",
  });

  pluginTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, gHttpTestRoot + "plugin_test.html");
  pluginBrowser = pluginTab.linkedBrowser;

  await ContentTask.spawn(pluginTab.linkedBrowser, null, async function() {
    let testPlugin = content.document.getElementById("test");
    ok(testPlugin, "part9: should have a plugin element in the page");
    let objLoadingContent = testPlugin.QueryInterface(Ci.nsIObjectLoadingContent);
    let condition = () => objLoadingContent.activated;
    await ContentTaskUtils.waitForCondition(condition, "part9: waited too long for plugin to activate");
    ok(objLoadingContent.activated, "part10: plugin should be activated");
  });

  BrowserTestUtils.removeTab(pluginTab);

  setPluginActivateState({
    managerWindow,
    pluginId: testPluginId,
    activateState: "ask-to-activate",
  });

  pluginTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, gHttpTestRoot + "plugin_test.html");
  pluginBrowser = pluginTab.linkedBrowser;

  condition = () => PopupNotifications.getNotification("click-to-play-plugins", pluginBrowser);
  await BrowserTestUtils.waitForCondition(condition, "part11: should have a click-to-play notification");

  BrowserTestUtils.removeTab(pluginTab);
  await close_manager(managerWindow);
  await SpecialPowers.popPrefEnv();
}

add_task(async function test_CTP_plugins_XUL_aboutaddons() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", false]],
  });
  await test_CTP_plugins("XUL");
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_CTP_plugins_HTML_aboutaddons() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", true]],
  });
  await test_CTP_plugins("HTML");
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_blocklisted_plugin_disabled() {
  async function ensurePluginEnabled() {
    let pluginTag = getTestPluginTag();
    pluginTag.enabledState = Ci.nsIPluginTag.STATE_ENABLED;
    await new Promise(resolve => {
      setAndUpdateBlocklist(gHttpTestRoot + "blockNoPlugins.xml", resolve);
    });
    resetBlocklist();
  }

  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.blocklist.suppressUI", true],
      ["extensions.blocklist.useXML", true],
    ],
  });

  // Causes appDisabled to be set.
  await new Promise(async resolve => {
    // Ensure to reset the blocklist if this test exits earlier because
    // of a failure.
    registerCleanupFunction(ensurePluginEnabled);
    setAndUpdateBlocklist(gHttpTestRoot + "blockPluginHard.xml", resolve);
  });

  await checkPlugins();

  await SpecialPowers.pushPrefEnv({
    set: [["extensions.blocklist.useXML", false]],
  });
  await checkPlugins();

  // Clear the blocklist and all prefs on the stack.
  await ensurePluginEnabled();
  // Using flushPrefEnv instead of 2x popPrefEnv to work around bug 1557397.
  await SpecialPowers.flushPrefEnv();
});

async function checkPlugins() {
  let testPluginAddon = await getTestPluginAddon();
  isnot(testPluginAddon, null, "Test Plug-in should exist");
  let testPluginId = testPluginAddon.id;

  let managerWindow;

  info("Test blocklisted plugin actions disabled in XUL about:addons");
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", false]],
  });
  managerWindow = await open_manager("addons://list/plugin");
  await assertPluginAppDisabled({managerWindow, pluginId: testPluginId});
  await close_manager(managerWindow);

  info("Test blocklisted plugin actions disabled in HTML about:addons");
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", true]],
  });
  managerWindow = await open_manager("addons://list/plugin");
  await assertPluginAppDisabled({managerWindow, pluginId: testPluginId});
  await close_manager(managerWindow);
}
