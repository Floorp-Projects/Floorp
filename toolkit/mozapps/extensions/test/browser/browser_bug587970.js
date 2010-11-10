/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 587970 - Provide ability "Update all now" within 'Available Updates' screen

var gManagerWindow;
var gProvider;

function test() {
  waitForExplicitFinish();
  
  gProvider = new MockProvider();

  gProvider.createAddons([{
    id: "addon1@tests.mozilla.org",
    name: "addon 1",
    version: "1.0",
    applyBackgroundUpdates: AddonManager.AUTOUPDATE_DISABLE
  }, {
    id: "addon2@tests.mozilla.org",
    name: "addon 2",
    version: "2.0",
    applyBackgroundUpdates: AddonManager.AUTOUPDATE_DISABLE
  }, {
    id: "addon3@tests.mozilla.org",
    name: "addon 3",
    version: "3.0",
    applyBackgroundUpdates: AddonManager.AUTOUPDATE_DISABLE
  }]);
  

  open_manager("addons://updates/available", function(aWindow) {
    gManagerWindow = aWindow;
    run_next_test();
  });
}


function end_test() {
  close_manager(gManagerWindow, finish);
}


add_test(function() {
  var list = gManagerWindow.document.getElementById("updates-list");
  is(list.childNodes.length, 0, "Available updates list should be empty");

  var emptyNotice = gManagerWindow.document.getElementById("empty-availableUpdates-msg");
  is_element_visible(emptyNotice, "Empty notice should be visible");
  
  var updateSelected = gManagerWindow.document.getElementById("update-selected-btn");
  is_element_hidden(updateSelected, "Update Selected button should be hidden");

  info("Adding updates");
  gProvider.createInstalls([{
    name: "addon 1",
    version: "1.1",
    existingAddon: gProvider.addons[0]
  }, {
    name: "addon 2",
    version: "2.1",
    existingAddon: gProvider.addons[1]
  }, {
    name: "addon 3",
    version: "3.1",
    existingAddon: gProvider.addons[2]
  }]);

  function wait_for_refresh() {
    if (list.childNodes.length == 3 &&
        list.childNodes[0].mManualUpdate &&
        list.childNodes[1].mManualUpdate &&
        list.childNodes[2].mManualUpdate) {
      run_next_test();
    } else {
      info("Waiting for pane to refresh");
      setTimeout(wait_for_refresh, 10);
    }
  }
  info("Waiting for pane to refresh");
  setTimeout(wait_for_refresh, 10);
});


add_test(function() {
  var list = gManagerWindow.document.getElementById("updates-list");
  is(list.childNodes.length, 3, "Available updates list should have 2 items");
  
  var item1 = get_addon_element(gManagerWindow, "addon1@tests.mozilla.org");
  isnot(item1, null, "Item for addon1@tests.mozilla.org should be in list");
  var item2 = get_addon_element(gManagerWindow, "addon2@tests.mozilla.org");
  isnot(item2, null, "Item for addon2@tests.mozilla.org should be in list");
  var item3 = get_addon_element(gManagerWindow, "addon3@tests.mozilla.org");
  isnot(item3, null, "Item for addon3@tests.mozilla.org should be in list");

  var emptyNotice = gManagerWindow.document.getElementById("empty-availableUpdates-msg");
  is_element_hidden(emptyNotice, "Empty notice should be hidden");

  var updateSelected = gManagerWindow.document.getElementById("update-selected-btn");
  is_element_visible(updateSelected, "Update Selected button should be visible");
  is(updateSelected.disabled, false, "Update Selected button should be enabled by default");

  is(item1._includeUpdate.checked, true, "Include Update checkbox should be checked by default for addon1");
  is(item2._includeUpdate.checked, true, "Include Update checkbox should be checked by default for addon2");
  is(item3._includeUpdate.checked, true, "Include Update checkbox should be checked by default for addon3");
  
  info("Unchecking Include Update checkbox for addon1");
  EventUtils.synthesizeMouse(item1._includeUpdate, 2, 2, { }, gManagerWindow);
  is(item1._includeUpdate.checked, false, "Include Update checkbox should now be be unchecked for addon1");
  is(updateSelected.disabled, false, "Update Selected button should still be enabled");

  info("Unchecking Include Update checkbox for addon2");
  EventUtils.synthesizeMouse(item2._includeUpdate, 2, 2, { }, gManagerWindow);
  is(item2._includeUpdate.checked, false, "Include Update checkbox should now be be unchecked for addon2");
  is(updateSelected.disabled, false, "Update Selected button should still be enabled");

  info("Unchecking Include Update checkbox for addon3");
  EventUtils.synthesizeMouse(item3._includeUpdate, 2, 2, { }, gManagerWindow);
  is(item3._includeUpdate.checked, false, "Include Update checkbox should now be be unchecked for addon3");
  is(updateSelected.disabled, true, "Update Selected button should now be disabled");

  info("Checking Include Update checkbox for addon2");
  EventUtils.synthesizeMouse(item2._includeUpdate, 2, 2, { }, gManagerWindow);
  is(item2._includeUpdate.checked, true, "Include Update checkbox should now be be checked for addon2");
  is(updateSelected.disabled, false, "Update Selected button should now be enabled");

  info("Checking Include Update checkbox for addon3");
  EventUtils.synthesizeMouse(item3._includeUpdate, 2, 2, { }, gManagerWindow);
  is(item3._includeUpdate.checked, true, "Include Update checkbox should now be be checked for addon3");
  is(updateSelected.disabled, false, "Update Selected button should now be enabled");

  var installCount = 0;
  var listener = {
    onDownloadStarted: function(aInstall) {
      isnot(aInstall.existingAddon.id, "addon1@tests.mozilla.org", "Should not have seen a download start for addon1");
    },

    onInstallEnded: function(aInstall) {
      if (++installCount < 2)
        return;

      gProvider.installs[0].removeTestListener(listener);
      gProvider.installs[1].removeTestListener(listener);
      gProvider.installs[2].removeTestListener(listener);

      // Installs are started synchronously so by the time an executeSoon is
      // executed all installs that are going to start will have started
      executeSoon(function() {
        is(gProvider.installs[0].state, AddonManager.STATE_AVAILABLE, "addon1 should not have been upgraded");
        is(gProvider.installs[1].state, AddonManager.STATE_INSTALLED, "addon2 should have been upgraded");
        is(gProvider.installs[2].state, AddonManager.STATE_INSTALLED, "addon3 should have been upgraded");

        run_next_test();
      });
    }
  }
  gProvider.installs[0].addTestListener(listener);
  gProvider.installs[1].addTestListener(listener);
  gProvider.installs[2].addTestListener(listener);
  info("Clicking Update Selected button");
  EventUtils.synthesizeMouseAtCenter(updateSelected, { }, gManagerWindow);
});


add_test(function() {
  var updateSelected = gManagerWindow.document.getElementById("update-selected-btn");
  is(updateSelected.disabled, true, "Update Selected button should be disabled");

  var item1 = get_addon_element(gManagerWindow, "addon1@tests.mozilla.org");
  isnot(item1, null, "Item for addon1@tests.mozilla.org should be in list");
  is(item1._includeUpdate.checked, false, "Include Update checkbox should not have changed");

  info("Checking Include Update checkbox for addon1");
  EventUtils.synthesizeMouse(item1._includeUpdate, 2, 2, { }, gManagerWindow);
  is(item1._includeUpdate.checked, true, "Include Update checkbox should now be be checked for addon1");
  is(updateSelected.disabled, false, "Update Selected button should now not be disabled");

  run_next_test();
});
