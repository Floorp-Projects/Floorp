// Disable update security
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);
gUseRealCertChecks = true;

const DATA = "data/signing_checks/";
const ID = "test@somewhere.com";

let testserver = createHttpServer({ hosts: ["example.com"] });

AddonTestUtils.registerJSON(testserver, "/update.json", {
  addons: {
    [ID]: {
      version: "2.0",
      applications: {
        gecko: {
          strict_min_version: "4",
          strict_max_version: "6",
        },
      },
    },
  },
});

Services.prefs.setCharPref(
  "extensions.update.background.url",
  "http://example.com/update.json"
);

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

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "4", "4");
});

// Updating the pref without changing the app version won't disable add-ons
// immediately but will after a signing check
add_task(async function() {
  Services.prefs.setBoolPref(PREF_XPI_SIGNATURES_REQUIRED, false);
  await promiseStartupManager();

  // Install an unsigned add-on
  await promiseInstallFile(do_get_file(DATA + "unsigned.xpi"));

  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_MISSING);

  await promiseShutdownManager();

  Services.prefs.setBoolPref(PREF_XPI_SIGNATURES_REQUIRED, true);

  await promiseStartupManager();

  addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_MISSING);

  // Update checks shouldn't affect the add-on
  await AddonManagerInternal.backgroundUpdateCheck();
  addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_MISSING);

  let changes = await verifySignatures();

  Assert.equal(changes.disabled.length, 1);
  Assert.equal(changes.disabled[0], ID);

  addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.ok(addon.appDisabled);
  Assert.ok(!addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_MISSING);

  await addon.uninstall();

  await promiseShutdownManager();
});

// Updating the pref with changing the app version will disable add-ons
// immediately
add_task(async function() {
  Services.prefs.setBoolPref(PREF_XPI_SIGNATURES_REQUIRED, false);
  await promiseStartupManager();

  // Install an unsigned add-on
  await promiseInstallFile(do_get_file(DATA + "unsigned.xpi"));

  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_MISSING);

  await promiseShutdownManager();

  Services.prefs.setBoolPref(PREF_XPI_SIGNATURES_REQUIRED, true);
  gAppInfo.version = 5.0;
  await promiseStartupManager();

  addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.ok(addon.appDisabled);
  Assert.ok(!addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_MISSING);

  await addon.uninstall();

  await promiseShutdownManager();
});
