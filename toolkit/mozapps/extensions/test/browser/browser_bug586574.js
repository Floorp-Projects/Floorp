/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 586574 - Provide way to set a default for automatic updates

const PREF_AUTOUPDATE_DEFAULT = "extensions.update.autoUpdateDefault";

var gManagerWindow;
var gProvider;

var gUtilsBtn;
var gDropdownMenu;
var gSetDefault;
var gResetToAutomatic;
var gResetToManual;

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

  info("Ensuring default is set to true");
  Services.prefs.setBoolPref(PREF_AUTOUPDATE_DEFAULT, true);

  wait_for_popup(function() {
    is(gSetDefault.getAttribute("checked"), "true",
       "Set Default menuitem should be checked");
    is_element_visible(gResetToAutomatic,
                       "Reset to Automatic menuitem should be visible");
    is_element_hidden(gResetToManual,
                      "Reset to Manual menuitem should be hidden");

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
  wait_for_popup(function() {
    is(gSetDefault.getAttribute("checked"), "true",
       "Set Default menuitem should be checked");
    is_element_visible(gResetToAutomatic,
                       "Reset to Automatic menuitem should be visible");
    is_element_hidden(gResetToManual,
                      "Reset to Manual menuitem should be hidden");

    wait_for_hide(run_next_test);

    info("Clicking Set Default menuitem");
    EventUtils.synthesizeMouseAtCenter(gSetDefault, { }, gManagerWindow);
  });

  info("Opening utilities menu");
  EventUtils.synthesizeMouseAtCenter(gUtilsBtn, { }, gManagerWindow);
});


add_test(function() {
  wait_for_popup(function() {
    isnot(gSetDefault.getAttribute("checked"), "true",
          "Set Default menuitem should not be checked");
    is_element_hidden(gResetToAutomatic,
                      "Reset to automatic menuitem should be hidden");
    is_element_visible(gResetToManual,
                       "Reset to manual menuitem should be visible");

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
          "Set Default menuitem should not be checked");
    is_element_hidden(gResetToAutomatic,
                      "Reset to automatic menuitem should be hidden");
    is_element_visible(gResetToManual,
                       "Reset to manual menuitem should be visible");

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
       "Set Default menuitem should be checked");
    is_element_visible(gResetToAutomatic,
                       "Reset to Automatic menuitem should be visible");
    is_element_hidden(gResetToManual,
                      "Reset to Manual menuitem should be hidden");

    wait_for_hide(run_next_test);

    gUtilsMenu.hidePopup();
  });

  info("Opening utilities menu");
  EventUtils.synthesizeMouseAtCenter(gUtilsBtn, { }, gManagerWindow);
});

