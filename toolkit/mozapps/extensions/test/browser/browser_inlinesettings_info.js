/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests various aspects of the details view
Components.utils.import("resource://gre/modules/Preferences.jsm");

var gManagerWindow;
var gCategoryUtilities;
var gProvider;

const SETTINGS_ROWS = 8;

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

var observer = {
  lastDisplayed: null,
  callback: null,
  checkDisplayed(aExpected) {
    is(this.lastDisplayed, aExpected, "'addon-options-displayed' notification should have fired");
    this.lastDisplayed = null;
  },
  checkNotDisplayed() {
    is(this.lastDisplayed, null, "'addon-options-displayed' notification should not have fired");
  },
  lastHidden: null,
  checkHidden(aExpected) {
    is(this.lastHidden, aExpected, "'addon-options-hidden' notification should have fired");
    this.lastHidden = null;
  },
  checkNotHidden() {
    is(this.lastHidden, null, "'addon-options-hidden' notification should not have fired");
  },
  observe(aSubject, aTopic, aData) {
    if (aTopic == AddonManager.OPTIONS_NOTIFICATION_DISPLAYED) {
      this.lastDisplayed = aData;
      // Test if the binding has applied before the observers are notified. We test the second setting here,
      // because the code operates on the first setting and we want to check it applies to all.
      var setting = aSubject.querySelector("rows > setting[first-row] ~ setting");
      var input = gManagerWindow.document.getAnonymousElementByAttribute(setting, "class", "preferences-title");
      isnot(input, null, "XBL binding should be applied");

      // Add some extra height to the scrolling pane to ensure that it needs to scroll when appropriate.
      gManagerWindow.document.getElementById("detail-controls").style.marginBottom = "1000px";

      if (this.callback) {
        var tempCallback = this.callback;
        this.callback = null;
        tempCallback();
      }
    } else if (aTopic == AddonManager.OPTIONS_NOTIFICATION_HIDDEN) {
      this.lastHidden = aData;
    }
  }
};

function installAddon(aCallback) {
  AddonManager.getInstallForURL(TESTROOT + "addons/browser_inlinesettings1_info.xpi",
                                function(aInstall) {
    aInstall.addListener({
      onInstallEnded() {
        executeSoon(aCallback);
      }
    });
    aInstall.install();
  }, "application/x-xpinstall");
}

function checkScrolling(aShouldHaveScrolled) {
  var detailView = gManagerWindow.document.getElementById("detail-view");
  var boxObject = detailView.boxObject;
  ok(detailView.scrollHeight > boxObject.height, "Page should require scrolling");
  if (aShouldHaveScrolled)
    isnot(detailView.scrollTop, 0, "Page should have scrolled");
  else
    is(detailView.scrollTop, 0, "Page should not have scrolled");
}

function test() {
  waitForExplicitFinish();

  gProvider = new MockProvider();

  gProvider.createAddons([{
    id: "inlinesettings2@tests.mozilla.org",
    name: "Inline Settings (Regular)",
    version: "1",
    optionsURL: CHROMEROOT + "options.xul",
    optionsType: AddonManager.OPTIONS_TYPE_INLINE_INFO,
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_DISABLE,
  }, {
    id: "inlinesettings3@tests.mozilla.org",
    name: "Inline Settings (More Options)",
    description: "Tests for option types introduced after Mozilla 7.0",
    version: "1",
    optionsURL: CHROMEROOT + "more_options.xul",
    optionsType: AddonManager.OPTIONS_TYPE_INLINE_INFO
  }, {
    id: "noninlinesettings@tests.mozilla.org",
    name: "Non-Inline Settings",
    version: "1",
    optionsURL: CHROMEROOT + "addon_prefs.xul"
  }]);

  installAddon(function() {
    open_manager("addons://list/extension", function(aWindow) {
      gManagerWindow = aWindow;
      gCategoryUtilities = new CategoryUtilities(gManagerWindow);

      Services.obs.addObserver(observer,
                               AddonManager.OPTIONS_NOTIFICATION_DISPLAYED,
                               false);
      Services.obs.addObserver(observer,
                               AddonManager.OPTIONS_NOTIFICATION_HIDDEN,
                               false);

      run_next_test();
    });
  });
}

