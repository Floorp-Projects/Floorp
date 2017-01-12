// Enable signature checks for these tests
gUseRealCertChecks = true;
// Disable update security
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

const DATA = "data/signing_checks/";
const GOOD = [
  ["signed_bootstrap_2.xpi", AddonManager.SIGNEDSTATE_SIGNED],
  ["signed_nonbootstrap_2.xpi", AddonManager.SIGNEDSTATE_SIGNED]
];
const BAD = [
  ["unsigned_bootstrap_2.xpi", AddonManager.SIGNEDSTATE_MISSING],
  ["signed_bootstrap_badid_2.xpi", AddonManager.SIGNEDSTATE_BROKEN],
  ["unsigned_nonbootstrap_2.xpi", AddonManager.SIGNEDSTATE_MISSING],
  ["signed_nonbootstrap_badid_2.xpi", AddonManager.SIGNEDSTATE_BROKEN],
];
const ID = "test@tests.mozilla.org";

const profileDir = gProfD.clone();
profileDir.append("extensions");

function verifySignatures() {
  return new Promise(resolve => {
    let observer = (subject, topic, data) => {
      Services.obs.removeObserver(observer, "xpi-signature-changed");
      resolve(JSON.parse(data));
    }
    Services.obs.addObserver(observer, "xpi-signature-changed", false);

    do_print("Verifying signatures");
    let XPIscope = Components.utils.import("resource://gre/modules/addons/XPIProvider.jsm", {});
    XPIscope.XPIProvider.verifySignatures();
  });
}

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "4", "4");

  run_next_test();
}

function verify_no_change([startFile, startState], [endFile, endState]) {
  add_task(function*() {
    do_print("A switch from " + startFile + " to " + endFile + " should cause no change.");

    // Install the first add-on
    manuallyInstall(do_get_file(DATA + startFile), profileDir, ID);
    startupManager();

    let addon = yield promiseAddonByID(ID);
    do_check_neq(addon, null);
    let wasAppDisabled = addon.appDisabled;
    do_check_neq(addon.appDisabled, addon.isActive);
    do_check_eq(addon.pendingOperations, AddonManager.PENDING_NONE);
    do_check_eq(addon.signedState, startState);

    // Swap in the files from the next add-on
    manuallyUninstall(profileDir, ID);
    manuallyInstall(do_get_file(DATA + endFile), profileDir, ID);

    let events = {
      [ID]: []
    };

    if (startState != endState)
      events[ID].unshift(["onPropertyChanged", ["signedState"]]);

    prepare_test(events);

    // Trigger the check
    let changes = yield verifySignatures();
    do_check_eq(changes.enabled.length, 0);
    do_check_eq(changes.disabled.length, 0);

    do_check_eq(addon.appDisabled, wasAppDisabled);
    do_check_neq(addon.appDisabled, addon.isActive);
    do_check_eq(addon.pendingOperations, AddonManager.PENDING_NONE);
    do_check_eq(addon.signedState, endState);

    // Remove the add-on and restart to let it go away
    manuallyUninstall(profileDir, ID);
    yield promiseRestartManager();
    yield promiseShutdownManager();
  });
}

function verify_enables([startFile, startState], [endFile, endState]) {
  add_task(function*() {
    do_print("A switch from " + startFile + " to " + endFile + " should enable the add-on.");

    // Install the first add-on
    manuallyInstall(do_get_file(DATA + startFile), profileDir, ID);
    startupManager();

    let addon = yield promiseAddonByID(ID);
    do_check_neq(addon, null);
    do_check_false(addon.isActive);
    do_check_eq(addon.pendingOperations, AddonManager.PENDING_NONE);
    do_check_eq(addon.signedState, startState);

    // Swap in the files from the next add-on
    manuallyUninstall(profileDir, ID);
    manuallyInstall(do_get_file(DATA + endFile), profileDir, ID);

    let needsRestart = hasFlag(addon.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_ENABLE);
    do_print(needsRestart);

    let events = {};
    if (!needsRestart) {
      events[ID] = [
        ["onPropertyChanged", ["appDisabled"]],
        ["onEnabling", false],
        "onEnabled"
      ];
    } else {
      events[ID] = [
        ["onPropertyChanged", ["appDisabled"]],
        "onEnabling"
      ];
    }

    if (startState != endState)
      events[ID].unshift(["onPropertyChanged", ["signedState"]]);

    prepare_test(events);

    // Trigger the check
    let changes = yield verifySignatures();
    do_check_eq(changes.enabled.length, 1);
    do_check_eq(changes.enabled[0], ID);
    do_check_eq(changes.disabled.length, 0);

    do_check_false(addon.appDisabled);
    if (needsRestart)
      do_check_neq(addon.pendingOperations, AddonManager.PENDING_NONE);
    else
      do_check_true(addon.isActive);
    do_check_eq(addon.signedState, endState);

    ensure_test_completed();

    // Remove the add-on and restart to let it go away
    manuallyUninstall(profileDir, ID);
    yield promiseRestartManager();
    yield promiseShutdownManager();
  });
}

function verify_disables([startFile, startState], [endFile, endState]) {
  add_task(function*() {
    do_print("A switch from " + startFile + " to " + endFile + " should disable the add-on.");

    // Install the first add-on
    manuallyInstall(do_get_file(DATA + startFile), profileDir, ID);
    startupManager();

    let addon = yield promiseAddonByID(ID);
    do_check_neq(addon, null);
    do_check_true(addon.isActive);
    do_check_eq(addon.pendingOperations, AddonManager.PENDING_NONE);
    do_check_eq(addon.signedState, startState);

    let needsRestart = hasFlag(addon.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_DISABLE);

    // Swap in the files from the next add-on
    manuallyUninstall(profileDir, ID);
    manuallyInstall(do_get_file(DATA + endFile), profileDir, ID);

    let events = {};
    if (!needsRestart) {
      events[ID] = [
        ["onPropertyChanged", ["appDisabled"]],
        ["onDisabling", false],
        "onDisabled"
      ];
    } else {
      events[ID] = [
        ["onPropertyChanged", ["appDisabled"]],
        "onDisabling"
      ];
    }

    if (startState != endState)
      events[ID].unshift(["onPropertyChanged", ["signedState"]]);

    prepare_test(events);

    // Trigger the check
    let changes = yield verifySignatures();
    do_check_eq(changes.enabled.length, 0);
    do_check_eq(changes.disabled.length, 1);
    do_check_eq(changes.disabled[0], ID);

    do_check_true(addon.appDisabled);
    if (needsRestart)
      do_check_neq(addon.pendingOperations, AddonManager.PENDING_NONE);
    else
      do_check_false(addon.isActive);
    do_check_eq(addon.signedState, endState);

    ensure_test_completed();

    // Remove the add-on and restart to let it go away
    manuallyUninstall(profileDir, ID);
    yield promiseRestartManager();
    yield promiseShutdownManager();
  });
}

for (let start of GOOD) {
  for (let end of BAD) {
    verify_disables(start, end);
  }
}

for (let start of BAD) {
  for (let end of GOOD) {
    verify_enables(start, end);
  }
}

for (let start of GOOD) {
  for (let end of GOOD.filter(f => f != start)) {
    verify_no_change(start, end);
  }
}

for (let start of BAD) {
  for (let end of BAD.filter(f => f != start)) {
    verify_no_change(start, end);
  }
}
