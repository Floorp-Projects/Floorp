/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 586574 - Provide way to set a default for automatic updates
// Bug 710064 - Make the "Update Add-ons Automatically" checkbox state
//              also depend on the extensions.update.enabled pref

// TEST_PATH=toolkit/mozapps/extensions/test/browser/browser_bug586574.js make -C obj-ff mochitest-browser-chrome

const PREF_UPDATE_ENABLED = "extensions.update.enabled";
const PREF_AUTOUPDATE_DEFAULT = "extensions.update.autoUpdateDefault";

var gManagerWindow;
var gProvider;

var gUtilsBtn;
var gUtilsMenu;
var gDropdownMenu;
var gSetDefault;
var gResetToAutomatic;
var gResetToManual;

// Make sure we don't accidentally start a background update while the prefs
// are enabled.
disableBackgroundUpdateTimer();
registerCleanupFunction(() => {
  enableBackgroundUpdateTimer();
});

function test() {
  waitForExplicitFinish();

  gProvider = new MockProvider();

  gProvider.createAddons([{
    id: "addon1@tests.mozilla.org",
    name: "addon 1",
    version: "1.0",
    applyBackgroundUpdates: AddonManager.AUTOUPDATE_DISABLE
  }]);

  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;

    gUtilsBtn = gManagerWindow.document.getElementById("header-utils-btn");
    gUtilsMenu = gManagerWindow.document.getElementById("utils-menu");

    run_next_test();
  });
}


function end_test() {
  close_manager(gManagerWindow, finish);
}


function wait_for_popup(aCallback) {
  if (gUtilsMenu.state == "open") {
    aCallback();
    return;
  }

  gUtilsMenu.addEventListener("popupshown", function() {
    gUtilsMenu.removeEventListener("popupshown", arguments.callee, false);
    info("Utilities menu shown");
    aCallback();
  }, false);
}

function wait_for_hide(aCallback) {
  if (gUtilsMenu.state == "closed") {
    aCallback();
    return;
  }

  gUtilsMenu.addEventListener("popuphidden", function() {
    gUtilsMenu.removeEventListener("popuphidden", arguments.callee, false);
    info("Utilities menu hidden");
    aCallback();
  }, false);
}

add_test(function() {
  gSetDefault = gManagerWindow.document.getElementById("utils-autoUpdateDefault");
  gResetToAutomatic = gManagerWindow.document.getElementById("utils-resetAddonUpdatesToAutomatic");
  gResetToManual = gManagerWindow.document.getElementById("utils-resetAddonUpdatesToManual");

  info("Ensuring default prefs are set to true");
  Services.prefs.setBoolPref(PREF_UPDATE_ENABLED, true);
  Services.prefs.setBoolPref(PREF_AUTOUPDATE_DEFAULT, true);

  wait_for_popup(function() {
    is(gSetDefault.getAttribute("checked"), "true",
       "#1 Set Default menuitem should be checked");
    is_element_visible(gResetToAutomatic,
                       "#1 Reset to Automatic menuitem should be visible");
    is_element_hidden(gResetToManual,
                      "#1 Reset to Manual menuitem should be hidden");

    var listener = {
      onPropertyChanged: function(aAddon, aProperties) {
        AddonManager.removeAddonListener(listener);
        is(aAddon.id, gProvider.addons[0].id,
           "Should get onPropertyChanged event for correct addon");
        ok(!("applyBackgroundUpdates" in aProperties),
           "Should have gotten applyBackgroundUpdates in properties array");
        is(aAddon.applyBackgroundUpdates, AddonManager.AUTOUPDATE_DEFAULT,
           "Addon.applyBackgroundUpdates should have been reset to default");

        info("Setting Addon.applyBackgroundUpdates back to disabled");
        aAddon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DISABLE;

        wait_for_hide(run_next_test);
      }
    };
    AddonManager.addAddonListener(listener);

    info("Clicking Reset to Automatic menuitem");
    EventUtils.synthesizeMouseAtCenter(gResetToAutomatic, { }, gManagerWindow);
  });

  info("Opening utilities menu");
  EventUtils.synthesizeMouseAtCenter(gUtilsBtn, { }, gManagerWindow);
});


add_test(function() {
  info("Disabling extensions.update.enabled");
  Services.prefs.setBoolPref(PREF_UPDATE_ENABLED, false);

  wait_for_popup(function() {
    isnot(gSetDefault.getAttribute("checked"), "true",
          "#2 Set Default menuitem should not be checked");
    is_element_visible(gResetToAutomatic,
                       "#2 Reset to Automatic menuitem should be visible");
    is_element_hidden(gResetToManual,
                      "#2 Reset to Manual menuitem should be hidden");

    wait_for_hide(run_next_test);

    info("Clicking Set Default menuitem to reenable");
    EventUtils.synthesizeMouseAtCenter(gSetDefault, { }, gManagerWindow);
  });

  info("Opening utilities menu");
  EventUtils.synthesizeMouseAtCenter(gUtilsBtn, { }, gManagerWindow);
});


