/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 656269 - Add link to Mozilla plugin check from Add-ons Manager

const MAIN_URL = "https://example.com/" + RELATIVE_DIR + "discovery.html";
const PREF_PLUGINCHECKURL = "plugins.update.url";

function test() {
  waitForExplicitFinish();

  Services.prefs.setCharPref(PREF_PLUGINCHECKURL, MAIN_URL);
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(PREF_PLUGINCHECKURL);
  });

  run_next_test();
}

function end_test() {
  finish();
}

add_test(function() {
  open_manager("addons://list/extension", function(aManager) {
    info("Testing plugin check information");
    var button = aManager.document.querySelector("#list-view button.global-info-plugincheck");
    is_element_hidden(button, "Plugin Check message button should be hidden");

    info("Changing view to plugins")
    EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-plugin"), { }, aManager);

    wait_for_view_load(aManager, function(aManager) {
      var button = aManager.document.querySelector("#list-view button.global-info-plugincheck");
      is_element_visible(button, "Plugin Check message button should be visible");

      info("Clicking 'Plugin Check' button");
      EventUtils.synthesizeMouseAtCenter(button, { }, aManager);
      gBrowser.addEventListener("load", function(event) {
        if (!(event.target instanceof Document) ||
            event.target.location.href == "about:blank")
          return;
        gBrowser.removeEventListener("load", arguments.callee, true);

        is(gBrowser.currentURI.spec, Services.urlFormatter.formatURLPref("plugins.update.url"), "Plugin Check URL should match");

        gBrowser.removeCurrentTab();
        close_manager(aManager, function() {
          run_next_test();
        });
      }, true);
    });
  });
});