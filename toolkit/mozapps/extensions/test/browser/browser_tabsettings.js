/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests various aspects of the details view

var gManagerWindow;
var gProvider;

function test() {
  waitForExplicitFinish();

  gProvider = new MockProvider();

  gProvider.createAddons([{
    id: "tabsettings@tests.mozilla.org",
    name: "Tab Settings",
    version: "1",
    optionsURL: CHROMEROOT + "addon_prefs.xul",
    optionsType: AddonManager.OPTIONS_TYPE_TAB
  }]);

  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;

    run_next_test();
  });
}

function end_test() {
  close_manager(gManagerWindow, function() {
    finish();
  });
}

add_test(function() {
  var addon = get_addon_element(gManagerWindow, "tabsettings@tests.mozilla.org");
  is(addon.mAddon.optionsType, AddonManager.OPTIONS_TYPE_TAB, "Options should be inline type");
  addon.parentNode.ensureElementIsVisible(addon);

  var button = gManagerWindow.document.getAnonymousElementByAttribute(addon, "anonid", "preferences-btn");
  is_element_visible(button, "Preferences button should be visible");

  if (gUseInContentUI) {
    EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);

    var browser = gBrowser.selectedBrowser;
    browser.addEventListener("DOMContentLoaded", function() {
      browser.removeEventListener("DOMContentLoaded", arguments.callee, false);
      is(browser.currentURI.spec, addon.mAddon.optionsURL, "New tab should have loaded the options URL");
      browser.contentWindow.close();
      run_next_test();
    }, false);
    return;
  }

  let instantApply = Services.prefs.getBoolPref("browser.preferences.instantApply");

  function observer(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "domwindowclosed":
        // Give the preference window a chance to finish closing before
        // closing the add-ons manager.
        waitForFocus(function() {
          Services.ww.unregisterNotification(observer);
          run_next_test();
        });
        break;
      case "domwindowopened":
        let win = aSubject.QueryInterface(Ci.nsIDOMEventTarget);
        waitForFocus(function() {
          // If the openDialog privileges are wrong a new browser window
          // will open, let the test proceed (and fail) rather than timeout.
          if (win.location != addon.mAddon.optionsURL &&
              win.location != "chrome://browser/content/browser.xul")
            return;

          is(win.location, addon.mAddon.optionsURL,
             "The correct addon pref window should have opened");

          let chromeFlags = win.QueryInterface(Ci.nsIInterfaceRequestor).
                                getInterface(Ci.nsIWebNavigation).
                                QueryInterface(Ci.nsIDocShellTreeItem).treeOwner.
                                QueryInterface(Ci.nsIInterfaceRequestor).
                                getInterface(Ci.nsIXULWindow).chromeFlags;
          ok(chromeFlags & Ci.nsIWebBrowserChrome.CHROME_OPENAS_CHROME &&
             (instantApply || chromeFlags & Ci.nsIWebBrowserChrome.CHROME_OPENAS_DIALOG),
             "Window was open as a chrome dialog.");

          win.close();
        }, win);
        break;
    }
  }

  Services.ww.registerNotification(observer);
  EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);
});
