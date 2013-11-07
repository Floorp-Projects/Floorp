/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that attempting to install a corrupt XPI file doesn't break the universe

const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  startupManager();

  if (TEST_UNPACKED)
    run_test_unpacked();
  else
    run_test_packed();
}

// When installing packed we won't detect corruption in the XPI until we attempt
// to load bootstrap.js so everything will look normal from the outside.
function run_test_packed() {
  do_test_pending();

  prepare_test({
    "corrupt@tests.mozilla.org": [
      ["onInstalling", false],
      ["onInstalled", false]
    ]
  }, [
    "onNewInstall",
    "onInstallStarted",
    "onInstallEnded"
  ]);

  installAllFiles([do_get_file("data/corruptfile.xpi")], function() {
    ensure_test_completed();

    AddonManager.getAddonByID("corrupt@tests.mozilla.org", function(addon) {
      do_check_neq(addon, null);

      do_test_finished();
    });
  });
}

// When extracting the corruption will be detected and the add-on fails to
// install
function run_test_unpacked() {
  do_test_pending();

  prepare_test({
    "corrupt@tests.mozilla.org": [
      ["onInstalling", false],
      "onOperationCancelled"
    ]
  }, [
    "onNewInstall",
    "onInstallStarted",
    "onInstallFailed"
  ]);

  installAllFiles([do_get_file("data/corruptfile.xpi")], function() {
    ensure_test_completed();

    // Check the add-on directory isn't left over
    var addonDir = profileDir.clone();
    addonDir.append("corrupt@tests.mozilla.org");
    pathShouldntExist(addonDir);

    // Check the staging directory isn't left over
    var stageDir = profileDir.clone();
    stageDir.append("staged");
    pathShouldntExist(stageDir);

    AddonManager.getAddonByID("corrupt@tests.mozilla.org", function(addon) {
      do_check_eq(addon, null);

      do_test_finished();
    });
  });
}
