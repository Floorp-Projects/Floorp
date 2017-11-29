/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");
var GMPScope = Cu.import("resource://gre/modules/addons/GMPProvider.jsm", {});

const TEST_DATE = new Date(2013, 0, 1, 12);

var gManagerWindow;
var gCategoryUtilities;

var gMockAddons = [];

for (let plugin of GMPScope.GMP_PLUGINS) {
  let mockAddon = Object.freeze({
    id: plugin.id,
    isValid: true,
    isInstalled: false,
    isEME: !!(plugin.id == "gmp-widevinecdm" || plugin.id.indexOf("gmp-eme-") == 0),
  });
  gMockAddons.push(mockAddon);
}

var gInstalledAddonId = "";
var gInstallDeferred = null;
var gPrefs = Services.prefs;
var getKey = GMPScope.GMPPrefs.getPrefKey;

function MockGMPInstallManager() {
}

MockGMPInstallManager.prototype = {
  checkForAddons: () => Promise.resolve({
    usedFallback: true,
    gmpAddons: gMockAddons
  }),

  installAddon: addon => {
    gInstalledAddonId = addon.id;
    gInstallDeferred.resolve();
    return Promise.resolve();
  },
};

function openDetailsView(aId) {
  let view = get_current_view(gManagerWindow);
  Assert.equal(view.id, "list-view", "Should be in the list view to use this function");

  let item = get_addon_element(gManagerWindow, aId);
  Assert.ok(item, "Should have got add-on element.");
  is_element_visible(item, "Add-on element should be visible.");

  item.scrollIntoView();
  EventUtils.synthesizeMouseAtCenter(item, { clickCount: 1 }, gManagerWindow);
  EventUtils.synthesizeMouseAtCenter(item, { clickCount: 2 }, gManagerWindow);

  return new Promise(resolve => {
    wait_for_view_load(gManagerWindow, resolve);
  });
}

add_task(async function initializeState() {
  gPrefs.setBoolPref(GMPScope.GMPPrefs.KEY_LOGGING_DUMP, true);
  gPrefs.setIntPref(GMPScope.GMPPrefs.KEY_LOGGING_LEVEL, 0);

  gManagerWindow = await open_manager();
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  registerCleanupFunction(async function() {
    for (let addon of gMockAddons) {
      gPrefs.clearUserPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_ENABLED, addon.id));
      gPrefs.clearUserPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_LAST_UPDATE, addon.id));
      gPrefs.clearUserPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_AUTOUPDATE, addon.id));
      gPrefs.clearUserPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_VERSION, addon.id));
      gPrefs.clearUserPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_VISIBLE, addon.id));
      gPrefs.clearUserPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_FORCE_SUPPORTED, addon.id));
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
    gPrefs.setBoolPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_ENABLED, addon.id), false);
    gPrefs.setIntPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_LAST_UPDATE, addon.id), 0);
    gPrefs.setBoolPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_AUTOUPDATE, addon.id), false);
    gPrefs.setCharPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_VERSION, addon.id), "");
    gPrefs.setBoolPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_VISIBLE, addon.id), true);
    gPrefs.setBoolPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_FORCE_SUPPORTED, addon.id), true);
  }
  await GMPScope.GMPProvider.shutdown();
  GMPScope.GMPProvider.startup();
});

add_task(async function testNotInstalledDisabled() {
  Assert.ok(gCategoryUtilities.isTypeVisible("plugin"), "Plugin tab visible.");
  await gCategoryUtilities.openType("plugin");

  for (let addon of gMockAddons) {
    let item = get_addon_element(gManagerWindow, addon.id);
    Assert.ok(item, "Got add-on element:" + addon.id);
    item.parentNode.ensureElementIsVisible(item);
    is(item.getAttribute("active"), "false");

    let el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "warning");
    is_element_hidden(el, "Warning notification is hidden.");
    el = item.ownerDocument.getAnonymousElementByAttribute(item, "class", "disabled-postfix");
    is_element_visible(el, "disabled-postfix is visible.");
    el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "disable-btn");
    is_element_hidden(el, "Disable button not visible.");
    el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "enable-btn");
    is_element_hidden(el, "Enable button not visible.");

    let menu = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "state-menulist");
    is_element_visible(menu, "State menu should be visible.");

    let neverActivate = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "never-activate-menuitem");
    is(menu.selectedItem, neverActivate, "Plugin state should be never-activate.");
  }
});