function end_test() {
  Services.obs.removeObserver(observer,
                              AddonManager.OPTIONS_NOTIFICATION_DISPLAYED);

  Services.prefs.clearUserPref("extensions.inlinesettings1.bool");
  Services.prefs.clearUserPref("extensions.inlinesettings1.boolint");
  Services.prefs.clearUserPref("extensions.inlinesettings1.integer");
  Services.prefs.clearUserPref("extensions.inlinesettings1.string");
  Services.prefs.clearUserPref("extensions.inlinesettings1.color");
  Services.prefs.clearUserPref("extensions.inlinesettings1.file");
  Services.prefs.clearUserPref("extensions.inlinesettings1.directory");
  Services.prefs.clearUserPref("extensions.inlinesettings3.radioBool");
  Services.prefs.clearUserPref("extensions.inlinesettings3.radioInt");
  Services.prefs.clearUserPref("extensions.inlinesettings3.radioString");
  Services.prefs.clearUserPref("extensions.inlinesettings3.menulist");

  MockFilePicker.cleanup();

  close_manager(gManagerWindow, function() {
    observer.checkHidden("inlinesettings2@tests.mozilla.org");
    Services.obs.removeObserver(observer,
                                AddonManager.OPTIONS_NOTIFICATION_HIDDEN);

    AddonManager.getAddonByID("inlinesettings1@tests.mozilla.org", function(aAddon) {
      aAddon.uninstall();
      finish();
    });
  });
}

// Addon with options.xul
add_test(function() {
  var addon = get_addon_element(gManagerWindow, "inlinesettings1@tests.mozilla.org");
  is(addon.mAddon.optionsType, AddonManager.OPTIONS_TYPE_INLINE_INFO, "Options should be inline info type");
  addon.parentNode.ensureElementIsVisible(addon);

  var button = gManagerWindow.document.getAnonymousElementByAttribute(addon, "anonid", "preferences-btn");
  is_element_hidden(button, "Preferences button should be hidden");

  run_next_test();
});

// Addon with inline preferences as optionsURL
add_test(function() {
  var addon = get_addon_element(gManagerWindow, "inlinesettings2@tests.mozilla.org");
  is(addon.mAddon.optionsType, AddonManager.OPTIONS_TYPE_INLINE_INFO, "Options should be inline info type");
  addon.parentNode.ensureElementIsVisible(addon);

  var button = gManagerWindow.document.getAnonymousElementByAttribute(addon, "anonid", "preferences-btn");
  is_element_hidden(button, "Preferences button should be hidden");

  run_next_test();
});

// Addon with non-inline preferences as optionsURL
add_test(function() {
  var addon = get_addon_element(gManagerWindow, "noninlinesettings@tests.mozilla.org");
  is(addon.mAddon.optionsType, AddonManager.OPTIONS_TYPE_DIALOG, "Options should be dialog type");
  addon.parentNode.ensureElementIsVisible(addon);

  var button = gManagerWindow.document.getAnonymousElementByAttribute(addon, "anonid", "preferences-btn");
  is_element_visible(button, "Preferences button should be visible");

  run_next_test();
});

