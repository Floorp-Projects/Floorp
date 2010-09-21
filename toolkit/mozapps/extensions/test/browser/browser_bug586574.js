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
    run_next_test();
  });
}


function end_test() {
  close_manager(gManagerWindow, finish);
}


add_test(function() {
  gUtilsBtn = gManagerWindow.document.getElementById("header-utils-btn");
  gUtilsMenu = gManagerWindow.document.getElementById("utils-menu");
  gSetDefault = gManagerWindow.document.getElementById("utils-autoUpdateDefault");
  gResetToAutomatic = gManagerWindow.document.getElementById("utils-resetAddonUpdatesToAutomatic");
  gResetToManual = gManagerWindow.document.getElementById("utils-resetAddonUpdatesToManual");
  
  info("Ensuring default is set to true");
  Services.prefs.setBoolPref(PREF_AUTOUPDATE_DEFAULT, true);
  
  gUtilsBtn.addEventListener("popupshown", function() {
    gUtilsBtn.removeEventListener("popupshown", arguments.callee, false);
    
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
        executeSoon(run_next_test);
      }
    };
    AddonManager.addAddonListener(listener);
    
    info("Clicking Reset to Automatic menuitem");
    EventUtils.synthesizeMouse(gResetToAutomatic, 2, 2, { }, gManagerWindow);

  }, false);
  info("Opening utilities menu");
  EventUtils.synthesizeMouse(gUtilsBtn, 2, 2, { }, gManagerWindow);
});


add_test(function() {
  gUtilsBtn.addEventListener("popupshown", function() {
    gUtilsBtn.removeEventListener("popupshown", arguments.callee, false);
    
    is(gSetDefault.getAttribute("checked"), "true",
       "Set Default menuitem should be checked");
    is_element_visible(gResetToAutomatic,
                       "Reset to Automatic menuitem should be visible");
    is_element_hidden(gResetToManual,
                      "Reset to Manual menuitem should be hidden");
    
    info("Clicking Set Default menuitem");
    EventUtils.synthesizeMouse(gSetDefault, 2, 2, { }, gManagerWindow);

    executeSoon(run_next_test);
  }, false);
  info("Opening utilities menu");
  EventUtils.synthesizeMouse(gUtilsBtn, 2, 2, { }, gManagerWindow);
});


add_test(function() {
  gUtilsBtn.addEventListener("popupshown", function() {
    gUtilsBtn.removeEventListener("popupshown", arguments.callee, false);
    
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
        executeSoon(run_next_test);
      }
    };
    AddonManager.addAddonListener(listener);
    
    info("Clicking Reset to Manual menuitem");
    EventUtils.synthesizeMouse(gResetToManual, 2, 2, { }, gManagerWindow);

  }, false);
  info("Opening utilities menu");
  EventUtils.synthesizeMouse(gUtilsBtn, 2, 2, { }, gManagerWindow);
});



add_test(function() {
  gUtilsBtn.addEventListener("popupshown", function() {
    gUtilsBtn.removeEventListener("popupshown", arguments.callee, false);
    
    isnot(gSetDefault.getAttribute("checked"), "true",
          "Set Default menuitem should not be checked");
    is_element_hidden(gResetToAutomatic,
                      "Reset to automatic menuitem should be hidden");
    is_element_visible(gResetToManual,
                       "Reset to manual menuitem should be visible");
    
    info("Clicking Set Default menuitem");
    EventUtils.synthesizeMouse(gSetDefault, 2, 2, { }, gManagerWindow);
    
    executeSoon(run_next_test);
  }, false);
  info("Opening utilities menu");
  EventUtils.synthesizeMouse(gUtilsBtn, 2, 2, { }, gManagerWindow);
});


add_test(function() {
  gUtilsBtn.addEventListener("popupshown", function() {
    gUtilsBtn.removeEventListener("popupshown", arguments.callee, false);
    
    is(gSetDefault.getAttribute("checked"), "true",
       "Set Default menuitem should be checked");
    is_element_visible(gResetToAutomatic,
                       "Reset to Automatic menuitem should be visible");
    is_element_hidden(gResetToManual,
                      "Reset to Manual menuitem should be hidden");
    
    gUtilsMenu.hidePopup();
    run_next_test();
  }, false);
  info("Opening utilities menu");
  EventUtils.synthesizeMouse(gUtilsBtn, 2, 2, { }, gManagerWindow);
});

