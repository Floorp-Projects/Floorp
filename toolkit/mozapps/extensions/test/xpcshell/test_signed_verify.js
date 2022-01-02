// Enable signature checks for these tests
gUseRealCertChecks = true;

const DATA = "data/signing_checks";
const ID = "test@somewhere.com";

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
    let XPIscope = ChromeUtils.import(
      "resource://gre/modules/addons/XPIProvider.jsm",
      null
    );
    XPIscope.XPIDatabase.verifySignatures();
  });
}

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "4", "4");

add_task(async function test_no_change() {
  await promiseStartupManager();

  // Install the first add-on
  await promiseInstallFile(do_get_file(`${DATA}/signed1.xpi`));

  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.appDisabled, false);
  Assert.equal(addon.isActive, true);
  Assert.equal(addon.pendingOperations, AddonManager.PENDING_NONE);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_SIGNED);

  // Swap in the files from the next add-on
  manuallyUninstall(profileDir, ID);
  await manuallyInstall(do_get_file(`${DATA}/signed2.xpi`), profileDir, ID);

  let listener = {
    onPropetyChanged(_addon, properties) {
      Assert.ok(false, `Got unexpected onPropertyChanged for ${_addon.id}`);
    },
  };

  AddonManager.addAddonListener(listener);

  // Trigger the check
  let changes = await verifySignatures();
  Assert.equal(changes.enabled.length, 0);
  Assert.equal(changes.disabled.length, 0);

  Assert.equal(addon.appDisabled, false);
  Assert.equal(addon.isActive, true);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_SIGNED);

  await addon.uninstall();
  AddonManager.removeAddonListener(listener);
});

add_task(async function test_diable() {
  // Install the first add-on
  await promiseInstallFile(do_get_file(`${DATA}/signed1.xpi`));

  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.ok(addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_SIGNED);

  // Swap in the files from the next add-on
  manuallyUninstall(profileDir, ID);
  await manuallyInstall(do_get_file(`${DATA}/unsigned.xpi`), profileDir, ID);

  let changedProperties = [];
  let listener = {
    onPropertyChanged(_, properties) {
      changedProperties.push(...properties);
    },
  };
  AddonManager.addAddonListener(listener);

  // Trigger the check
  let [changes] = await Promise.all([
    verifySignatures(),
    promiseAddonEvent("onDisabling"),
  ]);

  Assert.equal(changes.enabled.length, 0);
  Assert.equal(changes.disabled.length, 1);
  Assert.equal(changes.disabled[0], ID);

  Assert.deepEqual(
    changedProperties,
    ["signedState", "appDisabled"],
    "Got onPropertyChanged events for signedState and appDisabled"
  );

  Assert.ok(addon.appDisabled);
  Assert.ok(!addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_MISSING);

  await addon.uninstall();
  AddonManager.removeAddonListener(listener);
});
