// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

/* import-globals-from head_addons.js */

Services.prefs.setBoolPref(
  "security.turn_off_all_security_so_that_viruses_can_take_over_this_computer",
  true
);

// Enable all scopes.
Services.prefs.setIntPref("extensions.enabledScopes", AddonManager.SCOPE_ALL);
// Setting this to all enables the same behavior as before disabling sideloading.
// We reset this later after doing some legacy sideloading.
Services.prefs.setIntPref("extensions.sideloadScopes", AddonManager.SCOPE_ALL);
// AddonTestUtils sets this to zero, we need the default value.
Services.prefs.clearUserPref("extensions.autoDisableScopes");

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

function getID(n) {
  return `${n}@tests.mozilla.org`;
}
function initialVersion(n) {
  return `${n}.0`;
}

// Setup some common extension locations, one in each scope.

// SCOPE_SYSTEM
const globalDir = gProfD.clone();
globalDir.append("app-system-share");
globalDir.append(gAppInfo.ID);
registerDirectory("XRESysSExtPD", globalDir.parent);

// SCOPE_USER
const userDir = gProfD.clone();
userDir.append("app-system-user");
userDir.append(gAppInfo.ID);
registerDirectory("XREUSysExt", userDir.parent);

// SCOPE_APPLICATION
const addonAppDir = gProfD.clone();
addonAppDir.append("app-global");
addonAppDir.append("extensions");
registerDirectory("XREAddonAppDir", addonAppDir.parent);

// SCOPE_PROFILE
const profileDir = gProfD.clone();
profileDir.append("extensions");

const scopeDirectories = {
  global: globalDir,
  user: userDir,
  app: addonAppDir,
  profile: profileDir,
};

const scopeToDir = new Map([
  [AddonManager.SCOPE_SYSTEM, globalDir],
  [AddonManager.SCOPE_USER, userDir],
  [AddonManager.SCOPE_APPLICATION, addonAppDir],
  [AddonManager.SCOPE_PROFILE, profileDir],
]);

async function createWebExtension(id, version, dir) {
  let xpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version,
      applications: { gecko: { id } },
    },
  });
  await AddonTestUtils.manuallyInstall(xpi, dir);
}

function check_startup_changes(aType, aIds) {
  let changes = AddonManager.getStartupChanges(aType);
  changes = changes.filter(aEl => /@tests.mozilla.org$/.test(aEl));

  Assert.deepEqual([...aIds].sort(), changes.sort());
}
