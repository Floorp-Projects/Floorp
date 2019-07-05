/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that add-on update check failures are propagated correctly

// The test extension uses an insecure update url.
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

var testserver;

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  // Create and configure the HTTP server.
  testserver = createHttpServer({ hosts: ["example.com"] });
  testserver.registerDirectory("/data/", do_get_file("data"));

  await promiseStartupManager();
});

// Verify that an update check returns the correct errors.
add_task(async function() {
  await promiseInstallWebExtension({
    manifest: {
      name: "Test Addon 1",
      version: "1.0",
      applications: {
        gecko: {
          id: "addon1@tests.mozilla.org",
          update_url: "http://example.com/data/test_missing.json",
        },
      },
    },
  });

  let addon = await promiseAddonByID("addon1@tests.mozilla.org");
  equal(addon.version, "1.0");

  // We're expecting an error, so resolve when the promise is rejected.
  let update = await promiseFindAddonUpdates(
    addon,
    AddonManager.UPDATE_WHEN_USER_REQUESTED
  ).catch(e => e);

  ok(!update.compatibilityUpdate, "not expecting a compatibility update");
  ok(!update.updateAvailable, "not expecting a compatibility update");

  equal(update.error, AddonManager.UPDATE_STATUS_DOWNLOAD_ERROR);

  await addon.uninstall();
});
