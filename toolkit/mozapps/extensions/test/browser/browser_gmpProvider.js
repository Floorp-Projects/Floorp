/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { GMPInstallManager } = ChromeUtils.importESModule(
  "resource://gre/modules/GMPInstallManager.sys.mjs"
);
const { GMPPrefs, GMP_PLUGIN_IDS, WIDEVINE_L1_ID, WIDEVINE_L3_ID } =
  ChromeUtils.importESModule("resource://gre/modules/GMPUtils.sys.mjs");

const TEST_DATE = new Date(2013, 0, 1, 12);

var gMockAddons = [];

for (let pluginId of GMP_PLUGIN_IDS) {
  let mockAddon = Object.freeze({
    id: pluginId,
    isValid: true,
    isInstalled: false,
    isEME: pluginId == WIDEVINE_L1_ID || pluginId == WIDEVINE_L3_ID,
    usedFallback: true,
  });
  gMockAddons.push(mockAddon);
}

var gInstalledAddonId = "";
var gInstallDeferred = null;
var gPrefs = Services.prefs;
var getKey = GMPPrefs.getPrefKey;

const MockGMPInstallManagerPrototype = {
  checkForAddons: () =>
    Promise.resolve({
      addons: gMockAddons,
    }),

  installAddon: addon => {
    gInstalledAddonId = addon.id;
    gInstallDeferred.resolve();
    return Promise.resolve();
  },
};

function openDetailsView(win, id) {
  let item = getAddonCard(win, id);
  Assert.ok(item, "Should have got add-on element.");
  is_element_visible(item, "Add-on element should be visible.");

  let loaded = waitForViewLoad(win);
  EventUtils.synthesizeMouseAtCenter(item, {}, item.ownerGlobal);
  return loaded;
}

add_task(async function initializeState() {
  gPrefs.setBoolPref(GMPPrefs.KEY_LOGGING_DUMP, true);
  gPrefs.setIntPref(GMPPrefs.KEY_LOGGING_LEVEL, 0);

  registerCleanupFunction(async function () {
    for (let addon of gMockAddons) {
      gPrefs.clearUserPref(getKey(GMPPrefs.KEY_PLUGIN_ENABLED, addon.id));
      gPrefs.clearUserPref(getKey(GMPPrefs.KEY_PLUGIN_LAST_UPDATE, addon.id));
      gPrefs.clearUserPref(getKey(GMPPrefs.KEY_PLUGIN_AUTOUPDATE, addon.id));
      gPrefs.clearUserPref(getKey(GMPPrefs.KEY_PLUGIN_VERSION, addon.id));
      gPrefs.clearUserPref(getKey(GMPPrefs.KEY_PLUGIN_VISIBLE, addon.id));
      gPrefs.clearUserPref(
        getKey(GMPPrefs.KEY_PLUGIN_FORCE_SUPPORTED, addon.id)
      );
    }
    gPrefs.clearUserPref(GMPPrefs.KEY_LOGGING_DUMP);
    gPrefs.clearUserPref(GMPPrefs.KEY_LOGGING_LEVEL);
    gPrefs.clearUserPref(GMPPrefs.KEY_UPDATE_LAST_CHECK);
    gPrefs.clearUserPref(GMPPrefs.KEY_EME_ENABLED);
  });

  // Start out with plugins not being installed, disabled and automatic updates
  // disabled.
  gPrefs.setBoolPref(GMPPrefs.KEY_EME_ENABLED, true);
  for (let addon of gMockAddons) {
    gPrefs.setBoolPref(getKey(GMPPrefs.KEY_PLUGIN_ENABLED, addon.id), false);
    gPrefs.setIntPref(getKey(GMPPrefs.KEY_PLUGIN_LAST_UPDATE, addon.id), 0);
    gPrefs.setBoolPref(getKey(GMPPrefs.KEY_PLUGIN_AUTOUPDATE, addon.id), false);
    gPrefs.setCharPref(getKey(GMPPrefs.KEY_PLUGIN_VERSION, addon.id), "");
    gPrefs.setBoolPref(getKey(GMPPrefs.KEY_PLUGIN_VISIBLE, addon.id), true);
    gPrefs.setBoolPref(
      getKey(GMPPrefs.KEY_PLUGIN_FORCE_SUPPORTED, addon.id),
      true
    );
  }
});

