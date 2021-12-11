const PREF_SIGNATURES_GENERAL = "xpinstall.signatures.required";
const PREF_SIGNATURES_LANGPACKS = "extensions.langpacks.signatures.required";

// Try to install the given XPI file, and assert that the install
// succeeds.  Uninstalls before returning.
async function installShouldSucceed(file) {
  let install = await promiseInstallFile(file);
  Assert.equal(install.state, AddonManager.STATE_INSTALLED);
  Assert.notEqual(install.addon, null);
  await install.addon.uninstall();
}

// Try to install the given XPI file, assert that the install fails
// due to lack of signing.
async function installShouldFail(file) {
  let install;
  try {
    install = await AddonManager.getInstallForFile(file);
  } catch (err) {}
  Assert.equal(install.state, AddonManager.STATE_DOWNLOAD_FAILED);
  Assert.equal(install.error, AddonManager.ERROR_SIGNEDSTATE_REQUIRED);
  Assert.equal(install.addon, null);
}

// Test that the preference controlling langpack signing works properly
// (and that the general preference for addon signing does not affect
// language packs).
add_task(async function() {
  AddonTestUtils.useRealCertChecks = true;

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");
  await promiseStartupManager();

  Services.prefs.setBoolPref(PREF_SIGNATURES_GENERAL, true);
  Services.prefs.setBoolPref(PREF_SIGNATURES_LANGPACKS, true);

  // The signed langpack should always install.
  let signedXPI = do_get_file("data/signing_checks/langpack_signed.xpi");
  await installShouldSucceed(signedXPI);

  // With signatures required, unsigned langpack should not install.
  let unsignedXPI = do_get_file("data/signing_checks/langpack_unsigned.xpi");
  await installShouldFail(unsignedXPI);

  // Even with the general xpi signing pref off, an unsigned langapck
  // should not install.
  Services.prefs.setBoolPref(PREF_SIGNATURES_GENERAL, false);
  await installShouldFail(unsignedXPI);

  // But with the langpack signing pref off, unsigned langpack should
  // install only on non-release builds.
  Services.prefs.setBoolPref(PREF_SIGNATURES_LANGPACKS, false);
  if (AppConstants.MOZ_REQUIRE_SIGNING) {
    await installShouldFail(unsignedXPI);
  } else {
    await installShouldSucceed(unsignedXPI);
  }

  await promiseShutdownManager();
});
