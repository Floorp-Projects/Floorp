/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

ChromeUtils.import("resource://gre/modules/Promise.jsm", this);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
var GMPScope = ChromeUtils.import(
  "resource://gre/modules/addons/GMPProvider.jsm",
  null
);

const TEST_DATE = new Date(2013, 0, 1, 12);

var gManagerWindow;
var gCategoryUtilities;

var gMockAddons = [];

for (let plugin of GMPScope.GMP_PLUGINS) {
  let mockAddon = Object.freeze({
    id: plugin.id,
    isValid: true,
    isInstalled: false,
    isEME: !!(
      plugin.id == "gmp-widevinecdm" || plugin.id.indexOf("gmp-eme-") == 0
    ),
  });
  gMockAddons.push(mockAddon);
}

var gInstalledAddonId = "";
var gInstallDeferred = null;
var gPrefs = Services.prefs;
var getKey = GMPScope.GMPPrefs.getPrefKey;

function MockGMPInstallManager() {}

MockGMPInstallManager.prototype = {
  checkForAddons: () =>
    Promise.resolve({
      usedFallback: true,
      gmpAddons: gMockAddons,
    }),

  installAddon: addon => {
    gInstalledAddonId = addon.id;
    gInstallDeferred.resolve();
    return Promise.resolve();
  },
};

function openDetailsView(aId) {
  let view = get_current_view(gManagerWindow);
  Assert.equal(
    view.id,
    "html-view",
    "Should be in the list view to use this function"
  );

  let item = get_addon_element(gManagerWindow, aId);
  Assert.ok(item, "Should have got add-on element.");
  is_element_visible(item, "Add-on element should be visible.");

  item.scrollIntoView();
  EventUtils.synthesizeMouseAtCenter(item, { clickCount: 1 }, item.ownerGlobal);
  EventUtils.synthesizeMouseAtCenter(item, { clickCount: 2 }, item.ownerGlobal);

  return new Promise(resolve => {
    wait_for_view_load(gManagerWindow, resolve);
  });
}

async function initializeState() {
  gPrefs.setBoolPref(GMPScope.GMPPrefs.KEY_LOGGING_DUMP, true);
  gPrefs.setIntPref(GMPScope.GMPPrefs.KEY_LOGGING_LEVEL, 0);

  gManagerWindow = await open_manager();

  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  registerCleanupFunction(async function() {
    for (let addon of gMockAddons) {
      gPrefs.clearUserPref(
        getKey(GMPScope.GMPPrefs.KEY_PLUGIN_ENABLED, addon.id)
      );
      gPrefs.clearUserPref(
        getKey(GMPScope.GMPPrefs.KEY_PLUGIN_LAST_UPDATE, addon.id)
      );
      gPrefs.clearUserPref(
        getKey(GMPScope.GMPPrefs.KEY_PLUGIN_AUTOUPDATE, addon.id)
      );
      gPrefs.clearUserPref(
        getKey(GMPScope.GMPPrefs.KEY_PLUGIN_VERSION, addon.id)
      );
      gPrefs.clearUserPref(
        getKey(GMPScope.GMPPrefs.KEY_PLUGIN_VISIBLE, addon.id)
      );
      gPrefs.clearUserPref(
        getKey(GMPScope.GMPPrefs.KEY_PLUGIN_FORCE_SUPPORTED, addon.id)
      );
    }
    gPrefs.clearUserPref(GMPScope.GMPPrefs.KEY_LOGGING_DUMP);
    gPrefs.clearUserPref(GMPScope.GMPPrefs.KEY_LOGGING_LEVEL);
    gPrefs.clearUserPref(GMPScope.GMPPrefs.KEY_UPDATE_LAST_CHECK);
    gPrefs.clearUserPref(GMPScope.GMPPrefs.KEY_EME_ENABLED);
    await GMPScope.GMPProvider.shutdown();
    GMPScope.GMPProvider.startup();
  });

  // Start out with plugins not being installed, disabled and automatic updates
  // disabled.
  gPrefs.setBoolPref(GMPScope.GMPPrefs.KEY_EME_ENABLED, true);
  for (let addon of gMockAddons) {
    gPrefs.setBoolPref(
      getKey(GMPScope.GMPPrefs.KEY_PLUGIN_ENABLED, addon.id),
      false
    );
    gPrefs.setIntPref(
      getKey(GMPScope.GMPPrefs.KEY_PLUGIN_LAST_UPDATE, addon.id),
      0
    );
    gPrefs.setBoolPref(
      getKey(GMPScope.GMPPrefs.KEY_PLUGIN_AUTOUPDATE, addon.id),
      false
    );
    gPrefs.setCharPref(
      getKey(GMPScope.GMPPrefs.KEY_PLUGIN_VERSION, addon.id),
      ""
    );
    gPrefs.setBoolPref(
      getKey(GMPScope.GMPPrefs.KEY_PLUGIN_VISIBLE, addon.id),
      true
    );
    gPrefs.setBoolPref(
      getKey(GMPScope.GMPPrefs.KEY_PLUGIN_FORCE_SUPPORTED, addon.id),
      true
    );
  }
  await GMPScope.GMPProvider.shutdown();
  GMPScope.GMPProvider.startup();
}

