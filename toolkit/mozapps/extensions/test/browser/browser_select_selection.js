/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests the selection part of the post-app-update dialog

var gProvider;
var gWin;

const PROFILE = AddonManager.SCOPE_PROFILE;
const USER = AddonManager.SCOPE_USER;
const APP = AddonManager.SCOPE_APPLICATION;
const SYSTEM = AddonManager.SCOPE_SYSTEM;

// The matrix of testcases for the selection part of the UI
// Note that the isActive flag has the value it had when the previous version
// of the application ran with this add-on.
var ADDONS = [
  //userDisabled   wasAppDisabled isAppDisabled  isActive  hasUpdate  autoUpdate  scope    defaultKeep  position  keepString           disableString
  [false,          true,          false,         false,    false,     true,       PROFILE, true,        38,      "enabled",           ""],               // 0
  [false,          true,          false,         false,    true,      true,       PROFILE, true,        39,       "enabled",           ""],               // 1
  [false,          true,          false,         false,    true,      false,      PROFILE, true,        48,       "unneededupdate",    ""],               // 2
  [false,          false,         false,         true,     false,     true,       PROFILE, true,        53,       "",                  "disabled"],       // 3
  [false,          false,         false,         true,     true,      true,       PROFILE, true,        54,       "",                  "disabled"],       // 4
  [false,          false,         false,         true,     true,      false,      PROFILE, true,        55,       "unneededupdate",    "disabled"],       // 5
  [false,          true,          true,          false,    false,     true,       PROFILE, true,        56,       "incompatible",      ""],               // 6
  [false,          true,          true,          false,    true,      true,       PROFILE, true,        57,       "autoupdate",        ""],               // 7
  [false,          true,          true,          false,    true,      false,      PROFILE, true,        58,       "neededupdate",      ""],               // 8
  [false,          false,         true,          true,     false,     true,       PROFILE, true,        59,       "incompatible",      "disabled"],       // 9
  [false,          true,          true,          true,     true,      true,       PROFILE, true,        40,       "autoupdate",        "disabled"],       // 10
  [false,          true,          true,          true,     true,      false,      PROFILE, true,        41,       "neededupdate",      "disabled"],       // 11
  [true,           false,         false,         false,    false,     true,       PROFILE, false,       42,       "enabled",           ""],               // 12
  [true,           false,         false,         false,    true,      true,       PROFILE, false,       43,       "enabled",           ""],               // 13
  [true,           false,         false,         false,    true,      false,      PROFILE, false,       44,       "unneededupdate",    ""],               // 14

  // userDisabled and isActive cannot be true on startup

  [true,           true,          true,          false,    false,     true,       PROFILE, false,       45,       "incompatible",      ""],               // 15
  [true,           true,          true,          false,    true,      true,       PROFILE, false,       46,       "autoupdate",        ""],               // 16
  [true,           true,          true,          false,    true,      false,      PROFILE, false,       47,       "neededupdate",      ""],               // 17

  // userDisabled and isActive cannot be true on startup

  // Being in a different scope should make little difference except no updates are possible so don't exhaustively test each
  [false,          false,         false,         true,     true,      false,      USER,    false,       0,        "",                  "disabled"],       // 18
  [true,           true,          false,         false,    true,      false,      USER,    false,       1,        "enabled",           ""],               // 19
  [false,          true,          true,          true,     true,      false,      USER,    false,       2,        "incompatible",      "disabled"],       // 20
  [true,           true,          true,          false,    true,      false,      USER,    false,       3,        "incompatible",      ""],               // 21
  [false,          false,         false,         true,     true,      false,      SYSTEM,  false,       4,        "",                  "disabled"],       // 22
  [true,           true,          false,         false,    true,      false,      SYSTEM,  false,       5,        "enabled",           ""],               // 23
  [false,          true,          true,          true,     true,      false,      SYSTEM,  false,       6,        "incompatible",      "disabled"],       // 24
  [true,           true,          true,          false,    true,      false,      SYSTEM,  false,       7,        "incompatible",      ""],               // 25
  [false,          false,         false,         true,     true,      false,      APP,     true,        49,       "",                  "disabled"],       // 26
  [true,           true,          false,         false,    true,      false,      APP,     false,       50,       "enabled",           ""],               // 27
  [false,          true,          true,          true,     true,      false,      APP,     true,        51,       "incompatible",      "disabled"],       // 28
  [true,           true,          true,          false,    true,      false,      APP,     false,       52,       "incompatible",      ""],               // 29
];

function waitForView(aView, aCallback) {
  var view = gWin.document.getElementById(aView);
  view.addEventListener("ViewChanged", function() {
    view.removeEventListener("ViewChanged", arguments.callee, false);
    aCallback();
  }, false);
}

function getString(aName) {
  if (!aName)
    return "";

  var strings = Services.strings.createBundle("chrome://mozapps/locale/extensions/selectAddons.properties");
  return strings.GetStringFromName("action." + aName);
}

