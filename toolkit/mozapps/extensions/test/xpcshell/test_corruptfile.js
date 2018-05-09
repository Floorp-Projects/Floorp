/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that attempting to install a corrupt XPI file doesn't break the universe

const profileDir = gProfD.clone();
profileDir.append("extensions");

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  await promiseStartupManager();

  if (TEST_UNPACKED)
    return run_test_unpacked();
  return run_test_packed();
});

// When installing packed we won't detect corruption in the XPI until we attempt
// to load bootstrap.js so everything will look normal from the outside.
async function run_test_packed() {
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

  await promiseInstallAllFiles([do_get_file("data/corruptfile.xpi")]);
  ensure_test_completed();

  let addon = await AddonManager.getAddonByID("corrupt@tests.mozilla.org");
  Assert.notEqual(addon, null);
}

// When extracting the corruption will be detected and the add-on fails to
// install
async function run_test_unpacked() {
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

  await promiseInstallAllFiles([do_get_file("data/corruptfile.xpi")]);
  ensure_test_completed();

  // Check the add-on directory isn't left over
  var addonDir = profileDir.clone();
  addonDir.append("corrupt@tests.mozilla.org");
  pathShouldntExist(addonDir);

  // Check the staging directory isn't left over
  var stageDir = profileDir.clone();
  stageDir.append("staged");
  pathShouldntExist(stageDir);

  let addon = await AddonManager.getAddonByID("corrupt@tests.mozilla.org");
  Assert.equal(addon, null);
}
