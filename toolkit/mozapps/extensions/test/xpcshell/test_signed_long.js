gUseRealCertChecks = true;

const ID = "123456789012345678901234567890123456789012345678901@somewhere.com";

// Tests that signature verification works correctly on an extension with
// an ID that does not fit into a certificate CN field.
add_task(async function test_long_id() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
  await promiseStartupManager();

  Assert.greater(ID.length, 64, "ID is > 64 characters");

  await promiseInstallFile(do_get_file("data/signing_checks/long.xpi"));
  let addon = await promiseAddonByID(ID);

  Assert.notEqual(addon, null, "Addon install properly");
  Assert.ok(
    addon.signedState > AddonManager.SIGNEDSTATE_MISSING,
    "Signature verification worked properly"
  );

  await addon.uninstall();
});
