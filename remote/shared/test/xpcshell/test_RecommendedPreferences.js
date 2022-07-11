/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { RecommendedPreferences } = ChromeUtils.import(
  "chrome://remote/content/shared/RecommendedPreferences.jsm"
);

const COMMON_PREF = "toolkit.startup.max_resumed_crashes";

const MARIONETTE_PREF = "dom.disable_beforeunload";
const MARIONETTE_RECOMMENDED_PREFS = new Map([[MARIONETTE_PREF, true]]);

const CDP_PREF = "browser.contentblocking.features.standard";
const CDP_RECOMMENDED_PREFS = new Map([
  [CDP_PREF, "-tp,tpPrivate,cookieBehavior0,-cm,-fp"],
]);

function cleanup() {
  info("Restore recommended preferences and test preferences");
  Services.prefs.clearUserPref("remote.prefs.recommended");
  RecommendedPreferences.restoreAllPreferences();
}

// cleanup() should be called:
// - explicitly after each test to avoid side effects
// - via registerCleanupFunction in case a test crashes/times out
registerCleanupFunction(cleanup);

add_task(async function test_RecommendedPreferences() {
  info("Check initial values for the test preferences");
  checkPreferences({ cdp: false, common: false, marionette: false });

  checkPreferences({ cdp: false, common: false, marionette: false });

  info("Apply recommended preferences for a marionette client");
  RecommendedPreferences.applyPreferences(MARIONETTE_RECOMMENDED_PREFS);
  checkPreferences({ cdp: false, common: true, marionette: true });

  info("Apply recommended preferences for a cdp client");
  RecommendedPreferences.applyPreferences(CDP_RECOMMENDED_PREFS);
  checkPreferences({ cdp: true, common: true, marionette: true });

  info("Restore marionette preferences");
  RecommendedPreferences.restorePreferences(MARIONETTE_RECOMMENDED_PREFS);
  checkPreferences({ cdp: true, common: true, marionette: false });

  info("Restore cdp preferences");
  RecommendedPreferences.restorePreferences(CDP_RECOMMENDED_PREFS);
  checkPreferences({ cdp: false, common: true, marionette: false });

  info("Restore all the altered preferences");
  RecommendedPreferences.restoreAllPreferences();
  checkPreferences({ cdp: false, common: false, marionette: false });

  info("Attemps to restore again");
  RecommendedPreferences.restoreAllPreferences();
  checkPreferences({ cdp: false, common: false, marionette: false });

  cleanup();
});

add_task(async function test_RecommendedPreferences_disabled() {
  info("Disable RecommendedPreferences");
  Services.prefs.setBoolPref("remote.prefs.recommended", false);

  info("Check initial values for the test preferences");
  checkPreferences({ cdp: false, common: false, marionette: false });

  info("Recommended preferences are not applied, applyPreferences is a no-op");
  RecommendedPreferences.applyPreferences(MARIONETTE_RECOMMENDED_PREFS);
  checkPreferences({ cdp: false, common: false, marionette: false });

  cleanup();
});

// Check that protocols can override common preferences.
add_task(async function test_RecommendedPreferences_override() {
  info("Make sure the common preference has no user value");
  Services.prefs.clearUserPref(COMMON_PREF);

  const OVERRIDE_VALUE = 42;
  const OVERRIDE_COMMON_PREF = new Map([[COMMON_PREF, OVERRIDE_VALUE]]);

  info("Apply a map of preferences overriding a common preference");
  RecommendedPreferences.applyPreferences(OVERRIDE_COMMON_PREF);

  equal(
    Services.prefs.getIntPref(COMMON_PREF),
    OVERRIDE_VALUE,
    "The common preference was set to the expected value"
  );

  cleanup();
});

function checkPreferences({ cdp, common, marionette }) {
  checkPreference(COMMON_PREF, { hasValue: common });
  checkPreference(MARIONETTE_PREF, { hasValue: marionette });
  checkPreference(CDP_PREF, { hasValue: cdp });
}

function checkPreference(pref, { hasValue }) {
  equal(
    Services.prefs.prefHasUserValue(pref),
    hasValue,
    hasValue
      ? `The preference ${pref} has a user value`
      : `The preference ${pref} has no user value`
  );
}