add_test(function() {
  ok(Services.prefs.getBoolPref(PREF_UPDATE_ENABLED),
     "extensions.update.enabled should be true after the previous test");
  ok(Services.prefs.getBoolPref(PREF_AUTOUPDATE_DEFAULT),
     "extensions.update.autoUpdateDefault should be true after the previous test");

  info("Disabling both extensions.update.enabled and extensions.update.autoUpdateDefault");
  Services.prefs.setBoolPref(PREF_UPDATE_ENABLED, false);
  Services.prefs.setBoolPref(PREF_AUTOUPDATE_DEFAULT, false);

  wait_for_popup(function() {
    isnot(gSetDefault.getAttribute("checked"), "true",
          "#3 Set Default menuitem should not be checked");
    is_element_hidden(gResetToAutomatic,
                      "#3 Reset to automatic menuitem should be hidden");
    is_element_visible(gResetToManual,
                       "#3 Reset to manual menuitem should be visible");

    wait_for_hide(run_next_test);

    info("Clicking Set Default menuitem to reenable");
    EventUtils.synthesizeMouseAtCenter(gSetDefault, { }, gManagerWindow);
  });

  info("Opening utilities menu");
  EventUtils.synthesizeMouseAtCenter(gUtilsBtn, { }, gManagerWindow);
});


add_test(function() {
  ok(Services.prefs.getBoolPref(PREF_UPDATE_ENABLED),
     "extensions.update.enabled should be true after the previous test");
  ok(Services.prefs.getBoolPref(PREF_AUTOUPDATE_DEFAULT),
     "extensions.update.autoUpdateDefault should be true after the previous test");

  info("clicking the button to disable extensions.update.autoUpdateDefault");
  wait_for_popup(function() {
    is(gSetDefault.getAttribute("checked"), "true",
       "#4 Set Default menuitem should be checked");
    is_element_visible(gResetToAutomatic,
                       "#4 Reset to Automatic menuitem should be visible");
    is_element_hidden(gResetToManual,
                      "#4 Reset to Manual menuitem should be hidden");

    wait_for_hide(run_next_test);

    info("Clicking Set Default menuitem to disable");
    EventUtils.synthesizeMouseAtCenter(gSetDefault, { }, gManagerWindow);
  });

  info("Opening utilities menu");
  EventUtils.synthesizeMouseAtCenter(gUtilsBtn, { }, gManagerWindow);
});


add_test(function() {
  ok(Services.prefs.getBoolPref(PREF_UPDATE_ENABLED),
     "extensions.update.enabled should be true after the previous test");
  is(Services.prefs.getBoolPref(PREF_AUTOUPDATE_DEFAULT), false,
     "extensions.update.autoUpdateDefault should be false after the previous test");

  wait_for_popup(function() {
    isnot(gSetDefault.getAttribute("checked"), "true",
          "#5 Set Default menuitem should not be checked");
    is_element_hidden(gResetToAutomatic,
                      "#5 Reset to automatic menuitem should be hidden");
    is_element_visible(gResetToManual,
                       "#5 Reset to manual menuitem should be visible");

    var listener = {
      onPropertyChanged: function(aAddon, aProperties) {
        AddonManager.removeAddonListener(listener);
        is(aAddon.id, gProvider.addons[0].id,
           "Should get onPropertyChanged event for correct addon");
        ok(!("applyBackgroundUpdates" in aProperties),
           "Should have gotten applyBackgroundUpdates in properties array");
        is(aAddon.applyBackgroundUpdates, AddonManager.AUTOUPDATE_DEFAULT,
           "Addon.applyBackgroundUpdates should have been reset to default");

        info("Setting Addon.applyBackgroundUpdates back to disabled");
        aAddon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DISABLE;

        wait_for_hide(run_next_test);
      }
    };
    AddonManager.addAddonListener(listener);

    info("Clicking Reset to Manual menuitem");
    EventUtils.synthesizeMouseAtCenter(gResetToManual, { }, gManagerWindow);

  });

  info("Opening utilities menu");
  EventUtils.synthesizeMouseAtCenter(gUtilsBtn, { }, gManagerWindow);
});


add_test(function() {
  wait_for_popup(function() {
    isnot(gSetDefault.getAttribute("checked"), "true",
          "#6 Set Default menuitem should not be checked");
    is_element_hidden(gResetToAutomatic,
                      "#6 Reset to automatic menuitem should be hidden");
    is_element_visible(gResetToManual,
                       "#6 Reset to manual menuitem should be visible");

    wait_for_hide(run_next_test);

    info("Clicking Set Default menuitem");
    EventUtils.synthesizeMouseAtCenter(gSetDefault, { }, gManagerWindow);
  });

  info("Opening utilities menu");
  EventUtils.synthesizeMouseAtCenter(gUtilsBtn, { }, gManagerWindow);
});


add_test(function() {
  wait_for_popup(function() {
    is(gSetDefault.getAttribute("checked"), "true",
       "#7 Set Default menuitem should be checked");
    is_element_visible(gResetToAutomatic,
                       "#7 Reset to Automatic menuitem should be visible");
    is_element_hidden(gResetToManual,
                      "#7 Reset to Manual menuitem should be hidden");

    wait_for_hide(run_next_test);

    gUtilsMenu.hidePopup();
  });

  info("Opening utilities menu");
  EventUtils.synthesizeMouseAtCenter(gUtilsBtn, { }, gManagerWindow);
});

