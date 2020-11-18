/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test plugin blocking.

const gHttpTestRoot = "http://127.0.0.1:8888/" + RELATIVE_DIR + "/";
const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

function updateBlocklist(aURL, aCallback) {
  var observer = function() {
    Services.obs.removeObserver(observer, "plugin-blocklist-updated");
    SimpleTest.executeSoon(aCallback);
  };
  Services.obs.addObserver(observer, "plugin-blocklist-updated");
  info("Loading plugin data " + aURL);
  if (aURL.endsWith("blockNoPlugins")) {
    AddonTestUtils.loadBlocklistRawData({ plugins: [] });
  } else if (aURL.endsWith("blockPluginHard")) {
    AddonTestUtils.loadBlocklistRawData({
      plugins: [
        {
          matchFilename: "libnptest\\.so|nptest\\.dll|Test\\.plugin",
          versionRange: [{ severity: "2" }],
          blockID: "p9999",
        },
      ],
    });
  } else {
    ok(false, "Should never be asked to update to unknown blocklist data.");
  }
}

function setPluginActivateState({ managerWindow, pluginId, activateState }) {
  let pluginEl = get_addon_element(managerWindow, pluginId);
  ok(pluginEl, `Got the about:addon entry for "${pluginId}"`);

  // Activate plugin on the HTML about:addons views.
  let activateAction = pluginEl.querySelector(`[action="${activateState}"]`);
  ok(activateAction, `Got element for ${activateState} plugin action`);
  activateAction.click();
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

add_task(async function test_CTP_plugins() {
  let pluginTag = getTestPluginTag();
  pluginTag.enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;
  let managerWindow = await open_manager("addons://list/plugin");

  let testPluginAddon = await getTestPluginAddon();
  isnot(testPluginAddon, null, "part2: Test Plug-in should exist");

  let testPluginId = testPluginAddon.id;

  let pluginEl = get_addon_element(managerWindow, testPluginId);
  ok(pluginEl, `Got the about:addon entry for "${testPluginId}"`);

  info("part3: test plugin add-on actions status");

  let pluginOptions = pluginEl.querySelector("plugin-options");
  let pluginCheckedItem = pluginOptions.querySelector("panel-item[checked]");
  is(
    pluginCheckedItem.getAttribute("action"),
    "ask-to-activate",
    "plugin should have ask-to-activate state selected"
  );

  let pluginTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    gHttpTestRoot + "plugin_test.html"
  );
  let pluginBrowser = pluginTab.linkedBrowser;

  let condition = () =>
    PopupNotifications.getNotification("click-to-play-plugins", pluginBrowser);
  await BrowserTestUtils.waitForCondition(
    condition,
    "part4: should have a click-to-play notification"
  );

  BrowserTestUtils.removeTab(pluginTab);

  setPluginActivateState({
    managerWindow,
    pluginId: testPluginId,
    activateState: "always-activate",
  });

  pluginTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    gHttpTestRoot + "plugin_test.html"
  );

  await SpecialPowers.spawn(pluginTab.linkedBrowser, [], async function() {
    let testPlugin = content.document.getElementById("test");
    ok(testPlugin, "part5: should have a plugin element in the page");
    let condition = () => testPlugin.activated;
    await ContentTaskUtils.waitForCondition(
      condition,
      "part5: waited too long for plugin to activate"
    );
    ok(testPlugin.activated, "part6: plugin should be activated");
  });

  BrowserTestUtils.removeTab(pluginTab);

  setPluginActivateState({
    managerWindow,
    pluginId: testPluginId,
    activateState: "never-activate",
  });

  pluginTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    gHttpTestRoot + "plugin_test.html"
  );
  pluginBrowser = pluginTab.linkedBrowser;

  await SpecialPowers.spawn(pluginTab.linkedBrowser, [], async function() {
    let testPlugin = content.document.getElementById("test");
    ok(testPlugin, "part7: should have a plugin element in the page");
    ok(!testPlugin.activated, "part7: plugin should not be activated");
  });

  BrowserTestUtils.removeTab(pluginTab);

  info("part8: test plugin state is never-activate");
  pluginEl = get_addon_element(managerWindow, testPluginId);

  pluginOptions = pluginEl.querySelector("plugin-options");
  const pluginCheckItem = pluginOptions.querySelector("panel-item[checked]");
  is(
    pluginCheckItem.getAttribute("action"),
    "never-activate",
    "plugin should have never-activate state selected"
  );

  info("part9: set plugin state to always-activate");
  setPluginActivateState({
    managerWindow,
    pluginId: testPluginId,
    activateState: "always-activate",
  });

  pluginTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    gHttpTestRoot + "plugin_test.html"
  );
  pluginBrowser = pluginTab.linkedBrowser;

  await SpecialPowers.spawn(pluginTab.linkedBrowser, [], async function() {
    let testPlugin = content.document.getElementById("test");
    ok(testPlugin, "part9: should have a plugin element in the page");
    let condition = () => testPlugin.activated;
    await ContentTaskUtils.waitForCondition(
      condition,
      "part9: waited too long for plugin to activate"
    );
    ok(testPlugin.activated, "part10: plugin should be activated");
  });

  BrowserTestUtils.removeTab(pluginTab);

  setPluginActivateState({
    managerWindow,
    pluginId: testPluginId,
    activateState: "ask-to-activate",
  });

  pluginTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    gHttpTestRoot + "plugin_test.html"
  );
  pluginBrowser = pluginTab.linkedBrowser;

  condition = () =>
    PopupNotifications.getNotification("click-to-play-plugins", pluginBrowser);
  await BrowserTestUtils.waitForCondition(
    condition,
    "part11: should have a click-to-play notification"
  );

  BrowserTestUtils.removeTab(pluginTab);
  await close_manager(managerWindow);
});

