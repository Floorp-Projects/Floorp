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

  var version = Services.appinfo.version.replace(/^([^\.]+\.[0-9]+[a-z]*).*/gi, "$1");
  var pref = "extensions.checkCompatibility." + version;
  info("Setting " + pref + " pref to false")
  Services.prefs.setBoolPref(pref, false);

  open_manager(null, function(aWindow) {
    var label = aWindow.document.querySelector("#list-view label.global-warning-checkcompatibility");
    is_element_visible(label, "Check Compatibility warning label should be visible");
    var button = aWindow.document.querySelector("#list-view button.global-warning-checkcompatibility");
    is_element_visible(button, "Check Compatibility warning button should be visible");

    info("Clicking 'Enable' button");
    EventUtils.synthesizeMouse(button, 2, 2, { }, aWindow);
    is(Services.prefs.prefHasUserValue(pref), false, "Check Compatability pref should be cleared");
    is_element_hidden(label, "Check Compatibility warning label should be hidden");
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
    var label = aWindow.document.querySelector("#list-view label.global-warning-updatesecurity");
    is_element_visible(label, "Check Update Security warning label should be visible");
    var button = aWindow.document.querySelector("#list-view button.global-warning-updatesecurity");
    is_element_visible(button, "Check Update Security warning button should be visible");

    info("Clicking 'Enable' button");
    EventUtils.synthesizeMouse(button, 2, 2, { }, aWindow);
    is(Services.prefs.prefHasUserValue(pref), false, "Check Update Security pref should be cleared");
    is_element_hidden(label, "Check Update Security warning label should be hidden");
    is_element_hidden(button, "Check Update Security warning button should be hidden");

    close_manager(aWindow, function() {
      run_next_test();
    });
  });
});
