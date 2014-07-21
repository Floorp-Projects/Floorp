/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

let {AddonTestUtils} = Components.utils.import("resource://testing-common/AddonManagerTesting.jsm", {});

const OPENH264_PLUGIN_ID       = "gmp-gmpopenh264";
const OPENH264_PREF_BRANCH     = "media." + OPENH264_PLUGIN_ID + ".";
const OPENH264_PREF_ENABLED    = OPENH264_PREF_BRANCH + "enabled";
const OPENH264_PREF_PATH       = OPENH264_PREF_BRANCH + "path";
const OPENH264_PREF_VERSION    = OPENH264_PREF_BRANCH + "version";
const OPENH264_PREF_LASTUPDATE = OPENH264_PREF_BRANCH + "lastUpdate";
const OPENH264_PREF_AUTOUPDATE = OPENH264_PREF_BRANCH + "autoupdate";
const PREF_LOGGING             = OPENH264_PREF_BRANCH + "provider.logging";
const PREF_LOGGING_LEVEL       = PREF_LOGGING + ".level";
const PREF_LOGGING_DUMP        = PREF_LOGGING + ".dump";

const TEST_DATE = new Date(2013, 0, 1, 12);

let gManagerWindow;
let gCategoryUtilities;
let gIsEnUsLocale;

let gOptionsObserver = {
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
  let item = get_addon_element(gManagerWindow, aId);
  Assert.ok(item, "Should have got add-on element.");
  is_element_visible(item, "Add-on element should be visible.");

  EventUtils.synthesizeMouseAtCenter(item, { clickCount: 1 }, gManagerWindow);
  EventUtils.synthesizeMouseAtCenter(item, { clickCount: 2 }, gManagerWindow);

  let deferred = Promise.defer();
  wait_for_view_load(gManagerWindow, deferred.resolve);
  return deferred.promise;
}

add_task(function* initializeState() {
  Services.prefs.setBoolPref(PREF_LOGGING_DUMP, true);
  Services.prefs.setIntPref(PREF_LOGGING_LEVEL, 0);

  gManagerWindow = yield open_manager();
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  registerCleanupFunction(() => {
    Services.obs.removeObserver(gOptionsObserver, AddonManager.OPTIONS_NOTIFICATION_DISPLAYED);

    Services.prefs.clearUserPref(OPENH264_PREF_ENABLED);
    Services.prefs.clearUserPref(OPENH264_PREF_PATH);
    Services.prefs.clearUserPref(OPENH264_PREF_VERSION);
    Services.prefs.clearUserPref(OPENH264_PREF_LASTUPDATE);
    Services.prefs.clearUserPref(OPENH264_PREF_AUTOUPDATE);
    Services.prefs.clearUserPref(PREF_LOGGING_DUMP);
    Services.prefs.clearUserPref(PREF_LOGGING_LEVEL);
  });

  let chrome = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIXULChromeRegistry);
  gIsEnUsLocale = chrome.getSelectedLocale("global") == "en-US";

  Services.obs.addObserver(gOptionsObserver, AddonManager.OPTIONS_NOTIFICATION_DISPLAYED, false);

  // Start out with OpenH264 not being installed, disabled and automatic updates disabled.
  Services.prefs.setBoolPref(OPENH264_PREF_ENABLED, false);
  Services.prefs.setCharPref(OPENH264_PREF_VERSION, "");
  Services.prefs.setCharPref(OPENH264_PREF_LASTUPDATE, "");
  Services.prefs.setBoolPref(OPENH264_PREF_AUTOUPDATE, false);
  Services.prefs.setCharPref(OPENH264_PREF_PATH, "");
});

add_task(function* testNotInstalled() {
  Assert.ok(gCategoryUtilities.isTypeVisible("plugin"), "Plugin tab visible.");
  yield gCategoryUtilities.openType("plugin");

  let item = get_addon_element(gManagerWindow, OPENH264_PLUGIN_ID);
  Assert.ok(item, "Got add-on element.");
  item.parentNode.ensureElementIsVisible(item);
  is(item.getAttribute("active"), "false");

  let el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "warning");
  is_element_visible(el, "Warning notification is visible.");
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
});

add_task(function* testNotInstalledDetails() {
  yield openDetailsView(OPENH264_PLUGIN_ID);
  let doc = gManagerWindow.document;

  let el = doc.getElementsByClassName("disabled-postfix")[0];
  is_element_visible(el, "disabled-postfix is visible.");
  el = doc.getElementById("detail-findUpdates-btn");
  is_element_visible(el, "Find updates link is visible.");
  el = doc.getElementById("detail-warning");
  is_element_visible(el, "Warning notification is visible.");
  el = doc.getElementsByTagName("setting")[0];
});

add_task(function* testInstalled() {
  Services.prefs.setBoolPref(OPENH264_PREF_ENABLED, true);
  Services.prefs.setBoolPref(OPENH264_PREF_VERSION, "1.2.3.4");
  Services.prefs.setBoolPref(OPENH264_PREF_LASTUPDATE, "" + TEST_DATE.getTime());
  Services.prefs.setBoolPref(OPENH264_PREF_AUTOUPDATE, false);
  Services.prefs.setCharPref(OPENH264_PREF_PATH, "foo/bar");

  yield gCategoryUtilities.openType("plugin");

  let item = get_addon_element(gManagerWindow, OPENH264_PLUGIN_ID);
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
});

add_task(function* testInstalledDetails() {
  yield openDetailsView(OPENH264_PLUGIN_ID);
  let doc = gManagerWindow.document;

  let el = doc.getElementsByClassName("disabled-postfix")[0];
  is_element_hidden(el, "disabled-postfix is hidden.");
  el = doc.getElementById("detail-findUpdates-btn");
  is_element_visible(el, "Find updates link is visible.");
  el = doc.getElementById("detail-warning");
  is_element_hidden(el, "Warning notification is hidden.");
  el = doc.getElementsByTagName("setting")[0];
});

add_task(function* testPreferencesButton() {
  let file = Services.dirsvc.get("ProfD", Ci.nsIFile);
  file.append("openh264");
  file.append("testDir");

  let prefValues = [
    { enabled: false, path: "" },
    { enabled: false, path: file.path },
    { enabled: true, path: "" },
    { enabled: true, path: file.path },
  ];

  for (let prefs of prefValues) {
    dump("Testing preferences button with pref settings: " + JSON.stringify(prefs) + "\n");
    Services.prefs.setCharPref(OPENH264_PREF_PATH, prefs.path);
    Services.prefs.setBoolPref(OPENH264_PREF_ENABLED, prefs.enabled);

    yield gCategoryUtilities.openType("plugin");
    let doc = gManagerWindow.document;
    let item = get_addon_element(gManagerWindow, OPENH264_PLUGIN_ID);

    let button = doc.getAnonymousElementByAttribute(item, "anonid", "preferences-btn");
    EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);
    let deferred = Promise.defer();
    wait_for_view_load(gManagerWindow, deferred.resolve);
    yield deferred.promise;

    is(gOptionsObserver.lastDisplayed, OPENH264_PLUGIN_ID);
  }
});

add_task(function* test_cleanup() {
  yield close_manager(gManagerWindow);
});
