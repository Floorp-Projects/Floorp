/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");
var {AddonTestUtils} = Cu.import("resource://testing-common/AddonManagerTesting.jsm", {});
var GMPScope = Cu.import("resource://gre/modules/addons/GMPProvider.jsm");

const TEST_DATE = new Date(2013, 0, 1, 12);

var gManagerWindow;
var gCategoryUtilities;
var gIsEnUsLocale;

var gMockAddons = [];

for (let plugin of GMPScope.GMP_PLUGINS) {
  let mockAddon = Object.freeze({
      id: plugin.id,
      isValid: true,
      isInstalled: false,
      isEME: (plugin.id == "gmp-widevinecdm" ||
              plugin.id.indexOf("gmp-eme-") == 0) ? true : false,
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
  checkForAddons: () => Promise.resolve(gMockAddons),

  installAddon: addon => {
    gInstalledAddonId = addon.id;
    gInstallDeferred.resolve();
    return Promise.resolve();
  },
};

var gOptionsObserver = {
  lastDisplayed: null,
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == AddonManager.OPTIONS_NOTIFICATION_DISPLAYED) {
      this.lastDisplayed = aData;
    }
  }
};

function getInstallItem() {
  let doc = gManagerWindow.document;
  let list = doc.getElementById("addon-list");

  let node = list.firstChild;
  while (node) {
    if (node.getAttribute("status") == "installing") {
      return node;
    }
    node = node.nextSibling;
  }

  return null;
}

function openDetailsView(aId) {
  let view = get_current_view(gManagerWindow);
  Assert.equal(view.id, "list-view", "Should be in the list view to use this function");

  let item = get_addon_element(gManagerWindow, aId);
  Assert.ok(item, "Should have got add-on element.");
  is_element_visible(item, "Add-on element should be visible.");

  item.scrollIntoView();
  EventUtils.synthesizeMouseAtCenter(item, { clickCount: 1 }, gManagerWindow);
  EventUtils.synthesizeMouseAtCenter(item, { clickCount: 2 }, gManagerWindow);

  let deferred = Promise.defer();
  wait_for_view_load(gManagerWindow, deferred.resolve);
  return deferred.promise;
}

add_task(function* initializeState() {
  gPrefs.setBoolPref(GMPScope.GMPPrefs.KEY_LOGGING_DUMP, true);
  gPrefs.setIntPref(GMPScope.GMPPrefs.KEY_LOGGING_LEVEL, 0);

  gManagerWindow = yield open_manager();
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  registerCleanupFunction(Task.async(function*() {
    Services.obs.removeObserver(gOptionsObserver, AddonManager.OPTIONS_NOTIFICATION_DISPLAYED);

    for (let addon of gMockAddons) {
      gPrefs.clearUserPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_ENABLED, addon.id));
      gPrefs.clearUserPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_LAST_UPDATE, addon.id));
      gPrefs.clearUserPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_AUTOUPDATE, addon.id));
      gPrefs.clearUserPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_VERSION, addon.id));
      gPrefs.clearUserPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_VISIBLE, addon.id));
    }
    gPrefs.clearUserPref(GMPScope.GMPPrefs.KEY_LOGGING_DUMP);
    gPrefs.clearUserPref(GMPScope.GMPPrefs.KEY_LOGGING_LEVEL);
    gPrefs.clearUserPref(GMPScope.GMPPrefs.KEY_UPDATE_LAST_CHECK);
    gPrefs.clearUserPref(GMPScope.GMPPrefs.KEY_EME_ENABLED);
    yield GMPScope.GMPProvider.shutdown();
    GMPScope.GMPProvider.startup();
  }));

  let chrome = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIXULChromeRegistry);
  gIsEnUsLocale = chrome.getSelectedLocale("global") == "en-US";

  Services.obs.addObserver(gOptionsObserver, AddonManager.OPTIONS_NOTIFICATION_DISPLAYED, false);

  // Start out with plugins not being installed, disabled and automatic updates
  // disabled.
  gPrefs.setBoolPref(GMPScope.GMPPrefs.KEY_EME_ENABLED, true);
  for (let addon of gMockAddons) {
    gPrefs.setBoolPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_ENABLED, addon.id), false);
    gPrefs.setIntPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_LAST_UPDATE, addon.id), 0);
    gPrefs.setBoolPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_AUTOUPDATE, addon.id), false);
    gPrefs.setCharPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_VERSION, addon.id), "");
    gPrefs.setBoolPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_VISIBLE, addon.id),
                       true);
  }
  yield GMPScope.GMPProvider.shutdown();
  GMPScope.GMPProvider.startup();
});