// Addon with options.xul, also a test for the setting.xml bindings
add_test(function() {
  var addon = get_addon_element(gManagerWindow, "inlinesettings1@tests.mozilla.org");
  addon.parentNode.ensureElementIsVisible(addon);

  var button = gManagerWindow.document.getAnonymousElementByAttribute(addon, "anonid", "details-btn");
  EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);

  wait_for_view_load(gManagerWindow, function() {
    observer.checkDisplayed("inlinesettings1@tests.mozilla.org");

    var grid = gManagerWindow.document.getElementById("detail-grid");
    var settings = grid.querySelectorAll("rows > setting");
    is(settings.length, SETTINGS_ROWS, "Grid should have settings children");

    ok(settings[0].hasAttribute("first-row"), "First visible row should have first-row attribute");
    Services.prefs.setBoolPref("extensions.inlinesettings1.bool", false);
    var input = gManagerWindow.document.getAnonymousElementByAttribute(settings[0], "anonid", "input");
    isnot(input.checked, true, "Checkbox should have initial value");
    is(input.label, "Check box label", "Checkbox should be labelled");
    EventUtils.synthesizeMouseAtCenter(input, { clickCount: 1 }, gManagerWindow);
    is(input.checked, true, "Checkbox should have updated value");
    is(Services.prefs.getBoolPref("extensions.inlinesettings1.bool"), true, "Bool pref should have been updated");
    EventUtils.synthesizeMouseAtCenter(input, { clickCount: 1 }, gManagerWindow);
    isnot(input.checked, true, "Checkbox should have updated value");
    is(Services.prefs.getBoolPref("extensions.inlinesettings1.bool"), false, "Bool pref should have been updated");

    ok(!settings[1].hasAttribute("first-row"), "Not the first row");
    Services.prefs.setIntPref("extensions.inlinesettings1.boolint", 0);
    input = gManagerWindow.document.getAnonymousElementByAttribute(settings[1], "anonid", "input");
    isnot(input.checked, true, "Checkbox should have initial value");
    EventUtils.synthesizeMouseAtCenter(input, { clickCount: 1 }, gManagerWindow);
    is(input.checked, true, "Checkbox should have updated value");
    is(Services.prefs.getIntPref("extensions.inlinesettings1.boolint"), 1, "BoolInt pref should have been updated");
    EventUtils.synthesizeMouseAtCenter(input, { clickCount: 1 }, gManagerWindow);
    isnot(input.checked, true, "Checkbox should have updated value");
    is(Services.prefs.getIntPref("extensions.inlinesettings1.boolint"), 2, "BoolInt pref should have been updated");

    ok(!settings[2].hasAttribute("first-row"), "Not the first row");
    Services.prefs.setIntPref("extensions.inlinesettings1.integer", 0);
    input = gManagerWindow.document.getAnonymousElementByAttribute(settings[2], "anonid", "input");
    is(input.value, "0", "Number box should have initial value");
    input.select();
    EventUtils.synthesizeKey("1", {}, gManagerWindow);
    EventUtils.synthesizeKey("3", {}, gManagerWindow);
    is(input.value, "13", "Number box should have updated value");
    is(Services.prefs.getIntPref("extensions.inlinesettings1.integer"), 13, "Integer pref should have been updated");
    EventUtils.synthesizeKey("VK_DOWN", {}, gManagerWindow);
    is(input.value, "12", "Number box should have updated value");
    is(Services.prefs.getIntPref("extensions.inlinesettings1.integer"), 12, "Integer pref should have been updated");

    ok(!settings[3].hasAttribute("first-row"), "Not the first row");
    Services.prefs.setCharPref("extensions.inlinesettings1.string", "foo");
    input = gManagerWindow.document.getAnonymousElementByAttribute(settings[3], "anonid", "input");
    is(input.value, "foo", "Text box should have initial value");
    input.select();
    EventUtils.synthesizeKey("b", {}, gManagerWindow);
    EventUtils.synthesizeKey("a", {}, gManagerWindow);
    EventUtils.synthesizeKey("r", {}, gManagerWindow);
    is(input.value, "bar", "Text box should have updated value");
    is(Services.prefs.getCharPref("extensions.inlinesettings1.string"), "bar", "String pref should have been updated");

    ok(!settings[4].hasAttribute("first-row"), "Not the first row");
    input = settings[4].firstElementChild;
    is(input.value, "1", "Menulist should have initial value");
    input.focus();
    EventUtils.synthesizeKey("b", {}, gManagerWindow);
    is(input.value, "2", "Menulist should have updated value");
    is(gManagerWindow._testValue, "2", "Menulist oncommand handler should've updated the test value");
    delete gManagerWindow._testValue;

    ok(!settings[5].hasAttribute("first-row"), "Not the first row");
    Services.prefs.setCharPref("extensions.inlinesettings1.color", "#FF0000");
    input = gManagerWindow.document.getAnonymousElementByAttribute(settings[5], "anonid", "input");
    is(input.color, "#FF0000", "Color picker should have initial value");
    input.focus();
    EventUtils.synthesizeKey("VK_RIGHT", {}, gManagerWindow);
    EventUtils.synthesizeKey("VK_RIGHT", {}, gManagerWindow);
    EventUtils.synthesizeKey("VK_RETURN", {}, gManagerWindow);
    input.hidePopup();
    is(input.color, "#FF9900", "Color picker should have updated value");
    is(Services.prefs.getCharPref("extensions.inlinesettings1.color"), "#FF9900", "Color pref should have been updated");

    ok(!settings[6].hasAttribute("first-row"), "Not the first row");
    var button = gManagerWindow.document.getAnonymousElementByAttribute(settings[6], "anonid", "button");

    // Workaround for bug 1155324 - we need to ensure that the button is scrolled into view.
    button.scrollIntoView();

    input = gManagerWindow.document.getAnonymousElementByAttribute(settings[6], "anonid", "input");
    is(input.value, "", "Label value should be empty");
    is(input.tooltipText, "", "Label tooltip should be empty");

    var profD = Services.dirsvc.get("ProfD", Ci.nsIFile);
    var curProcD = Services.dirsvc.get("CurProcD", Ci.nsIFile);

    MockFilePicker.setFiles([profD]);
    MockFilePicker.returnValue = Ci.nsIFilePicker.returnOK;

    let promise = new Promise(resolve => {
      MockFilePicker.afterOpenCallback = resolve;
      EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);
    });

    promise.then(() => {
      is(MockFilePicker.mode, Ci.nsIFilePicker.modeOpen, "File picker mode should be open file");
      is(input.value, profD.path, "Label value should match file chosen");
      is(input.tooltipText, profD.path, "Label tooltip should match file chosen");
      is(Services.prefs.getCharPref("extensions.inlinesettings1.file"), profD.path, "File pref should match file chosen");

      MockFilePicker.setFiles([curProcD]);
      MockFilePicker.returnValue = Ci.nsIFilePicker.returnCancel;

      return promise = new Promise(resolve => {
        MockFilePicker.afterOpenCallback = resolve;
        EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);
      });
    }).then(() => {
      is(MockFilePicker.mode, Ci.nsIFilePicker.modeOpen, "File picker mode should be open file");
      is(input.value, profD.path, "Label value should not have changed");
      is(input.tooltipText, profD.path, "Label tooltip should not have changed");
      is(Services.prefs.getCharPref("extensions.inlinesettings1.file"), profD.path, "File pref should not have changed");

      ok(!settings[7].hasAttribute("first-row"), "Not the first row");
      button = gManagerWindow.document.getAnonymousElementByAttribute(settings[7], "anonid", "button");
      input = gManagerWindow.document.getAnonymousElementByAttribute(settings[7], "anonid", "input");
      is(input.value, "", "Label value should be empty");
      is(input.tooltipText, "", "Label tooltip should be empty");

      MockFilePicker.setFiles([profD]);
      MockFilePicker.returnValue = Ci.nsIFilePicker.returnOK;

      return new Promise(resolve => {
        MockFilePicker.afterOpenCallback = resolve;
        EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);
      });
    }).then(() => {
      is(MockFilePicker.mode, Ci.nsIFilePicker.modeGetFolder, "File picker mode should be directory");
      is(input.value, profD.path, "Label value should match file chosen");
      is(input.tooltipText, profD.path, "Label tooltip should match file chosen");
      is(Services.prefs.getCharPref("extensions.inlinesettings1.directory"), profD.path, "Directory pref should match file chosen");

      MockFilePicker.setFiles([curProcD]);
      MockFilePicker.returnValue = Ci.nsIFilePicker.returnCancel;

      return new Promise(resolve => {
        MockFilePicker.afterOpenCallback = resolve;
        EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);
      });
    }).then(() => {
      is(MockFilePicker.mode, Ci.nsIFilePicker.modeGetFolder, "File picker mode should be directory");
      is(input.value, profD.path, "Label value should not have changed");
      is(input.tooltipText, profD.path, "Label tooltip should not have changed");
      is(Services.prefs.getCharPref("extensions.inlinesettings1.directory"), profD.path, "Directory pref should not have changed");
    }).then(() => {
      button = gManagerWindow.document.getElementById("detail-prefs-btn");
      is_element_hidden(button, "Preferences button should not be visible");

      gCategoryUtilities.openType("extension", run_next_test);
    });
  });
});