add_task(async function testNotInstalledDisabled() {
  let win = await loadInitialView("extension");

  Assert.ok(isCategoryVisible(win, "plugin"), "Plugin tab visible.");
  await switchView(win, "plugin");

  for (let addon of gMockAddons) {
    let addonCard = getAddonCard(win, addon.id);
    Assert.ok(addonCard, "Got add-on element:" + addon.id);

    is(
      addonCard.ownerDocument.l10n.getAttributes(addonCard.addonNameEl).id,
      "addon-name-disabled",
      "The addon name should include a disabled postfix"
    );

    let cardMessage = addonCard.querySelector(
      "moz-message-bar.addon-card-message"
    );
    is_element_hidden(cardMessage, "Warning notification is hidden");
  }

  await closeView(win);
});

add_task(async function testNotInstalledDisabledDetails() {
  let win = await loadInitialView("plugin");

  for (let addon of gMockAddons) {
    await openDetailsView(win, addon.id);
    let addonCard = getAddonCard(win, addon.id);
    ok(addonCard, "Got add-on element: " + addon.id);

    is(
      win.document.l10n.getAttributes(addonCard.addonNameEl).id,
      "addon-name-disabled",
      "The addon name should include a disabled postfix"
    );

    let updatesBtn = addonCard.querySelector("[action=update-check]");
    is_element_visible(updatesBtn, "Check for Updates action is visible");
    let cardMessage = addonCard.querySelector(
      "moz-message-bar.addon-card-message"
    );
    is_element_hidden(cardMessage, "Warning notification is hidden");

    await switchView(win, "plugin");
  }

  await closeView(win);
});

