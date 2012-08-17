/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 566194 - safe mode / security & compatibility check status are not exposed in new addon manager UI

function test() {
  waitForExplicitFinish();
  run_next_test();
}

function end_test() {
  finish();
}

add_test(function() {
  info("Testing compatibility checking warning");

  info("Setting checkCompatibility to false");
  AddonManager.checkCompatibility = false;

  open_manager("addons://list/extension", function(aWindow) {
    var hbox = aWindow.document.querySelector("#list-view hbox.global-warning-checkcompatibility");
    is_element_visible(hbox, "Check Compatibility warning hbox should be visible");
    var button = aWindow.document.querySelector("#list-view button.global-warning-checkcompatibility");
    is_element_visible(button, "Check Compatibility warning button should be visible");

    info("Clicking 'Enable' button");
    EventUtils.synthesizeMouse(button, 2, 2, { }, aWindow);
    is(AddonManager.checkCompatibility, true, "Check Compatibility pref should be cleared");
    is_element_hidden(hbox, "Check Compatibility warning hbox should be hidden");
    is_element_hidden(button, "Check Compatibility warning button should be hidden");

    close_manager(aWindow, function() {
      run_next_test();
    });
  });
});

add_test(function() {
  info("Testing update security checking warning");

  var pref = "extensions.checkUpdateSecurity";
  info("Setting " + pref + " pref to false")
  Services.prefs.setBoolPref(pref, false);

  open_manager(null, function(aWindow) {
    var hbox = aWindow.document.querySelector("#list-view hbox.global-warning-updatesecurity");
    is_element_visible(hbox, "Check Update Security warning hbox should be visible");
    var button = aWindow.document.querySelector("#list-view button.global-warning-updatesecurity");
    is_element_visible(button, "Check Update Security warning button should be visible");

    info("Clicking 'Enable' button");
    EventUtils.synthesizeMouse(button, 2, 2, { }, aWindow);
    is(Services.prefs.prefHasUserValue(pref), false, "Check Update Security pref should be cleared");
    is_element_hidden(hbox, "Check Update Security warning hbox should be hidden");
    is_element_hidden(button, "Check Update Security warning button should be hidden");

    close_manager(aWindow, function() {
      run_next_test();
    });
  });
});
