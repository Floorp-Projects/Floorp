/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This test verifies that removing a listener during a callback for that type
// of listener still results in all listeners being called.

var addon1 = {
  id: "addon1@tests.mozilla.org",
  version: "2.0",
  name: "Test 1",
  bootstrap: "true",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

var listener1 = {
  sawEvent: false,
  onDisabling() {
    this.sawEvent = true;
    AddonManager.removeAddonListener(this);
  },
  onNewInstall() {
    this.sawEvent = true;
    AddonManager.removeInstallListener(this);
  }
};
var listener2 = {
  sawEvent: false,
  onDisabling() {
    this.sawEvent = true;
  },
  onNewInstall() {
    this.sawEvent = true;
  }
};
var listener3 = {
  sawEvent: false,
  onDisabling() {
    this.sawEvent = true;
  },
  onNewInstall() {
    this.sawEvent = true;
  }
};

const profileDir = gProfD.clone();
profileDir.append("extensions");


function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  writeInstallRDFForExtension(addon1, profileDir);
  startupManager();

  run_test_1();
}

function run_test_1() {
  AddonManager.addAddonListener(listener1);
  AddonManager.addAddonListener(listener2);
  AddonManager.addAddonListener(listener3);

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org"], function([a1]) {
    Assert.notEqual(a1, null);
    Assert.ok(!a1.userDisabled);
    Assert.ok(a1.isActive);

    a1.userDisabled = true;

    Assert.ok(listener1.sawEvent);
    listener1.sawEvent = false;
    Assert.ok(listener2.sawEvent);
    listener2.sawEvent = false;
    Assert.ok(listener3.sawEvent);
    listener3.sawEvent = false;

    AddonManager.removeAddonListener(listener1);
    AddonManager.removeAddonListener(listener2);
    AddonManager.removeAddonListener(listener3);

    a1.uninstall();
    run_test_2();
  });
}

function run_test_2() {
  AddonManager.addInstallListener(listener1);
  AddonManager.addInstallListener(listener2);
  AddonManager.addInstallListener(listener3);

  AddonManager.getInstallForFile(do_get_addon("test_bug757663"), function(aInstall) {

    Assert.ok(listener1.sawEvent);
    listener1.sawEvent = false;
    Assert.ok(listener2.sawEvent);
    listener2.sawEvent = false;
    Assert.ok(listener3.sawEvent);
    listener3.sawEvent = false;

    AddonManager.removeInstallListener(listener1);
    AddonManager.removeInstallListener(listener2);
    AddonManager.removeInstallListener(listener3);

    executeSoon(do_test_finished);
  });
}
