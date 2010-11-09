/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 608316 - Test that cancelling an uninstall during the onUninstalling
// event doesn't confuse the UI

var gProvider;

function test() {
  waitForExplicitFinish();
  
  gProvider = new MockProvider();

  gProvider.createAddons([{
    id: "addon1@tests.mozilla.org",
    name: "addon 1",
    version: "1.0"
  }]);

  run_next_test();
}


function end_test() {
  finish();
}


add_test(function() {
  var sawUninstall = false;
  var listener = {
    onUninstalling: function(aAddon, aRestartRequired) {
      if (aAddon.id != "addon1@tests.mozilla.org")
        return;
      sawUninstall = true;
      aAddon.cancelUninstall();
    }
  }

  // Important to add this before opening the UI so it gets its events first
  AddonManager.addAddonListener(listener);
  registerCleanupFunction(function() {
    AddonManager.removeAddonListener(listener);
  });

  open_manager("addons://list/extension", function(aManager) {
    var addon = get_addon_element(aManager, "addon1@tests.mozilla.org");
    isnot(addon, null, "Should see the add-on in the list");

    var removeBtn = aManager.document.getAnonymousElementByAttribute(addon, "anonid", "remove-btn");
    EventUtils.synthesizeMouseAtCenter(removeBtn, { }, aManager);

    ok(sawUninstall, "Should have seen the uninstall event");
    sawUninstall = false;

    is(addon.getAttribute("pending"), "", "Add-on should not be uninstalling");

    close_manager(aManager, function() {
      ok(!sawUninstall, "Should not have seen another uninstall event");

      run_next_test();
    });
  });
});