async function testNotInstalledDisabled() {
  Assert.ok(gCategoryUtilities.isTypeVisible("plugin"), "Plugin tab visible.");
  await gCategoryUtilities.openType("plugin");

  for (let addon of gMockAddons) {
    let addonCard = get_addon_element(gManagerWindow, addon.id);
    Assert.ok(addonCard, "Got add-on element:" + addon.id);

    is(
      addonCard.ownerDocument.l10n.getAttributes(addonCard.addonNameEl).id,
      "addon-name-disabled",
      "The addon name should include a disabled postfix"
    );

    let cardMessage = addonCard.querySelector("message-bar.addon-card-message");
    is_element_hidden(cardMessage, "Warning notification is hidden");
  }
}

async function testNotInstalledDisabledDetails() {
  for (let addon of gMockAddons) {
    await openDetailsView(addon.id);
    let doc = gManagerWindow.document;

    let addonCard = get_addon_element(gManagerWindow, addon.id);
    ok(addonCard, "Got add-on element: " + addon.id);

    is(
      doc.l10n.getAttributes(addonCard.addonNameEl).id,
      "addon-name-disabled",
      "The addon name should include a disabled postfix"
    );

    let updatesBtn = addonCard.querySelector("[action=update-check]");
    is_element_visible(updatesBtn, "Check for Updates action is visible");
    let cardMessage = addonCard.querySelector("message-bar.addon-card-message");
    is_element_hidden(cardMessage, "Warning notification is hidden");

    await gCategoryUtilities.openType("plugin");
  }
}

async function testNotInstalled() {
  for (let addon of gMockAddons) {
    gPrefs.setBoolPref(
      getKey(GMPScope.GMPPrefs.KEY_PLUGIN_ENABLED, addon.id),
      true
    );
    let item = get_addon_element(gManagerWindow, addon.id);
    Assert.ok(item, "Got add-on element:" + addon.id);

    let warningMessageBar = await BrowserTestUtils.waitForCondition(() => {
      return item.querySelector("message-bar.addon-card-message[type=warning]");
    }, "Wait for the addon card message to be updated");

    is_element_visible(warningMessageBar, "Warning notification is visible");

    is(item.parentNode.getAttribute("section"), "0", "Should be enabled");
    // Open the options menu (needed to check the disabled buttons).
    const pluginOptions = item.querySelector("plugin-options");
    pluginOptions.querySelector("panel-list").open = true;
    const alwaysActivate = pluginOptions.querySelector(
      "panel-item[action=always-activate]"
    );
    ok(
      alwaysActivate.hasAttribute("checked"),
      "Plugin state should be always-activate"
    );
    pluginOptions.querySelector("panel-list").open = false;
  }
}

