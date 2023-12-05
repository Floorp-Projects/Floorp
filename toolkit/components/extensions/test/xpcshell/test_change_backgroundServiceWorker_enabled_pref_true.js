"use strict";

// extensions.backgroundServiceWorker.enabled=true is set in the test manifest
// because there is no guarantee that the pref value set at runtime takes effect
// due to the pref being declared "mirror: once". The value of this pref is
// frozen upon the first access to any "mirror:once" pref, and we can therefore
// not assume the pref value to be mutable at runtime.
const PREF_EXT_SW_ENABLED = "extensions.backgroundServiceWorker.enabled";
const defaultPrefs = Services.prefs.getDefaultBranch("");

add_task(
  { skip_if: () => AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED },
  async function test_when_extensions_webidl_bindings_disabled() {
    Assert.equal(
      defaultPrefs.getBoolPref(PREF_EXT_SW_ENABLED),
      false,
      "The default pref value should be false"
    );
    Assert.ok(
      Services.prefs.prefIsLocked(PREF_EXT_SW_ENABLED),
      "Pref should be locked when MOZ_WEBEXT_WEBIDL_ENABLED is false"
    );
    // Despite the pref set to true (see comment at PREF_EXT_SW_ENABLED), the
    // pref should be locked to false when IDL is disabled.
    Assert.equal(
      Services.prefs.getBoolPref(PREF_EXT_SW_ENABLED),
      false,
      "Pref value should be the default value"
    );
    Assert.equal(
      WebExtensionPolicy.backgroundServiceWorkerEnabled,
      false,
      "backgroundServiceWorkerEnabled should be false"
    );

    Services.prefs.unlockPref(PREF_EXT_SW_ENABLED);

    // After unlocking the pref, the pref set to true in the test manifest
    // should apply now.
    Assert.ok(
      Services.prefs.getBoolPref(PREF_EXT_SW_ENABLED),
      "After unlocking the pref, the pref can have a non-default value"
    );
    Assert.equal(
      WebExtensionPolicy.backgroundServiceWorkerEnabled,
      false,
      "backgroundServiceWorkerEnabled is still false despite the pref flip"
    );
  }
);

add_task(
  { skip_if: () => !AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED },
  async function test_when_extensions_webidl_bindings_enabled() {
    const defaultPrefValue = defaultPrefs.getBoolPref(PREF_EXT_SW_ENABLED);
    info(`The default pref value is ${defaultPrefValue}`);
    Assert.ok(
      !Services.prefs.prefIsLocked(PREF_EXT_SW_ENABLED),
      "Pref should not be locked when MOZ_WEBEXT_WEBIDL_ENABLED is true"
    );
    // Note: Pref is set to true, see comment at PREF_EXT_SW_ENABLED.
    Assert.equal(
      Services.prefs.getBoolPref(PREF_EXT_SW_ENABLED),
      true,
      "Pref value should be true"
    );
    Assert.equal(
      WebExtensionPolicy.backgroundServiceWorkerEnabled,
      true,
      "backgroundServiceWorkerEnabled should be false"
    );

    // Flip pref and test result.
    Services.prefs.setBoolPref(PREF_EXT_SW_ENABLED, false);
    Assert.ok(
      !Services.prefs.getBoolPref(PREF_EXT_SW_ENABLED),
      "The pref can be flipped to a different value"
    );
    Assert.equal(
      WebExtensionPolicy.backgroundServiceWorkerEnabled,
      true,
      "backgroundServiceWorkerEnabled is still true despite the pref flip"
    );
  }
);
