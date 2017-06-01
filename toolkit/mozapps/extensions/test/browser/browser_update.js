/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that updates correctly flush caches and that new files gets updated.

function test() {
  requestLongerTimeout(2);
  waitForExplicitFinish();

  SpecialPowers.pushPrefEnv({"set": [
    ["extensions.checkUpdateSecurity", false],
  ]});

  run_next_test();
}

// Install a first version
add_test(function() {
  AddonManager.getInstallForURL(TESTROOT + "addons/browser_update1_1.xpi",
                                function(aInstall) {
    aInstall.install();
  }, "application/x-xpinstall");

  Services.ppmm.addMessageListener("my-addon-1", function messageListener() {
    Services.ppmm.removeMessageListener("my-addon-1", messageListener);
    ok(true, "first version sent frame script message");
    run_next_test();
  });
});

// Update to a second version and verify that content gets updated
add_test(function() {
  AddonManager.getInstallForURL(TESTROOT + "addons/browser_update1_2.xpi",
                                function(aInstall) {
    aInstall.install();
  }, "application/x-xpinstall");

  Services.ppmm.addMessageListener("my-addon-2", function messageListener() {
    Services.ppmm.removeMessageListener("my-addon-2", messageListener);
    ok(true, "second version sent frame script message");
    run_next_test();
  });
});

// Finally, cleanup things
add_test(function() {
  AddonManager.getAddonByID("update1@tests.mozilla.org", function(aAddon) {
    aAddon.uninstall();

    finish();
  });
});