async function testNotInstalledDetails() {
  for (let addon of gMockAddons) {
    await openDetailsView(addon.id);

    const addonCard = get_addon_element(gManagerWindow, addon.id);
    let el = addonCard.querySelector("[action=update-check]");
    is_element_visible(el, "Check for Updates action is visible");

    let warningMessageBar = await BrowserTestUtils.waitForCondition(() => {
      return addonCard.querySelector(
        "message-bar.addon-card-message[type=warning]"
      );
    }, "Wait for the addon card message to be updated");
    is_element_visible(warningMessageBar, "Warning notification is visible");

    await gCategoryUtilities.openType("plugin");
  }
}

async function testInstalled() {
  for (let addon of gMockAddons) {
    gPrefs.setIntPref(
      getKey(GMPScope.GMPPrefs.KEY_PLUGIN_LAST_UPDATE, addon.id),
      TEST_DATE.getTime()
    );
    gPrefs.setBoolPref(
      getKey(GMPScope.GMPPrefs.KEY_PLUGIN_AUTOUPDATE, addon.id),
      false
    );
    gPrefs.setCharPref(
      getKey(GMPScope.GMPPrefs.KEY_PLUGIN_VERSION, addon.id),
      "1.2.3.4"
    );

    let item = get_addon_element(gManagerWindow, addon.id);
    Assert.ok(item, "Got add-on element.");

    is(item.parentNode.getAttribute("section"), "0", "Should be enabled");
    // Open the options menu (needed to check the disabled buttons).
    const pluginOptions = item.querySelector("plugin-options");
    pluginOptions.querySelector("panel-list").open = true;
    const alwaysActivate = pluginOptions.querySelector(
      "panel-item[action=always-activate]"
    );
    ok(
      alwaysActivate.hasAttribute("checked"),
      "Plugin state should be always-activate"
    );
    pluginOptions.querySelector("panel-list").open = false;
  }
}

async function testInstalledDetails() {
  for (let addon of gMockAddons) {
    await openDetailsView(addon.id);

    let card = get_addon_element(gManagerWindow, addon.id);
    ok(card, "Got add-on element:" + addon.id);

    is_element_visible(
      card.querySelector("[action=update-check]"),
      "Find updates link is visible"
    );

    await gCategoryUtilities.openType("plugin");
  }
}

async function testInstalledGlobalEmeDisabled() {
  gPrefs.setBoolPref(GMPScope.GMPPrefs.KEY_EME_ENABLED, false);
  for (let addon of gMockAddons) {
    let item = get_addon_element(gManagerWindow, addon.id);
    if (addon.isEME) {
      is(item.parentNode.getAttribute("section"), "1", "Should be disabled");
      // Open the options menu (needed to check the disabled buttons).
      const pluginOptions = item.querySelector("plugin-options");
      pluginOptions.querySelector("panel-list").open = true;
      const askActivate = pluginOptions.querySelector(
        "panel-item[action=ask-to-activate]"
      );
      ok(
        askActivate.shadowRoot.querySelector("button").disabled,
        "ask-to-activate should be disabled"
      );
      pluginOptions.querySelector("panel-list").open = false;
    } else {
      Assert.ok(item, "Got add-on element.");
    }
  }
  gPrefs.setBoolPref(GMPScope.GMPPrefs.KEY_EME_ENABLED, true);
}

async function testPreferencesButton() {
  let prefValues = [
    { enabled: false, version: "" },
    { enabled: false, version: "1.2.3.4" },
    { enabled: true, version: "" },
    { enabled: true, version: "1.2.3.4" },
  ];

  for (let preferences of prefValues) {
    dump(
      "Testing preferences button with pref settings: " +
        JSON.stringify(preferences) +
        "\n"
    );
    for (let addon of gMockAddons) {
      await close_manager(gManagerWindow);
      gManagerWindow = await open_manager();
      gCategoryUtilities = new CategoryUtilities(gManagerWindow);
      gPrefs.setCharPref(
        getKey(GMPScope.GMPPrefs.KEY_PLUGIN_VERSION, addon.id),
        preferences.version
      );
      gPrefs.setBoolPref(
        getKey(GMPScope.GMPPrefs.KEY_PLUGIN_ENABLED, addon.id),
        preferences.enabled
      );

      await gCategoryUtilities.openType("plugin");
      let item = get_addon_element(gManagerWindow, addon.id);

      // Open the options menu (needed to check the more options action is enabled).
      const pluginOptions = item.querySelector("plugin-options");
      pluginOptions.querySelector("panel-list").open = true;
      const moreOptions = pluginOptions.querySelector(
        "panel-item[action=expand]"
      );
      ok(
        !moreOptions.shadowRoot.querySelector("button").disabled,
        "more options action should be enabled"
      );
      moreOptions.click();

      await wait_for_view_load(gManagerWindow);
    }
  }
}