// Tests for the setting.xml bindings introduced after Mozilla 7
add_test(function() {
  observer.checkHidden("inlinesettings1@tests.mozilla.org");

  var addon = get_addon_element(gManagerWindow, "inlinesettings3@tests.mozilla.org");
  addon.parentNode.ensureElementIsVisible(addon);

  var button = gManagerWindow.document.getAnonymousElementByAttribute(addon, "anonid", "details-btn");
  EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);

  wait_for_view_load(gManagerWindow, function() {
    observer.checkDisplayed("inlinesettings3@tests.mozilla.org");

    var grid = gManagerWindow.document.getElementById("detail-grid");
    var settings = grid.querySelectorAll("rows > setting");
    is(settings.length, 4, "Grid should have settings children");

    ok(settings[0].hasAttribute("first-row"), "First visible row should have first-row attribute");
    Services.prefs.setBoolPref("extensions.inlinesettings3.radioBool", false);
    var radios = settings[0].getElementsByTagName("radio");
    isnot(radios[0].selected, true, "Correct radio button should be selected");
    is(radios[1].selected, true, "Correct radio button should be selected");
    EventUtils.synthesizeMouseAtCenter(radios[0], { clickCount: 1 }, gManagerWindow);
    is(Services.prefs.getBoolPref("extensions.inlinesettings3.radioBool"), true, "Radio pref should have been updated");
    EventUtils.synthesizeMouseAtCenter(radios[1], { clickCount: 1 }, gManagerWindow);
    is(Services.prefs.getBoolPref("extensions.inlinesettings3.radioBool"), false, "Radio pref should have been updated");

    ok(!settings[1].hasAttribute("first-row"), "Not the first row");
    Services.prefs.setIntPref("extensions.inlinesettings3.radioInt", 5);
    radios = settings[1].getElementsByTagName("radio");
    isnot(radios[0].selected, true, "Correct radio button should be selected");
    is(radios[1].selected, true, "Correct radio button should be selected");
    isnot(radios[2].selected, true, "Correct radio button should be selected");
    EventUtils.synthesizeMouseAtCenter(radios[0], { clickCount: 1 }, gManagerWindow);
    is(Services.prefs.getIntPref("extensions.inlinesettings3.radioInt"), 4, "Radio pref should have been updated");
    EventUtils.synthesizeMouseAtCenter(radios[2], { clickCount: 1 }, gManagerWindow);
    is(Services.prefs.getIntPref("extensions.inlinesettings3.radioInt"), 6, "Radio pref should have been updated");

    ok(!settings[2].hasAttribute("first-row"), "Not the first row");
    Services.prefs.setCharPref("extensions.inlinesettings3.radioString", "juliet");
    radios = settings[2].getElementsByTagName("radio");
    isnot(radios[0].selected, true, "Correct radio button should be selected");
    is(radios[1].selected, true, "Correct radio button should be selected");
    isnot(radios[2].selected, true, "Correct radio button should be selected");
    EventUtils.synthesizeMouseAtCenter(radios[0], { clickCount: 1 }, gManagerWindow);
    is(Services.prefs.getCharPref("extensions.inlinesettings3.radioString"), "india", "Radio pref should have been updated");
    EventUtils.synthesizeMouseAtCenter(radios[2], { clickCount: 1 }, gManagerWindow);
    is(Preferences.get("extensions.inlinesettings3.radioString", "wrong"), "kilo \u338F", "Radio pref should have been updated");

    ok(!settings[3].hasAttribute("first-row"), "Not the first row");
    Services.prefs.setIntPref("extensions.inlinesettings3.menulist", 8);
    var input = settings[3].firstElementChild;
    is(input.value, "8", "Menulist should have initial value");
    input.focus();
    EventUtils.synthesizeKey("n", {}, gManagerWindow);
    is(input.value, "9", "Menulist should have updated value");
    is(Services.prefs.getIntPref("extensions.inlinesettings3.menulist"), 9, "Menulist pref should have been updated");

    button = gManagerWindow.document.getElementById("detail-prefs-btn");
    is_element_hidden(button, "Preferences button should not be visible");

    gCategoryUtilities.openType("extension", run_next_test);
  });
});

