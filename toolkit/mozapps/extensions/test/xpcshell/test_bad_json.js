/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that we rebuild the database correctly if it contains
// JSON data that parses correctly but doesn't contain required fields

add_task(async function() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  await promiseStartupManager();

  const ID = "addon@tests.mozilla.org";
  await promiseInstallWebExtension({
    manifest: {
      version: "2.0",
      applications: { gecko: { id: ID } },
    },
  });

  await promiseShutdownManager();

  // First startup/shutdown finished
  // Replace the JSON store with something bogus
  await saveJSON({ not: "what we expect to find" }, gExtensionsJSON.path);

  await promiseStartupManager();
  // Retrieve an addon to force the database to rebuild
  let addon = await AddonManager.getAddonByID(ID);

  Assert.equal(addon.id, ID);

  await promiseShutdownManager();

  // Make sure our JSON database has schemaVersion and our installed extension
  let data = await loadJSON(gExtensionsJSON.path);
  Assert.ok("schemaVersion" in data);
  Assert.equal(data.addons[0].id, ID);
});