async function testUpdateButton() {
  gPrefs.clearUserPref(GMPScope.GMPPrefs.KEY_UPDATE_LAST_CHECK);

  let originalInstallManager = GMPScope.GMPInstallManager;
  Object.defineProperty(GMPScope, "GMPInstallManager", {
    value: MockGMPInstallManager,
    writable: true,
    enumerable: true,
    configurable: true,
  });

  for (let addon of gMockAddons) {
    await gCategoryUtilities.openType("plugin");
    let item = get_addon_element(gManagerWindow, addon.id);

    gInstalledAddonId = "";
    gInstallDeferred = Promise.defer();

    item.querySelector("[action=expand]").click();
    await wait_for_view_load(gManagerWindow);
    let detail = get_addon_element(gManagerWindow, addon.id);
    detail.querySelector("[action=update-check]").click();

    await gInstallDeferred.promise;
    Assert.equal(gInstalledAddonId, addon.id);
  }
  Object.defineProperty(GMPScope, "GMPInstallManager", {
    value: originalInstallManager,
    writable: true,
    enumerable: true,
    configurable: true,
  });
}

async function testEmeSupport() {
  for (let addon of gMockAddons) {
    gPrefs.clearUserPref(
      getKey(GMPScope.GMPPrefs.KEY_PLUGIN_FORCE_SUPPORTED, addon.id)
    );
  }
  await GMPScope.GMPProvider.shutdown();
  GMPScope.GMPProvider.startup();

  for (let addon of gMockAddons) {
    await gCategoryUtilities.openType("plugin");
    let item = get_addon_element(gManagerWindow, addon.id);
    if (addon.id == GMPScope.EME_ADOBE_ID) {
      if (AppConstants.isPlatformAndVersionAtLeast("win", "6")) {
        Assert.ok(item, "Adobe EME supported, found add-on element.");
      } else {
        Assert.ok(
          !item,
          "Adobe EME not supported, couldn't find add-on element."
        );
      }
    } else if (addon.id == GMPScope.WIDEVINE_ID) {
      if (
        AppConstants.isPlatformAndVersionAtLeast("win", "6") ||
        AppConstants.platform == "macosx" ||
        AppConstants.platform == "linux"
      ) {
        Assert.ok(item, "Widevine supported, found add-on element.");
      } else {
        Assert.ok(
          !item,
          "Widevine not supported, couldn't find add-on element."
        );
      }
    } else {
      Assert.ok(item, "Found add-on element.");
    }
  }

  for (let addon of gMockAddons) {
    gPrefs.setBoolPref(
      getKey(GMPScope.GMPPrefs.KEY_PLUGIN_VISIBLE, addon.id),
      true
    );
    gPrefs.setBoolPref(
      getKey(GMPScope.GMPPrefs.KEY_PLUGIN_FORCE_SUPPORTED, addon.id),
      true
    );
  }
  await GMPScope.GMPProvider.shutdown();
  GMPScope.GMPProvider.startup();
}

async function testCleanupState() {
  await SpecialPowers.popPrefEnv();
  await close_manager(gManagerWindow);
}

// This function run the sequence of all the gmpProvider tests
// under the same initializeStateOptions (which will enable or disable
// the HTML about:addons views).
add_task(async function test_gmpProvider(initializeStateOptions) {
  await initializeState();
  await testNotInstalledDisabled();
  await testNotInstalledDisabledDetails();
  await testNotInstalled();
  await testNotInstalledDetails();
  await testInstalled();
  await testInstalledDetails();
  await testInstalledGlobalEmeDisabled();
  await testPreferencesButton();
  await testUpdateButton();
  await testEmeSupport();
  await testCleanupState();
});