add_task(function* testNotInstalledDisabled() {
  Assert.ok(gCategoryUtilities.isTypeVisible("plugin"), "Plugin tab visible.");
  yield gCategoryUtilities.openType("plugin");

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

add_task(function* testNotInstalledDisabledDetails() {
  for (let addon of gMockAddons) {
    yield openDetailsView(addon.id);
    let doc = gManagerWindow.document;

    let el = doc.getElementsByClassName("disabled-postfix")[0];
    is_element_visible(el, "disabled-postfix is visible.");
    el = doc.getElementById("detail-findUpdates-btn");
    is_element_visible(el, "Find updates link is visible.");
    el = doc.getElementById("detail-warning");
    is_element_hidden(el, "Warning notification is hidden.");
    el = doc.getElementsByTagName("setting")[0];

    yield gCategoryUtilities.openType("plugin");
  }
});

add_task(function* testNotInstalled() {
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

add_task(function* testNotInstalledDetails() {
  for (let addon of gMockAddons) {
    yield openDetailsView(addon.id);
    let doc = gManagerWindow.document;

    let el = doc.getElementsByClassName("disabled-postfix")[0];
    is_element_hidden(el, "disabled-postfix is hidden.");
    el = doc.getElementById("detail-findUpdates-btn");
    is_element_visible(el, "Find updates link is visible.");
    el = doc.getElementById("detail-warning");
    is_element_visible(el, "Warning notification is visible.");
    el = doc.getElementsByTagName("setting")[0];

    yield gCategoryUtilities.openType("plugin");
  }
});

add_task(function* testInstalled() {
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

add_task(function* testInstalledDetails() {
  for (let addon of gMockAddons) {
    yield openDetailsView(addon.id);
    let doc = gManagerWindow.document;

    let el = doc.getElementsByClassName("disabled-postfix")[0];
    is_element_hidden(el, "disabled-postfix is hidden.");
    el = doc.getElementById("detail-findUpdates-btn");
    is_element_visible(el, "Find updates link is visible.");
    el = doc.getElementById("detail-warning");
    is_element_hidden(el, "Warning notification is hidden.");
    el = doc.getElementsByTagName("setting")[0];

    let contextMenu = doc.getElementById("addonitem-popup");
    let deferred = Promise.defer();
    let listener = () => {
      contextMenu.removeEventListener("popupshown", listener, false);
      deferred.resolve();
    };
    contextMenu.addEventListener("popupshown", listener, false);
    el = doc.getElementsByClassName("detail-view-container")[0];
    EventUtils.synthesizeMouse(el, 4, 4, { }, gManagerWindow);
    EventUtils.synthesizeMouse(el, 4, 4, { type: "contextmenu", button: 2 }, gManagerWindow);
    yield deferred.promise;
    let menuSep = doc.getElementById("addonitem-menuseparator");
    is_element_hidden(menuSep, "Menu separator is hidden.");
    contextMenu.hidePopup();

    yield gCategoryUtilities.openType("plugin");
  }
});

add_task(function* testInstalledGlobalEmeDisabled() {
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

add_task(function* testPreferencesButton() {

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
      yield close_manager(gManagerWindow);
      gManagerWindow = yield open_manager();
      gCategoryUtilities = new CategoryUtilities(gManagerWindow);
      gPrefs.setCharPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_VERSION, addon.id),
                         preferences.version);
      gPrefs.setBoolPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_ENABLED, addon.id),
                         preferences.enabled);

      yield gCategoryUtilities.openType("plugin");
      let doc = gManagerWindow.document;
      let item = get_addon_element(gManagerWindow, addon.id);

      let button = doc.getAnonymousElementByAttribute(item, "anonid", "preferences-btn");
      EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);
      let deferred = Promise.defer();
      wait_for_view_load(gManagerWindow, deferred.resolve);
      yield deferred.promise;

      is(gOptionsObserver.lastDisplayed, addon.id);
    }
  }
});

add_task(function* testUpdateButton() {
  gPrefs.clearUserPref(GMPScope.GMPPrefs.KEY_UPDATE_LAST_CHECK);

  let originalInstallManager = GMPScope.GMPInstallManager;
  Object.defineProperty(GMPScope, "GMPInstallManager", {
    value: MockGMPInstallManager,
    writable: true,
    enumerable: true,
    configurable: true
  });

  for (let addon of gMockAddons) {
    yield gCategoryUtilities.openType("plugin");
    let doc = gManagerWindow.document;
    let item = get_addon_element(gManagerWindow, addon.id);

    gInstalledAddonId = "";
    gInstallDeferred = Promise.defer();

    let button = doc.getAnonymousElementByAttribute(item, "anonid", "preferences-btn");
    EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);
    let deferred = Promise.defer();
    wait_for_view_load(gManagerWindow, deferred.resolve);
    yield deferred.promise;

    button = doc.getElementById("detail-findUpdates-btn");
    Assert.ok(button != null, "Got detail-findUpdates-btn");
    EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);
    yield gInstallDeferred.promise;

    Assert.equal(gInstalledAddonId, addon.id);
  }
  Object.defineProperty(GMPScope, "GMPInstallManager", {
    value: originalInstallManager,
    writable: true,
    enumerable: true,
    configurable: true
  });
});

add_task(function* testEmeSupport() {
  for (let addon of gMockAddons) {
    gPrefs.clearUserPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_VISIBLE, addon.id));
  }
  yield GMPScope.GMPProvider.shutdown();
  GMPScope.GMPProvider.startup();

  for (let addon of gMockAddons) {
    yield gCategoryUtilities.openType("plugin");
    let doc = gManagerWindow.document;
    let item = get_addon_element(gManagerWindow, addon.id);
    if (addon.id == GMPScope.EME_ADOBE_ID) {
      if (Services.appinfo.OS == "WINNT") {
        Assert.ok(item, "Adobe EME supported, found add-on element.");
      } else {
        Assert.ok(!item,
                  "Adobe EME not supported, couldn't find add-on element.");
      }
    } else if (addon.id == GMPScope.WIDEVINE_ID) {
      if (AppConstants.isPlatformAndVersionAtLeast("win", "6") ||
          AppConstants.isPlatformAndVersionAtLeast("macosx", "10.7")) {
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
    gPrefs.setBoolPref(getKey(GMPScope.GMPPrefs.KEY_PLUGIN_VISIBLE, addon.id),
                       true);
  }
  yield GMPScope.GMPProvider.shutdown();
  GMPScope.GMPProvider.startup();

});

add_task(function* test_cleanup() {
  yield close_manager(gManagerWindow);
});
