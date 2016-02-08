const PREF_XPI_SIGNATURES_DEV_ROOT    = "xpinstall.signatures.dev-root";

// Disable update security
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

// The test add-ons were signed by the dev root
Services.prefs.setBoolPref(PREF_XPI_SIGNATURES_DEV_ROOT, true);

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
    do_check_eq(addon.signedState, AddonManager.SIGNEDSTATE_SIGNED);

    addon.uninstall();
  }
});

// Installs the cases that should be broken
add_task(function* test_broken() {
 yield promiseInstallAllFiles([do_get_file(DATA + "long_63_hash.xpi"),
                               do_get_file(DATA + "long_64_hash.xpi")]);

  let addons = yield promiseAddonsByIDs([ID_63, ID_64]);

  for (let addon of addons) {
    do_check_neq(addon, null);
    do_check_eq(addon.signedState, AddonManager.SIGNEDSTATE_BROKEN);

    addon.uninstall();
  }
});
