/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that resource protocol substitutions are set and unset for bootstrapped add-ons.

const profileDir = gProfD.clone();
profileDir.append("extensions");


function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "2");

  let resourceProtocol = Services.io.getProtocolHandler("resource")
                                 .QueryInterface(Components.interfaces.nsIResProtocolHandler);
  startupManager();

  installAllFiles([do_get_addon("test_chromemanifest_6")],
                  function() {

    AddonManager.getAddonByID("addon6@tests.mozilla.org", function(addon) {
      do_check_neq(addon, null);
      do_check_true(addon.isActive);
      do_check_true(resourceProtocol.hasSubstitution("test-addon-1"));

      prepare_test({
        "addon6@tests.mozilla.org": [
          ["onDisabling", false],
          "onDisabled"
        ]
      });

      do_check_eq(addon.operationsRequiringRestart &
                  AddonManager.OP_NEEDS_RESTART_DISABLE, 0);
      addon.userDisabled = true;
      ensure_test_completed();
      do_check_false(resourceProtocol.hasSubstitution("test-addon-1"))

      prepare_test({
        "addon6@tests.mozilla.org": [
          ["onEnabling", false],
          "onEnabled"
        ]
      });

      do_check_eq(addon.operationsRequiringRestart &
                  AddonManager.OP_NEEDS_RESTART_ENABLE, 0);
      addon.userDisabled = false;
      ensure_test_completed();
      do_check_true(resourceProtocol.hasSubstitution("test-addon-1"));

      do_execute_soon(do_test_finished);
    });
  });
}