add_task(async function test_blocklisted_plugin_disabled() {
  async function ensurePluginEnabled() {
    let pluginTag = getTestPluginTag();
    pluginTag.enabledState = Ci.nsIPluginTag.STATE_ENABLED;
    await new Promise(resolve => {
      updateBlocklist(gHttpTestRoot + "blockNoPlugins", resolve);
    });
  }

  await SpecialPowers.pushPrefEnv({
    set: [["extensions.blocklist.suppressUI", true]],
  });

  // Causes appDisabled to be set.
  await new Promise(resolve => {
    // Ensure to reset the blocklist if this test exits earlier because
    // of a failure.
    registerCleanupFunction(ensurePluginEnabled);
    updateBlocklist(gHttpTestRoot + "blockPluginHard", resolve);
  });

  await checkPlugins();

  // Using flushPrefEnv instead of 2x popPrefEnv to work around bug 1557397.
  await SpecialPowers.flushPrefEnv();
});

async function checkPlugins() {
  let testPluginAddon = await getTestPluginAddon();
  isnot(testPluginAddon, null, "Test Plug-in should exist");
  let testPluginId = testPluginAddon.id;

  let managerWindow = await open_manager("addons://list/plugin");

  let pluginEl = get_addon_element(managerWindow, testPluginId);
  ok(pluginEl, `Got about:addons entry for ${testPluginId}`);

  // Open the options menu (needed to check the disabled buttons).
  const pluginOptions = pluginEl.querySelector("plugin-options");
  pluginOptions.querySelector("panel-list").open = true;

  // tests all buttons disabled (besides the checked one and the expand action)
  // are expected to be disabled if locked is true.
  for (const item of pluginOptions.querySelectorAll(
    "panel-item:not([hidden])"
  )) {
    const actionName = item.getAttribute("action");
    if (
      !item.hasAttribute("checked") &&
      actionName !== "expand" &&
      actionName !== "preferences"
    ) {
      ok(
        item.shadowRoot.querySelector("button").disabled,
        `Plugin action "${actionName}" should be disabled`
      );
    }
  }
  pluginOptions.querySelector("panel-list").open = false;

  await close_manager(managerWindow);
}
