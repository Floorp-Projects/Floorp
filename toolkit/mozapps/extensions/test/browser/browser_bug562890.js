/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Tests the Preferences button for addons in list view
 */

function test() {
  requestLongerTimeout(2);

  waitForExplicitFinish();

  var addonPrefsURI = CHROMEROOT + "addon_prefs.xul";

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
    optionsURL: addonPrefsURI
  }]);
  
  open_manager("addons://list/extension", function(aManager) {
    var addonList = aManager.document.getElementById("addon-list");
    for (var addonItem of addonList.childNodes) {
      if (addonItem.hasAttribute("name") &&
          addonItem.getAttribute("name") == "Test add-on 1")
        break;
    }
    var prefsBtn = aManager.document.getAnonymousElementByAttribute(addonItem,
                                                                   "anonid",
                                                                   "preferences-btn");
    is(prefsBtn.hidden, true, "Prefs button should be hidden for addon with no optionsURL set")

    for (addonItem of addonList.childNodes) {
      if (addonItem.hasAttribute("name") &&
          addonItem.getAttribute("name") == "Test add-on 2")
        break;
    }
    prefsBtn = aManager.document.getAnonymousElementByAttribute(addonItem,
                                                                "anonid",
                                                                "preferences-btn");
    is(prefsBtn.hidden, false, "Prefs button should be shown for addon with a optionsURL set")

    Services.ww.registerNotification(function TEST_ww_observer(aSubject, aTopic, aData) {
      if (aTopic == "domwindowclosed") {
        Services.ww.unregisterNotification(TEST_ww_observer);
        // Give the preference window a chance to finish closing before closing
        // the add-ons manager.
        executeSoon(function() {
          close_manager(aManager, finish);
        });
      } else if (aTopic == "domwindowopened") {
        let win = aSubject.QueryInterface(Ci.nsIDOMEventTarget);
        win = aSubject.QueryInterface(Ci.nsIDOMEventTarget);
        win.addEventListener("load", function TEST_ww_onLoad() {
          if (win.location != addonPrefsURI)
            return;

          win.removeEventListener("load", TEST_ww_onLoad, false);
          is(win.location, addonPrefsURI,
             "The correct addon pref window should have opened");
          win.close();
        }, false);
      }
    });

    addonList.ensureElementIsVisible(addonItem);
    EventUtils.synthesizeMouseAtCenter(prefsBtn, { }, aManager);
  });

}
