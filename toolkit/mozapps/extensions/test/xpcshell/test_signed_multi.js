// Enable signature checks for these tests
gUseRealCertChecks = true;
// Disable update security
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

const DATA = "data/signing_checks/";

// Each multi-package XPI contains one valid theme and one other add-on that
// has the following error state:
const ADDONS = {
  "multi_signed.xpi": 0,
  "multi_badid.xpi": AddonManager.ERROR_CORRUPT_FILE,
  "multi_broken.xpi": AddonManager.ERROR_CORRUPT_FILE,
  "multi_unsigned.xpi": AddonManager.ERROR_SIGNEDSTATE_REQUIRED,
};

function createInstall(filename) {
  return new Promise(resolve => {
    AddonManager.getInstallForFile(do_get_file(DATA + filename), resolve, "application/x-xpinstall");
  });
}

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "4", "4");
  startupManager();

  run_next_test();
}

function* test_addon(filename) {
  do_print("Testing " + filename);

  let install = yield createInstall(filename);
  do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);
  do_check_eq(install.error, 0);

  do_check_neq(install.linkedInstalls, null);
  do_check_eq(install.linkedInstalls.length, 1);

  let linked = install.linkedInstalls[0];
  do_print(linked.state);
  do_check_eq(linked.error, ADDONS[filename]);
  if (linked.error == 0) {
    do_check_eq(linked.state, AddonManager.STATE_DOWNLOADED);
    linked.cancel();
  }
  else {
    do_check_eq(linked.state, AddonManager.STATE_DOWNLOAD_FAILED);
  }

  install.cancel();
}

for (let filename of Object.keys(ADDONS))
  add_task(test_addon.bind(null, filename));
