/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that various error conditions are handled correctly

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  await promiseStartupManager();
});

// Checks that a local file validates ok
add_task(async function run_test_1() {
  let xpi = await createTempWebExtensionFile({});
  let install = await AddonManager.getInstallForFile(xpi);
  Assert.notEqual(install, null);
  Assert.equal(install.state, AddonManager.STATE_DOWNLOADED);
  Assert.equal(install.error, 0);

  install.cancel();
});

// Checks that a corrupt file shows an error
add_task(async function run_test_2() {
  let xpi = AddonTestUtils.allocTempXPIFile();
  await OS.File.writeAtomic(
    xpi.path,
    new TextEncoder().encode("this is not a zip file")
  );

  let install = await AddonManager.getInstallForFile(xpi);
  Assert.notEqual(install, null);
  Assert.equal(install.state, AddonManager.STATE_DOWNLOAD_FAILED);
  Assert.equal(install.error, AddonManager.ERROR_CORRUPT_FILE);
});

// Checks that an empty file shows an error
add_task(async function run_test_3() {
  let xpi = await AddonTestUtils.createTempXPIFile({});
  let install = await AddonManager.getInstallForFile(xpi);
  Assert.notEqual(install, null);
  Assert.equal(install.state, AddonManager.STATE_DOWNLOAD_FAILED);
  Assert.equal(install.error, AddonManager.ERROR_CORRUPT_FILE);
});

// Checks that a file that doesn't match its hash shows an error
add_task(async function run_test_4() {
  let xpi = await createTempWebExtensionFile({});
  let url = Services.io.newFileURI(xpi).spec;
  let install = await AddonManager.getInstallForURL(url, { hash: "sha1:foo" });
  Assert.notEqual(install, null);
  Assert.equal(install.state, AddonManager.STATE_DOWNLOAD_FAILED);
  Assert.equal(install.error, AddonManager.ERROR_INCORRECT_HASH);
});

// Checks that a file that doesn't exist shows an error
add_task(async function run_test_5() {
  let file = do_get_file("data");
  file.append("missing.xpi");
  let install = await AddonManager.getInstallForFile(file);
  Assert.notEqual(install, null);
  Assert.equal(install.state, AddonManager.STATE_DOWNLOAD_FAILED);
  Assert.equal(install.error, AddonManager.ERROR_NETWORK_FAILURE);
});

// Checks that an add-on with an illegal ID shows an error
add_task(async function run_test_6() {
  let xpi = await createTempWebExtensionFile({
    manifest: {
      applications: { gecko: { id: "invalid" } },
    },
  });
  let install = await AddonManager.getInstallForFile(xpi);
  Assert.notEqual(install, null);
  Assert.equal(install.state, AddonManager.STATE_DOWNLOAD_FAILED);
  Assert.equal(install.error, AddonManager.ERROR_CORRUPT_FILE);
});
