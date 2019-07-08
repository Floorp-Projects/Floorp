/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that add-on update check correctly fills in the
// %COMPATIBILITY_MODE% token in the update URL.

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

let testserver = createHttpServer({ hosts: ["example.com"] });

let lastMode;
testserver.registerPathHandler("/update.json", (request, response) => {
  let params = new URLSearchParams(request.queryString);
  lastMode = params.get("mode");

  response.setHeader("content-type", "application/json", true);
  response.write(JSON.stringify({ addons: {} }));
});

const ID_NORMAL = "compatmode@tests.mozilla.org";
const ID_STRICT = "compatmode-strict@tests.mozilla.org";

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  let xpi = await createAddon({
    id: ID_NORMAL,
    updateURL: "http://example.com/update.json?mode=%COMPATIBILITY_MODE%",
    targetApplications: [
      {
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1",
      },
    ],
  });
  await manuallyInstall(xpi, AddonTestUtils.profileExtensions, ID_NORMAL);

  xpi = await createAddon({
    id: ID_STRICT,
    updateURL: "http://example.com/update.json?mode=%COMPATIBILITY_MODE%",
    strictCompatibility: true,
    targetApplications: [
      {
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1",
      },
    ],
  });
  await manuallyInstall(xpi, AddonTestUtils.profileExtensions, ID_STRICT);

  await promiseStartupManager();
});

// Strict compatibility checking disabled.
add_task(async function test_strict_disabled() {
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);
  let addon = await AddonManager.getAddonByID(ID_NORMAL);
  Assert.notEqual(addon, null);

  await promiseFindAddonUpdates(addon, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  Assert.equal(
    lastMode,
    "normal",
    "COMPATIBIILITY_MODE normal was set correctly"
  );
});

// Strict compatibility checking enabled.
add_task(async function test_strict_enabled() {
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, true);
  let addon = await AddonManager.getAddonByID(ID_NORMAL);
  Assert.notEqual(addon, null);

  await promiseFindAddonUpdates(addon, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  Assert.equal(
    lastMode,
    "strict",
    "COMPATIBILITY_MODE strict was set correctly"
  );
});

// Strict compatibility checking opt-in.
add_task(async function test_strict_optin() {
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);
  let addon = await AddonManager.getAddonByID(ID_STRICT);
  Assert.notEqual(addon, null);

  await promiseFindAddonUpdates(addon, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  Assert.equal(
    lastMode,
    "normal",
    "COMPATIBILITY_MODE is normal even for an addon with strictCompatibility"
  );
});

// Compatibility checking disabled.
add_task(async function test_compat_disabled() {
  AddonManager.checkCompatibility = false;
  let addon = await AddonManager.getAddonByID(ID_NORMAL);
  Assert.notEqual(addon, null);

  await promiseFindAddonUpdates(addon, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  Assert.equal(
    lastMode,
    "ignore",
    "COMPATIBILITY_MODE ignore was set correctly"
  );
});
