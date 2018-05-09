
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

// Enable signature checks for these tests
gUseRealCertChecks = true;
// Disable update security
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

const DATA = "data/signing_checks/";
let GOOD = [
  ["privileged_bootstrap_2.xpi", AddonManager.SIGNEDSTATE_PRIVILEGED],
];
if (AppConstants.MOZ_ALLOW_LEGACY_EXTENSIONS) {
  GOOD.push(
    ["signed_bootstrap_2.xpi", AddonManager.SIGNEDSTATE_SIGNED],
  );
}

const BAD = [
  ["unsigned_bootstrap_2.xpi", AddonManager.SIGNEDSTATE_MISSING],
  ["signed_bootstrap_badid_2.xpi", AddonManager.SIGNEDSTATE_BROKEN],
];
const ID = "test@tests.mozilla.org";

const profileDir = gProfD.clone();
profileDir.append("extensions");

function verifySignatures() {
  return new Promise(resolve => {
    let observer = (subject, topic, data) => {
      Services.obs.removeObserver(observer, "xpi-signature-changed");
      resolve(JSON.parse(data));
    };
    Services.obs.addObserver(observer, "xpi-signature-changed");

    info("Verifying signatures");
    let XPIscope = ChromeUtils.import("resource://gre/modules/addons/XPIProvider.jsm", {});
    XPIscope.XPIProvider.verifySignatures();
  });
}

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "4", "4");

  run_next_test();
}

function verify_no_change([startFile, startState], [endFile, endState]) {
  add_task(async function() {
    info("A switch from " + startFile + " to " + endFile + " should cause no change.");

    // Install the first add-on
    await manuallyInstall(do_get_file(DATA + startFile), profileDir, ID);
    await promiseStartupManager();

    let addon = await promiseAddonByID(ID);
    Assert.notEqual(addon, null);
    let wasAppDisabled = addon.appDisabled;
    Assert.notEqual(addon.appDisabled, addon.isActive);
    Assert.equal(addon.pendingOperations, AddonManager.PENDING_NONE);
    Assert.equal(addon.signedState, startState);

    // Swap in the files from the next add-on
    manuallyUninstall(profileDir, ID);
    await manuallyInstall(do_get_file(DATA + endFile), profileDir, ID);

    let events = {
      [ID]: []
    };

    if (startState != endState)
      events[ID].unshift(["onPropertyChanged", ["signedState"]]);

    prepare_test(events);

    // Trigger the check
    let changes = await verifySignatures();
    Assert.equal(changes.enabled.length, 0);
    Assert.equal(changes.disabled.length, 0);

    Assert.equal(addon.appDisabled, wasAppDisabled);
    Assert.notEqual(addon.appDisabled, addon.isActive);
    Assert.equal(addon.pendingOperations, AddonManager.PENDING_NONE);
    Assert.equal(addon.signedState, endState);

    // Remove the add-on and restart to let it go away
    manuallyUninstall(profileDir, ID);
    await promiseRestartManager();
    await promiseShutdownManager();
  });
}

function verify_enables([startFile, startState], [endFile, endState]) {
  add_task(async function() {
    info("A switch from " + startFile + " to " + endFile + " should enable the add-on.");

    // Install the first add-on
    await manuallyInstall(do_get_file(DATA + startFile), profileDir, ID);
    await promiseStartupManager();

    let addon = await promiseAddonByID(ID);
    Assert.notEqual(addon, null);
    Assert.ok(!addon.isActive);
    Assert.equal(addon.pendingOperations, AddonManager.PENDING_NONE);
    Assert.equal(addon.signedState, startState);

    // Swap in the files from the next add-on
    manuallyUninstall(profileDir, ID);
    await manuallyInstall(do_get_file(DATA + endFile), profileDir, ID);

    let needsRestart = hasFlag(addon.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_ENABLE);
    info(needsRestart);

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
    let changes = await verifySignatures();
    Assert.equal(changes.enabled.length, 1);
    Assert.equal(changes.enabled[0], ID);
    Assert.equal(changes.disabled.length, 0);

    Assert.ok(!addon.appDisabled);
    if (needsRestart)
      Assert.notEqual(addon.pendingOperations, AddonManager.PENDING_NONE);
    else
      Assert.ok(addon.isActive);
    Assert.equal(addon.signedState, endState);

    ensure_test_completed();

    // Remove the add-on and restart to let it go away
    manuallyUninstall(profileDir, ID);
    await promiseRestartManager();
    await promiseShutdownManager();
  });
}

function verify_disables([startFile, startState], [endFile, endState]) {
  add_task(async function() {
    info("A switch from " + startFile + " to " + endFile + " should disable the add-on.");

    // Install the first add-on
    await manuallyInstall(do_get_file(DATA + startFile), profileDir, ID);
    await promiseStartupManager();

    let addon = await promiseAddonByID(ID);
    Assert.notEqual(addon, null);
    Assert.ok(addon.isActive);
    Assert.equal(addon.pendingOperations, AddonManager.PENDING_NONE);
    Assert.equal(addon.signedState, startState);

    let needsRestart = hasFlag(addon.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_DISABLE);

    // Swap in the files from the next add-on
    manuallyUninstall(profileDir, ID);
    await manuallyInstall(do_get_file(DATA + endFile), profileDir, ID);

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
    let changes = await verifySignatures();
    Assert.equal(changes.enabled.length, 0);
    Assert.equal(changes.disabled.length, 1);
    Assert.equal(changes.disabled[0], ID);

    Assert.ok(addon.appDisabled);
    if (needsRestart)
      Assert.notEqual(addon.pendingOperations, AddonManager.PENDING_NONE);
    else
      Assert.ok(!addon.isActive);
    Assert.equal(addon.signedState, endState);

    ensure_test_completed();

    // Remove the add-on and restart to let it go away
    manuallyUninstall(profileDir, ID);
    await promiseRestartManager();
    await promiseShutdownManager();
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
