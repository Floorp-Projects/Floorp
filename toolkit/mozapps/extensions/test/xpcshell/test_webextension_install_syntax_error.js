const ADDON_ID = "webext-test@tests.mozilla.org";

add_task(async function install_xpi() {

  // Data for WebExtension with syntax error
  let xpi1 = Extension.generateXPI({
    files: {
      "manifest.json": String.raw`{
        // This is a manifest. Intentional syntax error in next line.
        "manifest_version: 2,
        "applications": {"gecko": {"id": "${ADDON_ID}"}},
        "name": "Temp WebExt with Error",
        "version": "0.1"
      }`,
    },
  });

  // Data for WebExtension without syntax error
  let xpi2 = Extension.generateXPI({
    files: {
      "manifest.json": String.raw`{
        // This is a manifest.
        "manifest_version": 2,
        "applications": {"gecko": {"id": "${ADDON_ID}"}},
        "name": "Temp WebExt without Error",
        "version": "0.1"
      }`,
    },
  });

  let install1 = await AddonManager.getInstallForFile(xpi1);
  Assert.equal(install1.state, AddonManager.STATE_DOWNLOAD_FAILED);
  Assert.equal(install1.error, AddonManager.ERROR_CORRUPT_FILE);

  // Replace xpi1 with xpi2 to have the same filename to reproduce install error
  xpi2.moveTo(xpi1.parent, xpi1.leafName);

  let install2 = await AddonManager.getInstallForFile(xpi2);
  Assert.notEqual(install2.error, AddonManager.ERROR_CORRUPT_FILE);

});

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
  startupManager();

  run_next_test();
}