add_task(async function testNotInstalled() {
  let win = await loadInitialView("plugin");

  for (let addon of gMockAddons) {
    gPrefs.setBoolPref(getKey(GMPPrefs.KEY_PLUGIN_ENABLED, addon.id), true);
    let item = getAddonCard(win, addon.id);
    Assert.ok(item, "Got add-on element:" + addon.id);

    let warningMessageBar = await BrowserTestUtils.waitForCondition(() => {
      return item.querySelector(
        "moz-message-bar.addon-card-message[type=warning]"
      );
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

  await closeView(win);
});

add_task(async function testNotInstalledDetails() {
  let win = await loadInitialView("plugin");

  for (let addon of gMockAddons) {
    await openDetailsView(win, addon.id);

    const addonCard = getAddonCard(win, addon.id);
    let el = addonCard.querySelector("[action=update-check]");
    is_element_visible(el, "Check for Updates action is visible");

    let warningMessageBar = await BrowserTestUtils.waitForCondition(() => {
      return addonCard.querySelector(
        "moz-message-bar.addon-card-message[type=warning]"
      );
    }, "Wait for the addon card message to be updated");
    is_element_visible(warningMessageBar, "Warning notification is visible");

    await switchView(win, "plugin");
  }

  await closeView(win);
});

add_task(async function testInstalled() {
  let win = await loadInitialView("plugin");

  for (let addon of gMockAddons) {
    gPrefs.setIntPref(
      getKey(GMPPrefs.KEY_PLUGIN_LAST_UPDATE, addon.id),
      TEST_DATE.getTime()
    );
    gPrefs.setBoolPref(getKey(GMPPrefs.KEY_PLUGIN_AUTOUPDATE, addon.id), false);
    gPrefs.setCharPref(
      getKey(GMPPrefs.KEY_PLUGIN_VERSION, addon.id),
      "1.2.3.4"
    );

    let item = getAddonCard(win, addon.id);
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

  await closeView(win);
});

add_task(async function testInstalledDetails() {
  let win = await loadInitialView("plugin");

  for (let addon of gMockAddons) {
    await openDetailsView(win, addon.id);

    let card = getAddonCard(win, addon.id);
    ok(card, "Got add-on element:" + addon.id);

    is_element_visible(
      card.querySelector("[action=update-check]"),
      "Find updates link is visible"
    );

    await switchView(win, "plugin");
  }

  await closeView(win);
});

add_task(async function testInstalledGlobalEmeDisabled() {
  let win = await loadInitialView("plugin");
  gPrefs.setBoolPref(GMPPrefs.KEY_EME_ENABLED, false);

  for (let addon of gMockAddons) {
    let item = getAddonCard(win, addon.id);
    if (addon.isEME) {
      is(item.parentNode.getAttribute("section"), "1", "Should be disabled");
    } else {
      Assert.ok(item, "Got add-on element.");
    }
  }

  gPrefs.setBoolPref(GMPPrefs.KEY_EME_ENABLED, true);
  await closeView(win);
});

add_task(async function testPreferencesButton() {
  let prefValues = [
    { enabled: false, version: "" },
    { enabled: false, version: "1.2.3.4" },
    { enabled: true, version: "" },
    { enabled: true, version: "1.2.3.4" },
  ];

  for (let preferences of prefValues) {
    info(
      "Testing preferences button with pref settings: " +
        JSON.stringify(preferences)
    );
    for (let addon of gMockAddons) {
      let win = await loadInitialView("plugin");
      gPrefs.setCharPref(
        getKey(GMPPrefs.KEY_PLUGIN_VERSION, addon.id),
        preferences.version
      );
      gPrefs.setBoolPref(
        getKey(GMPPrefs.KEY_PLUGIN_ENABLED, addon.id),
        preferences.enabled
      );

      let item = getAddonCard(win, addon.id);

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

      await waitForViewLoad(win);

      item = getAddonCard(win, addon.id);
      ok(item, "The right view is loaded");

      await closeView(win);
    }
  }
});

add_task(async function testUpdateButton() {
  gPrefs.clearUserPref(GMPPrefs.KEY_UPDATE_LAST_CHECK);

  // The GMPInstallManager constructor has an empty body,
  // so replacing the prototype is safe.
  let originalInstallManager = GMPInstallManager.prototype;
  GMPInstallManager.prototype = MockGMPInstallManagerPrototype;

  let win = await loadInitialView("plugin");

  for (let addon of gMockAddons) {
    let item = getAddonCard(win, addon.id);

    gInstalledAddonId = "";
    gInstallDeferred = Promise.withResolvers();

    let loaded = waitForViewLoad(win);
    item.querySelector("[action=expand]").click();
    await loaded;
    let detail = getAddonCard(win, addon.id);
    detail.querySelector("[action=update-check]").click();

    await gInstallDeferred.promise;
    Assert.equal(gInstalledAddonId, addon.id);

    await switchView(win, "plugin");
  }

  GMPInstallManager.prototype = originalInstallManager;

  await closeView(win);
});

add_task(async function testEmeSupport() {
  for (let addon of gMockAddons) {
    gPrefs.clearUserPref(getKey(GMPPrefs.KEY_PLUGIN_FORCE_SUPPORTED, addon.id));
  }

  let win = await loadInitialView("plugin");

  for (let addon of gMockAddons) {
    let item = getAddonCard(win, addon.id);
    if (addon.id == WIDEVINE_L1_ID) {
      if (
        AppConstants.MOZ_WMF_CDM &&
        AppConstants.platform == "win" &&
        UpdateUtils.ABI.match(/x64/)
      ) {
        Assert.ok(item, "Widevine L1 supported, found add-on element.");
      } else {
        Assert.ok(
          !item,
          "Widevine L1 not supported, couldn't find add-on element."
        );
      }
    } else if (addon.id == WIDEVINE_L3_ID) {
      if (
        AppConstants.platform == "win" ||
        AppConstants.platform == "macosx" ||
        AppConstants.platform == "linux"
      ) {
        Assert.ok(item, "Widevine L3 supported, found add-on element.");
      } else {
        Assert.ok(
          !item,
          "Widevine L3 not supported, couldn't find add-on element."
        );
      }
    } else {
      Assert.ok(item, "Found add-on element.");
    }
  }

  await closeView(win);

  for (let addon of gMockAddons) {
    gPrefs.setBoolPref(getKey(GMPPrefs.KEY_PLUGIN_VISIBLE, addon.id), true);
    gPrefs.setBoolPref(
      getKey(GMPPrefs.KEY_PLUGIN_FORCE_SUPPORTED, addon.id),
      true
    );
  }
});
