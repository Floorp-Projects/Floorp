/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that add-ons distributed with the application in
// langauge subdirectories correctly get installed

// Allow distributed add-ons to install
Services.prefs.setBoolPref("extensions.installDistroAddons", true);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

const profileDir = gProfD.clone();
profileDir.append("extensions");
const distroDir = gProfD.clone();
distroDir.append("distribution");
distroDir.append("extensions");
registerDirectory("XREAppDist", distroDir.parent);
const enUSDistroDir = distroDir.clone();
enUSDistroDir.append("locale-en-US");
const deDEDistroDir = distroDir.clone();
deDEDistroDir.append("locale-de-DE");
const esESDistroDir = distroDir.clone();
esESDistroDir.append("locale-es-ES");

const enUSID = "addon-en-US@tests.mozilla.org";
const deDEID = "addon-de-DE@tests.mozilla.org";
const esESID = "addon-es-ES@tests.mozilla.org";

async function writeDistroAddons(version) {
  let xpi = await createTempWebExtensionFile({
    manifest: {
      version,
      browser_specific_settings: { gecko: { id: enUSID } },
    },
  });
  xpi.copyTo(enUSDistroDir, `${enUSID}.xpi`);

  xpi = await createTempWebExtensionFile({
    manifest: {
      version,
      browser_specific_settings: { gecko: { id: deDEID } },
    },
  });
  xpi.copyTo(deDEDistroDir, `${deDEID}.xpi`);

  xpi = await createTempWebExtensionFile({
    manifest: {
      version,
      browser_specific_settings: { gecko: { id: esESID } },
    },
  });
  xpi.copyTo(esESDistroDir, `${esESID}.xpi`);
}

add_task(async function setup() {
  await writeDistroAddons("1.0");
});

// Tests that on the first startup the requested locale
// add-on gets installed, and others don't.
add_task(async function run_locale_test() {
  Services.locale.availableLocales = ["de-DE", "en-US"];
  Services.locale.requestedLocales = ["de-DE"];

  Assert.equal(Services.locale.requestedLocale, "de-DE");

  await promiseStartupManager();

  let a1 = await AddonManager.getAddonByID(deDEID);
  Assert.notEqual(a1, null);
  Assert.equal(a1.version, "1.0");
  Assert.ok(a1.isActive);
  Assert.equal(a1.scope, AddonManager.SCOPE_PROFILE);
  Assert.ok(!a1.foreignInstall);

  let a2 = await AddonManager.getAddonByID(enUSID);
  Assert.equal(a2, null);

  let a3 = await AddonManager.getAddonByID(esESID);
  Assert.equal(a3, null);

  await a1.uninstall();
  await promiseShutdownManager();
});

// Tests that on the first startup the correct fallback locale
// add-on gets installed, and others don't.
add_task(async function run_fallback_test() {
  Services.locale.availableLocales = ["es-ES", "en-US"];
  Services.locale.requestedLocales = ["es-UY"];

  Assert.equal(Services.locale.requestedLocale, "es-UY");

  await promiseStartupManager();

  let a1 = await AddonManager.getAddonByID(esESID);
  Assert.notEqual(a1, null);
  Assert.equal(a1.version, "1.0");
  Assert.ok(a1.isActive);
  Assert.equal(a1.scope, AddonManager.SCOPE_PROFILE);
  Assert.ok(!a1.foreignInstall);

  let a2 = await AddonManager.getAddonByID(enUSID);
  Assert.equal(a2, null);

  let a3 = await AddonManager.getAddonByID(deDEID);
  Assert.equal(a3, null);

  await a1.uninstall();
  await promiseShutdownManager();
});