// Addon with inline preferences as optionsURL
add_test(function() {
  observer.checkHidden("inlinesettings3@tests.mozilla.org");

  var addon = get_addon_element(gManagerWindow, "inlinesettings2@tests.mozilla.org");
  addon.parentNode.ensureElementIsVisible(addon);

  var button = gManagerWindow.document.getAnonymousElementByAttribute(addon, "anonid", "details-btn");
  EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);

  wait_for_view_load(gManagerWindow, function() {
    observer.checkDisplayed("inlinesettings2@tests.mozilla.org");

    var grid = gManagerWindow.document.getElementById("detail-grid");
    var settings = grid.querySelectorAll("rows > setting");
    is(settings.length, 5, "Grid should have settings children");

    var node = settings[0];
    node = settings[0];
    is_element_hidden(node, "Unsupported settings should not be visible");
    ok(!node.hasAttribute("first-row"), "Hidden row is not the first row");

    node = settings[1];
    is(node.nodeName, "setting", "Should be a setting node");
    ok(node.hasAttribute("first-row"), "First visible row should have first-row attribute");
    var description = gManagerWindow.document.getAnonymousElementByAttribute(node, "class", "preferences-description");
    is(description.textContent, "Description Attribute", "Description node should contain description");

    node = settings[2];
    is(node.nodeName, "setting", "Should be a setting node");
    ok(!node.hasAttribute("first-row"), "Not the first row");
    description = gManagerWindow.document.getAnonymousElementByAttribute(node, "class", "preferences-description");
    is(description.textContent, "Description Text Node", "Description node should contain description");

    node = settings[3];
    is(node.nodeName, "setting", "Should be a setting node");
    ok(!node.hasAttribute("first-row"), "Not the first row");
    description = gManagerWindow.document.getAnonymousElementByAttribute(node, "class", "preferences-description");
    is(description.textContent, "This is a test, all this text should be visible", "Description node should contain description");
    var button = node.firstElementChild;
    isnot(button, null, "There should be a button");

    node = settings[4];
    is_element_hidden(node, "Unsupported settings should not be visible");
    ok(!node.hasAttribute("first-row"), "Hidden row is not the first row");

    button = gManagerWindow.document.getElementById("detail-prefs-btn");
    is_element_hidden(button, "Preferences button should not be visible");

    gCategoryUtilities.openType("extension", run_next_test);
  });
});