function test() {
  waitForExplicitFinish();

  gProvider = new MockProvider();

  ADDONS.forEach(function(aAddon, aPos) {
    var addon = new MockAddon("test" + aPos + "@tests.mozilla.org",
                              "Test Add-on " + aPos, "extension");
    addon.version = "1.0";
    addon.userDisabled = aAddon[0];
    addon.appDisabled = aAddon[1];
    addon.isActive = aAddon[3];
    addon.applyBackgroundUpdates = aAddon[5] ? AddonManager.AUTOUPDATE_ENABLE
                                             : AddonManager.AUTOUPDATE_DISABLE;
    addon.scope = aAddon[6];

    // Remove the upgrade permission from non-profile add-ons
    if (addon.scope != AddonManager.SCOPE_PROFILE)
      addon._permissions -= AddonManager.PERM_CAN_UPGRADE;

    addon.findUpdates = function(aListener, aReason, aAppVersion, aPlatformVersion) {
      addon.appDisabled = aAddon[2];
      addon.isActive = addon.shouldBeActive;

      if (aAddon[4]) {
        var newAddon = new MockAddon(this.id, this.name, "extension");
        newAddon.version = "2.0";
        var install = new MockInstall(this.name, this.type, newAddon);
        install.existingAddon = this;
        aListener.onUpdateAvailable(this, install);
      }

      aListener.onUpdateFinished(this, AddonManager.UPDATE_STATUS_NO_ERROR);
    };

    gProvider.addAddon(addon);
  });

  gWin = Services.ww.openWindow(null,
                                "chrome://mozapps/content/extensions/selectAddons.xul",
                                "",
                                "chrome,centerscreen,dialog,titlebar",
                                null);
  waitForFocus(function() {
    waitForView("select", run_next_test);
  }, gWin);
}

function end_test() {
  gWin.close();
  finish();
}

// Minimal test for the checking UI
add_test(function checking_test() {
  // By the time we're here the progress bar should be full
  var progress = gWin.document.getElementById("checking-progress");
  is(progress.mode, "determined", "Should be a determined progress bar");
  is(progress.value, progress.max, "Should be at full progress");

  run_next_test();
});

// Tests that the selection UI behaves correctly
add_test(function selection_test() {
  function check_state() {
    var str = addon[keep.checked ? 9 : 10];
    var expected = getString(str);
    var showCheckbox = str == "neededupdate" || str == "unneededupdate";
    is(action.textContent, expected, "Action message should have the right text");
    is(!is_hidden(update), showCheckbox, "Checkbox should have the right visibility");
    is(is_hidden(action), showCheckbox, "Message should have the right visibility");
    if (showCheckbox)
      ok(update.checked, "Optional update checkbox should be checked");

    if (keep.checked) {
      is(row.hasAttribute("active"), !addon[2] || hasUpdate,
       "Add-on will be active if it isn't appDisabled or an update is available");

      if (showCheckbox) {
        info("Flipping update checkbox");
        EventUtils.synthesizeMouseAtCenter(update, { }, gWin);
        is(row.hasAttribute("active"), str == "unneededupdate",
           "If the optional update isn't needed then the add-on will still be active");

        info("Flipping update checkbox");
        EventUtils.synthesizeMouseAtCenter(update, { }, gWin);
        is(row.hasAttribute("active"), !addon[2] || hasUpdate,
         "Add-on will be active if it isn't appDisabled or an update is available");
      }
    }
    else {
      ok(!row.hasAttribute("active"), "Add-on won't be active when not keeping");

      if (showCheckbox) {
        info("Flipping update checkbox");
        EventUtils.synthesizeMouseAtCenter(update, { }, gWin);
        ok(!row.hasAttribute("active"),
           "Unchecking the update checkbox shouldn't make the add-on active");

        info("Flipping update checkbox");
        EventUtils.synthesizeMouseAtCenter(update, { }, gWin);
        ok(!row.hasAttribute("active"),
           "Re-checking the update checkbox shouldn't make the add-on active");
      }
    }
  }

  is(gWin.document.getElementById("view-deck").selectedPanel.id, "select",
     "Should be on the right view");

  var pos = 0;
  var scrollbox = gWin.document.getElementById("select-scrollbox");
  var scrollBoxObject = scrollbox.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
  for (var row = gWin.document.getElementById("select-rows").firstChild; row; row = row.nextSibling) {
    // Ignore separators but increase the position by a large amount so we
    // can verify they were in the right place
    if (row.localName == "separator") {
      pos += 30;
      continue;
    }

    is(row._addon.type, "extension", "Should only be listing extensions");

    // Ignore non-test add-ons that may be present
    if (row.id.substr(-18) != "@tests.mozilla.org")
      continue;

    var id = parseInt(row.id.substring(4, row.id.length - 18));
    var addon = ADDONS[id];

    info("Testing add-on " + id);
    scrollBoxObject.ensureElementIsVisible(row);
    var keep = gWin.document.getAnonymousElementByAttribute(row, "anonid", "keep");
    var action = gWin.document.getAnonymousElementByAttribute(row, "class", "addon-action-message");
    var update = gWin.document.getAnonymousElementByAttribute(row, "anonid", "update");

    // Non-profile add-ons don't appear to have updates since we won't install
    // them
    var hasUpdate = addon[4] && addon[6] == PROFILE;

    is(pos, addon[8], "Should have been in the right position");
    is(keep.checked, addon[7], "Keep checkbox should be in the right state");

    check_state();

    info("Flipping keep");
    EventUtils.synthesizeMouseAtCenter(keep, { }, gWin);
    is(keep.checked, !addon[7], "Keep checkbox should be in the right state");

    check_state();

    pos++;
  }

  is(pos, 60, "Should have seen the right number of add-ons");

  run_next_test();
});
