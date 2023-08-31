/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

// Need a profile dir to initialize Glean.
add_setup(async () => {
  do_get_profile();
  Services.fog.initializeFOG();
});

add_task(async function test_remote_extensions_pref_telemetry() {
  let original = Services.prefs.getBoolPref("extensions.webextensions.remote");
  await AddonTestUtils.promiseStartupManager();

  equal(
    original,
    Glean.extensions.useRemotePref.testGetValue(),
    "useRemotePref flag in glean is correct."
  );
  equal(
    original,
    Glean.extensions.useRemotePolicy.testGetValue(),
    "useRemotePolicy flag in glean is correct."
  );

  // Change the pref to simulate nimbus doing so after startup.
  Services.prefs.setBoolPref("extensions.webextensions.remote", !original);

  equal(
    !original,
    Glean.extensions.useRemotePref.testGetValue(),
    "useRemotePref flag reflects the changed pref."
  );
  // EPS::UseRemoteExtensions() only reads the pref once, for consistency.
  equal(
    original,
    Glean.extensions.useRemotePolicy.testGetValue(),
    "useRemotePolicy flag still equal to original pref value."
  );
});
