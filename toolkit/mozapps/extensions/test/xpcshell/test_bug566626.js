/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that multiple calls to the async API return fully formed
// add-ons

var addon1 = {
  id: "addon1@tests.mozilla.org",
  version: "1.0",
  name: "Test 1",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

const profileDir = gProfD.clone();
profileDir.append("extensions");

var gAddon;

// Sets up the profile by installing an add-on.
function run_test() {
  do_test_pending();

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  writeInstallRDFForExtension(addon1, profileDir);

  startupManager();

  run_test_1();
}

// Verifies that multiple calls to get an add-on at various stages of execution
// return an add-on with a valid name.
function run_test_1() {
  var count = 0;

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    Assert.notEqual(a1, null);
    Assert.equal(a1.name, "Test 1");

    if (count == 0)
      gAddon = a1;
    else
      Assert.equal(a1, gAddon);
    count++;
    if (count == 4)
      run_test_2();
  });

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    Assert.notEqual(a1, null);
    Assert.equal(a1.name, "Test 1");

    if (count == 0)
      gAddon = a1;
    else
      Assert.equal(a1, gAddon);
    count++;
    if (count == 4)
      run_test_2();
  });

  executeSoon(function() {
    AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
      Assert.notEqual(a1, null);
      Assert.equal(a1.name, "Test 1");

      if (count == 0)
        gAddon = a1;
      else
        Assert.equal(a1, gAddon);
      count++;
      if (count == 4)
        run_test_2();
    });
  });

  executeSoon(function() {
    executeSoon(function() {
      AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
        Assert.notEqual(a1, null);
        Assert.equal(a1.name, "Test 1");

        if (count == 0)
          gAddon = a1;
        else
          Assert.equal(a1, gAddon);
        count++;
        if (count == 4)
          run_test_2();
      });
    });
  });
}

// Verifies that a subsequent call gets the same add-on from the cache
function run_test_2() {
  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    Assert.notEqual(a1, null);
    Assert.equal(a1.name, "Test 1");

    Assert.equal(a1, gAddon);

    executeSoon(do_test_finished);
  });

}