// Addon with non-inline preferences as optionsURL
add_test(function() {
  observer.checkHidden("inlinesettings2@tests.mozilla.org");

  var addon = get_addon_element(gManagerWindow, "noninlinesettings@tests.mozilla.org");
  addon.parentNode.ensureElementIsVisible(addon);

  var button = gManagerWindow.document.getAnonymousElementByAttribute(addon, "anonid", "details-btn");
  EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);

  wait_for_view_load(gManagerWindow, function() {
    observer.checkNotDisplayed();

    var grid = gManagerWindow.document.getElementById("detail-grid");
    var settings = grid.querySelectorAll("rows > setting");
    is(settings.length, 0, "Grid should not have settings children");

    var button = gManagerWindow.document.getElementById("detail-prefs-btn");
    is_element_visible(button, "Preferences button should be visible");

    gCategoryUtilities.openType("extension", run_next_test);
  });
});

// Addon with options.xul, disabling and enabling should hide and show settings UI
add_test(function() {
  observer.checkNotHidden();

  var addon = get_addon_element(gManagerWindow, "inlinesettings1@tests.mozilla.org");
  addon.parentNode.ensureElementIsVisible(addon);

  var button = gManagerWindow.document.getAnonymousElementByAttribute(addon, "anonid", "details-btn");
  EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);

  wait_for_view_load(gManagerWindow, function() {
    observer.checkDisplayed("inlinesettings1@tests.mozilla.org");
    is(gManagerWindow.gViewController.currentViewId,
       "addons://detail/inlinesettings1%40tests.mozilla.org",
       "Current view should not scroll to preferences");
    checkScrolling(false);

    var grid = gManagerWindow.document.getElementById("detail-grid");
    var settings = grid.querySelectorAll("rows > setting");
    is(settings.length, SETTINGS_ROWS, "Grid should have settings children");

    // disable
    var button = gManagerWindow.document.getElementById("detail-disable-btn");
    button.focus(); // make sure it's in view
    EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);

    observer.checkHidden("inlinesettings1@tests.mozilla.org");

    settings = grid.querySelectorAll("rows > setting");
    is(settings.length, 0, "Grid should not have settings children");

    gCategoryUtilities.openType("extension", function() {
      var addon = get_addon_element(gManagerWindow, "inlinesettings1@tests.mozilla.org");
      addon.parentNode.ensureElementIsVisible(addon);

      var button = gManagerWindow.document.getAnonymousElementByAttribute(addon, "anonid", "preferences-btn");
      is_element_hidden(button, "Preferences button should not be visible");

      button = gManagerWindow.document.getAnonymousElementByAttribute(addon, "anonid", "details-btn");
      EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);

      wait_for_view_load(gManagerWindow, function() {
        var grid = gManagerWindow.document.getElementById("detail-grid");
        var settings = grid.querySelectorAll("rows > setting");
        is(settings.length, 0, "Grid should not have settings children");

        // enable
        var button = gManagerWindow.document.getElementById("detail-enable-btn");
        EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);

        observer.callback = function() {
          observer.checkDisplayed("inlinesettings1@tests.mozilla.org");

          settings = grid.querySelectorAll("rows > setting");
          is(settings.length, SETTINGS_ROWS, "Grid should have settings children");

          gCategoryUtilities.openType("extension", run_next_test);
        };
      });
    });
  });
});


