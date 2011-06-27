/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html,<p>test for bug 663443. test1";

const POSITION_PREF = "devtools.webconsole.position";

function tabLoad(aEvent) {
  browser.removeEventListener(aEvent.type, arguments.callee, true);

  Services.prefs.setCharPref(POSITION_PREF, "window");

  openConsole();

  document.addEventListener("popupshown", function() {
    document.removeEventListener("popupshown", arguments.callee, false);

    let hudId = HUDService.getHudIdByWindow(content);
    ok(hudId, "Web Console is open");

    let HUD = HUDService.hudReferences[hudId];
    ok(HUD.consolePanel, "Web Console opened in a panel");

    isnot(HUD.consolePanel.label.indexOf("test1"), -1, "panel title is correct");

    browser.addEventListener("load", function() {
      browser.removeEventListener("load", arguments.callee, true);

      isnot(HUD.consolePanel.label.indexOf("test2"), -1,
            "panel title is correct after page navigation");

      Services.prefs.clearUserPref(POSITION_PREF);

      executeSoon(finish);
    }, true);

    content.location = "data:text/html,<p>test2 for bug 663443";
  }, false);
}

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", tabLoad, true);
}
