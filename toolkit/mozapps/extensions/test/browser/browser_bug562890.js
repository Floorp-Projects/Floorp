/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Tests the Preferences button for addons in list view
 */

function test() {
  waitForExplicitFinish();
  
  var addonPrefsURI = TESTROOT + "addon_prefs.xul";

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
    for (var i = 0; i < addonList.childNodes.length; i++) {
      var addonItem = addonList.childNodes[i];
      if (addonItem.hasAttribute("name") &&
          addonItem.getAttribute("name") == "Test add-on 1")
        break;
    }
    var prefsBtn = aManager.document.getAnonymousElementByAttribute(addonItem,
                                                                   "anonid",
                                                                   "preferences-btn");
    is(prefsBtn.hidden, true, "Prefs button should be hidden for addon with no optionsURL set")

    for (i = 0; i < addonList.childNodes.length; i++) {
      addonItem = addonList.childNodes[i];
      if (addonItem.hasAttribute("name") &&
          addonItem.getAttribute("name") == "Test add-on 2")
        break;
    }
    prefsBtn = aManager.document.getAnonymousElementByAttribute(addonItem,
                                                                "anonid",
                                                                "preferences-btn");
    is(prefsBtn.hidden, false, "Prefs button should be shown for addon with a optionsURL set")

    Services.ww.registerNotification(function(aSubject, aTopic, aData) {
      if (aTopic == "domwindowclosed") {
        Services.ww.unregisterNotification(arguments.callee);
        waitForFocus(function() {
          close_manager(aManager);
          finish();
        }, aManager);
      } else if (aTopic == "domwindowopened") {
        let win = aSubject.QueryInterface(Ci.nsIDOMEventTarget);
        win.documentURI, addonPrefsURI, "The correct addon pref window should open"
        waitForFocus(function() {
          win.close();
        }, win);
      }
    });

    EventUtils.synthesizeMouseAtCenter(prefsBtn, { }, aManager);
  });

}
