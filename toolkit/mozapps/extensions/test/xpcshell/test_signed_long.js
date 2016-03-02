// Disable update security
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

gUseRealCertChecks = true;

const DATA = "data/signing_checks/";

const ID_63 = "123456789012345678901234567890123456789012345@tests.mozilla.org"
const ID_64 = "1234567890123456789012345678901234567890123456@tests.mozilla.org"
const ID_65 = "12345678901234567890123456789012345678901234568@tests.mozilla.org"

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
  startupManager();

  run_next_test();
}

// Installs the cases that should be working
add_task(function* test_working() {
  yield promiseInstallAllFiles([do_get_file(DATA + "long_63_plain.xpi"),
                                do_get_file(DATA + "long_64_plain.xpi"),
                                do_get_file(DATA + "long_65_hash.xpi")]);

  let addons = yield promiseAddonsByIDs([ID_63, ID_64, ID_65]);

  for (let addon of addons) {
    do_check_neq(addon, null);
    do_check_true(addon.signedState > AddonManager.SIGNEDSTATE_MISSING);

    addon.uninstall();
  }
});

// Checks the cases that should be broken
add_task(function* test_broken() {
  function promiseInstallForFile(file) {
    return new Promise(resolve => AddonManager.getInstallForFile(file, resolve));
  }

  let promises = [promiseInstallForFile(do_get_file(DATA + "long_63_hash.xpi")),
                  promiseInstallForFile(do_get_file(DATA + "long_64_hash.xpi"))];
  let installs = yield Promise.all(promises);

  for (let install of installs) {
    do_check_eq(install.state, AddonManager.STATE_DOWNLOAD_FAILED);
    do_check_eq(install.error, AddonManager.ERROR_CORRUPT_FILE);
  }
});
