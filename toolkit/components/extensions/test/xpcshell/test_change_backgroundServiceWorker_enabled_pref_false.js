"use strict";

// extensions.backgroundServiceWorker.enabled=false is set in the test manifest
// because there is no guarantee that the pref value set at runtime takes effect
// due to the pref being declared "mirror: once". The value of this pref is
// frozen upon the first access to any "mirror:once" pref, and we can therefore
// not assume the pref value to be mutable at runtime.
const PREF_EXT_SW_ENABLED = "extensions.backgroundServiceWorker.enabled";

add_task(async function test_backgroundServiceWorkerEnabled() {
  // Sanity check:
  Assert.equal(
    Services.prefs.getBoolPref(PREF_EXT_SW_ENABLED),
    false,
    "Pref value should be false"
  );
  Assert.equal(
    WebExtensionPolicy.backgroundServiceWorkerEnabled,
    false,
    "backgroundServiceWorkerEnabled should be false"
  );

  if (AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED) {
    Assert.ok(
      !Services.prefs.prefIsLocked(PREF_EXT_SW_ENABLED),
      "Pref should be not locked when MOZ_WEBEXT_WEBIDL_ENABLED is true"
    );
  } else {
    Assert.ok(
      Services.prefs.prefIsLocked(PREF_EXT_SW_ENABLED),
      "Pref should be locked when MOZ_WEBEXT_WEBIDL_ENABLED is false"
    );
    Services.prefs.unlockPref(PREF_EXT_SW_ENABLED);
  }

  // Flip pref and test result.
  Services.prefs.setBoolPref(PREF_EXT_SW_ENABLED, true);
  Assert.ok(
    Services.prefs.getBoolPref(PREF_EXT_SW_ENABLED),
    "pref can change after setting it"
  );
  Assert.equal(
    WebExtensionPolicy.backgroundServiceWorkerEnabled,
    false,
    "backgroundServiceWorkerEnabled is still false despite the pref flip"
  );
});