// Addon with options.xul that requires a restart to disable,
// disabling and enabling should not hide and show settings UI.
add_test(function() {
  observer.checkHidden("inlinesettings1@tests.mozilla.org");

  var addon = get_addon_element(gManagerWindow, "inlinesettings2@tests.mozilla.org");
  addon.parentNode.ensureElementIsVisible(addon);

  var button = gManagerWindow.document.getAnonymousElementByAttribute(addon, "anonid", "details-btn");
  EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);

  wait_for_view_load(gManagerWindow, function() {
    observer.checkDisplayed("inlinesettings2@tests.mozilla.org");

    var grid = gManagerWindow.document.getElementById("detail-grid");
    var settings = grid.querySelectorAll("rows > setting");
    ok(settings.length > 0, "Grid should have settings children");

    // disable
    var button = gManagerWindow.document.getElementById("detail-disable-btn");
    button.focus(); // make sure it's in view
    EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);
    observer.checkNotHidden();

    settings = grid.querySelectorAll("rows > setting");
    ok(settings.length > 0, "Grid should still have settings children");

    // cancel pending disable
    button = gManagerWindow.document.getElementById("detail-enable-btn");
    button.focus(); // make sure it's in view
    EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);
    observer.checkNotDisplayed();

    gCategoryUtilities.openType("extension", run_next_test);
  });
});