add_task(async function testNotInstalledDisabledDetails() {
  for (let addon of gMockAddons) {
    await openDetailsView(addon.id);
    let doc = gManagerWindow.document;

    let el = doc.getElementsByClassName("disabled-postfix")[0];
    is_element_visible(el, "disabled-postfix is visible.");
    el = doc.getElementById("detail-findUpdates-btn");
    is_element_visible(el, "Find updates link is visible.");
    el = doc.getElementById("detail-warning");
    is_element_hidden(el, "Warning notification is hidden.");
    el = doc.getElementsByTagName("setting")[0];

    await gCategoryUtilities.openType("plugin");
  }
});

add_task(async function testNotInstalled() {
  for (let addon of gMockAddons) {
    gPrefs.setBoolPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_ENABLED, addon.id), true);
    let item = get_addon_element(gManagerWindow, addon.id);
    Assert.ok(item, "Got add-on element:" + addon.id);
    item.parentNode.ensureElementIsVisible(item);
    is(item.getAttribute("active"), "true");

    let el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "warning");
    is_element_visible(el, "Warning notification is visible.");
    el = item.ownerDocument.getAnonymousElementByAttribute(item, "class", "disabled-postfix");
    is_element_hidden(el, "disabled-postfix is hidden.");
    el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "disable-btn");
    is_element_hidden(el, "Disable button not visible.");
    el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "enable-btn");
    is_element_hidden(el, "Enable button not visible.");

    let menu = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "state-menulist");
    is_element_visible(menu, "State menu should be visible.");

    let alwaysActivate = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "always-activate-menuitem");
    is(menu.selectedItem, alwaysActivate, "Plugin state should be always-activate.");
  }
});

add_task(async function testNotInstalledDetails() {
  for (let addon of gMockAddons) {
    await openDetailsView(addon.id);
    let doc = gManagerWindow.document;

    let el = doc.getElementsByClassName("disabled-postfix")[0];
    is_element_hidden(el, "disabled-postfix is hidden.");
    el = doc.getElementById("detail-findUpdates-btn");
    is_element_visible(el, "Find updates link is visible.");
    el = doc.getElementById("detail-warning");
    is_element_visible(el, "Warning notification is visible.");
    el = doc.getElementsByTagName("setting")[0];

    await gCategoryUtilities.openType("plugin");
  }
});

add_task(async function testInstalled() {
  for (let addon of gMockAddons) {
    gPrefs.setIntPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_LAST_UPDATE, addon.id),
                      TEST_DATE.getTime());
    gPrefs.setBoolPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_AUTOUPDATE, addon.id), false);
    gPrefs.setCharPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_VERSION, addon.id), "1.2.3.4");

    let item = get_addon_element(gManagerWindow, addon.id);
    Assert.ok(item, "Got add-on element.");
    item.parentNode.ensureElementIsVisible(item);
    is(item.getAttribute("active"), "true");

    let el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "warning");
    is_element_hidden(el, "Warning notification is hidden.");
    el = item.ownerDocument.getAnonymousElementByAttribute(item, "class", "disabled-postfix");
    is_element_hidden(el, "disabled-postfix is hidden.");
    el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "disable-btn");
    is_element_hidden(el, "Disable button not visible.");
    el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "enable-btn");
    is_element_hidden(el, "Enable button not visible.");

    let menu = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "state-menulist");
    is_element_visible(menu, "State menu should be visible.");

    let alwaysActivate = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "always-activate-menuitem");
    is(menu.selectedItem, alwaysActivate, "Plugin state should be always-activate.");
  }
});

add_task(async function testInstalledDetails() {
  for (let addon of gMockAddons) {
    await openDetailsView(addon.id);
    let doc = gManagerWindow.document;

    let el = doc.getElementsByClassName("disabled-postfix")[0];
    is_element_hidden(el, "disabled-postfix is hidden.");
    el = doc.getElementById("detail-findUpdates-btn");
    is_element_visible(el, "Find updates link is visible.");
    el = doc.getElementById("detail-warning");
    is_element_hidden(el, "Warning notification is hidden.");
    el = doc.getElementsByTagName("setting")[0];

    let contextMenu = doc.getElementById("addonitem-popup");
    await new Promise(resolve => {
      let listener = () => {
        contextMenu.removeEventListener("popupshown", listener);
        resolve();
      };
      contextMenu.addEventListener("popupshown", listener);
      el = doc.getElementsByClassName("detail-view-container")[0];
      EventUtils.synthesizeMouse(el, 4, 4, { }, gManagerWindow);
      EventUtils.synthesizeMouse(el, 4, 4, { type: "contextmenu", button: 2 }, gManagerWindow);
    });
    let menuSep = doc.getElementById("addonitem-menuseparator");
    is_element_hidden(menuSep, "Menu separator is hidden.");
    contextMenu.hidePopup();

    await gCategoryUtilities.openType("plugin");
  }
});

