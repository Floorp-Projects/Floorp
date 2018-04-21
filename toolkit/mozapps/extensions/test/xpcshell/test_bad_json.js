/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that we rebuild the database correctly if it contains
// JSON data that parses correctly but doesn't contain required fields

var addon1 = {
  id: "addon1@tests.mozilla.org",
  version: "2.0",
  name: "Test 1",
  bootstrap: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

const profileDir = gProfD.clone();
profileDir.append("extensions");

add_task(async function() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  // This addon will be auto-installed at startup
  writeInstallRDFForExtension(addon1, profileDir);

  await promiseStartupManager();
  await promiseShutdownManager();

  // First startup/shutdown finished
  // Replace the JSON store with something bogus
  await saveJSON({not: "what we expect to find"}, gExtensionsJSON.path);

  await promiseStartupManager(false);
  // Retrieve an addon to force the database to rebuild
  let a1 = await AddonManager.getAddonByID(addon1.id);

  Assert.equal(a1.id, addon1.id);

  await promiseShutdownManager();

  // Make sure our JSON database has schemaVersion and our installed extension
  let data = await loadJSON(gExtensionsJSON.path);
  Assert.ok("schemaVersion" in data);
  Assert.equal(data.addons[0].id, addon1.id);
});
