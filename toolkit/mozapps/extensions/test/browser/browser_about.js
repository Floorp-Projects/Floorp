/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Tests the default and custom "about" dialogs of add-ons.
 *
 * Test for bug 610661 <https://bugzilla.mozilla.org/show_bug.cgi?id=610661>:
 * Addon object not passed to custom about dialogs.
 */

var gManagerWindow;

const URI_ABOUT_DEFAULT = "chrome://mozapps/content/extensions/about.xul";
const URI_ABOUT_CUSTOM = CHROMEROOT + "addon_about.xul";

function test() {
  requestLongerTimeout(2);

  waitForExplicitFinish();

  var gProvider = new MockProvider();
  gProvider.createAddons([{
    id: "test1@tests.mozilla.org",
    name: "Test add-on 1",
    description: "foo"
  },
  {
    id: "test2@tests.mozilla.org",
    name: "Test add-on 2",
    description: "bar",
    aboutURL: URI_ABOUT_CUSTOM
  }]);

  open_manager("addons://list/extension", function(aManager) {
    gManagerWindow = aManager;

    test_about_window("Test add-on 1", URI_ABOUT_DEFAULT, function() {
      test_about_window("Test add-on 2", URI_ABOUT_CUSTOM, function() {
        close_manager(gManagerWindow, finish);
      });
    });
  });
}

function test_about_window(aAddonItemName, aExpectedAboutUri, aCallback) {
  var addonList = gManagerWindow.document.getElementById("addon-list");
  for (var addonItem of addonList.childNodes) {
    if (addonItem.hasAttribute("name") &&
        addonItem.getAttribute("name") === aAddonItemName)
      break;
  }

  info("Waiting for about dialog");
  Services.ww.registerNotification(function TEST_ww_observer(aSubject, aTopic,
                                                             aData) {
    if (aTopic == "domwindowclosed") {
      Services.ww.unregisterNotification(TEST_ww_observer);

      info("About dialog closed, waiting for focus on browser window");
      waitForFocus(() => executeSoon(aCallback));
    } else if (aTopic == "domwindowopened") {
      info("About dialog opened, waiting for focus");

      let win = aSubject.QueryInterface(Ci.nsIDOMEventTarget);
      waitForFocus(function() {
        info("Saw about dialog");

        is(win.location,
           aExpectedAboutUri,
           "The correct add-on about window should have opened");

        is(win.arguments && win.arguments[0] && win.arguments[0].name,
           aAddonItemName,
           "window.arguments[0] should refer to the add-on object");

        executeSoon(() => win.close());
      }, win);
    }
  });

  gManagerWindow.gViewController.doCommand("cmd_showItemAbout",
                                           addonItem.mAddon);
}