add_task(async function testInstalledGlobalEmeDisabled() {
  gPrefs.setBoolPref(GMPScope.GMPPrefs.KEY_EME_ENABLED, false);
  for (let addon of gMockAddons) {
    let item = get_addon_element(gManagerWindow, addon.id);
    if (addon.isEME) {
      Assert.ok(!item, "Couldn't get add-on element.");
    } else {
      Assert.ok(item, "Got add-on element.");
    }
  }
  gPrefs.setBoolPref(GMPScope.GMPPrefs.KEY_EME_ENABLED, true);
});

add_task(async function testPreferencesButton() {

  let prefValues = [
    { enabled: false, version: "" },
    { enabled: false, version: "1.2.3.4" },
    { enabled: true, version: "" },
    { enabled: true, version: "1.2.3.4" },
  ];

  for (let preferences of prefValues) {
    dump("Testing preferences button with pref settings: " +
         JSON.stringify(preferences) + "\n");
    for (let addon of gMockAddons) {
      await close_manager(gManagerWindow);
      gManagerWindow = await open_manager();
      gCategoryUtilities = new CategoryUtilities(gManagerWindow);
      gPrefs.setCharPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_VERSION, addon.id),
                         preferences.version);
      gPrefs.setBoolPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_ENABLED, addon.id),
                         preferences.enabled);

      await gCategoryUtilities.openType("plugin");
      let doc = gManagerWindow.document;
      let item = get_addon_element(gManagerWindow, addon.id);

      let button = doc.getAnonymousElementByAttribute(item, "anonid", "preferences-btn");
      is_element_visible(button);
      EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);
      await new Promise(resolve => {
        wait_for_view_load(gManagerWindow, resolve);
      });
    }
  }
});

add_task(async function testUpdateButton() {
  gPrefs.clearUserPref(GMPScope.GMPPrefs.KEY_UPDATE_LAST_CHECK);

  let originalInstallManager = GMPScope.GMPInstallManager;
  Object.defineProperty(GMPScope, "GMPInstallManager", {
    value: MockGMPInstallManager,
    writable: true,
    enumerable: true,
    configurable: true
  });

  for (let addon of gMockAddons) {
    await gCategoryUtilities.openType("plugin");
    let doc = gManagerWindow.document;
    let item = get_addon_element(gManagerWindow, addon.id);

    gInstalledAddonId = "";
    gInstallDeferred = Promise.defer();

    let button = doc.getAnonymousElementByAttribute(item, "anonid", "preferences-btn");
    EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);
    await new Promise(resolve => {
      wait_for_view_load(gManagerWindow, resolve);
    });

    button = doc.getElementById("detail-findUpdates-btn");
    Assert.ok(button != null, "Got detail-findUpdates-btn");
    EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);
    await gInstallDeferred.promise;

    Assert.equal(gInstalledAddonId, addon.id);
  }
  Object.defineProperty(GMPScope, "GMPInstallManager", {
    value: originalInstallManager,
    writable: true,
    enumerable: true,
    configurable: true
  });
});

add_task(async function testEmeSupport() {
  for (let addon of gMockAddons) {
    gPrefs.clearUserPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_FORCE_SUPPORTED, addon.id));
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
        Assert.ok(!item,
                  "Adobe EME not supported, couldn't find add-on element.");
      }
    } else if (addon.id == GMPScope.WIDEVINE_ID) {
      if (AppConstants.isPlatformAndVersionAtLeast("win", "6") ||
          AppConstants.platform == "macosx" ||
          AppConstants.platform == "linux") {
        Assert.ok(item, "Widevine supported, found add-on element.");
      } else {
        Assert.ok(!item,
                  "Widevine not supported, couldn't find add-on element.");
      }
    } else {
      Assert.ok(item, "Found add-on element.");
    }
  }

  for (let addon of gMockAddons) {
    gPrefs.setBoolPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_VISIBLE, addon.id), true);
    gPrefs.setBoolPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_FORCE_SUPPORTED, addon.id), true);
  }
  await GMPScope.GMPProvider.shutdown();
  GMPScope.GMPProvider.startup();

});

add_task(async function test_cleanup() {
  await close_manager(gManagerWindow);
});
