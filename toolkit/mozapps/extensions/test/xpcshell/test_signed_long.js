// Disable update security
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

gUseRealCertChecks = true;

const DATA = "data/signing_checks/";

const ID_63 = "123456789012345678901234567890123456789012345@tests.mozilla.org";
const ID_64 = "1234567890123456789012345678901234567890123456@tests.mozilla.org";
const ID_65 = "12345678901234567890123456789012345678901234568@tests.mozilla.org";

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
  startupManager();

  run_next_test();
}

// Installs the cases that should be working
add_task(async function test_working() {
  await promiseInstallAllFiles([do_get_file(DATA + "long_63_plain.xpi"),
                                do_get_file(DATA + "long_64_plain.xpi"),
                                do_get_file(DATA + "long_65_hash.xpi")]);

  let addons = await promiseAddonsByIDs([ID_63, ID_64, ID_65]);

  for (let addon of addons) {
    Assert.notEqual(addon, null);
    Assert.ok(addon.signedState > AddonManager.SIGNEDSTATE_MISSING);

    addon.uninstall();
  }
});

// Checks the cases that should be broken
add_task(async function test_broken() {
  let promises = [AddonManager.getInstallForFile(do_get_file(DATA + "long_63_hash.xpi")),
                  AddonManager.getInstallForFile(do_get_file(DATA + "long_64_hash.xpi"))];
  let installs = await Promise.all(promises);

  for (let install of installs) {
    Assert.equal(install.state, AddonManager.STATE_DOWNLOAD_FAILED);
    Assert.equal(install.error, AddonManager.ERROR_CORRUPT_FILE);
  }
});
