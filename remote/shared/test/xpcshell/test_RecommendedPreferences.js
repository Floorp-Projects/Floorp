/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { RecommendedPreferences } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/RecommendedPreferences.sys.mjs"
);

const COMMON_PREF = "toolkit.startup.max_resumed_crashes";

const PROTOCOL_1_PREF = "dom.disable_beforeunload";
const PROTOCOL_1_RECOMMENDED_PREFS = new Map([[PROTOCOL_1_PREF, true]]);

const PROTOCOL_2_PREF = "browser.contentblocking.features.standard";
const PROTOCOL_2_RECOMMENDED_PREFS = new Map([
  [PROTOCOL_2_PREF, "-tp,tpPrivate,cookieBehavior0,-cm,-fp"],
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

add_task(async function test_multipleClients() {
  info("Check initial values for the test preferences");
  checkPreferences({ common: false, protocol_1: false, protocol_2: false });

  checkPreferences({ common: false, protocol_1: false, protocol_2: false });

  info("Apply recommended preferences for a protocol_1 client");
  RecommendedPreferences.applyPreferences(PROTOCOL_1_RECOMMENDED_PREFS);
  checkPreferences({ common: true, protocol_1: true, protocol_2: false });

  info("Apply recommended preferences for a protocol_2 client");
  RecommendedPreferences.applyPreferences(PROTOCOL_2_RECOMMENDED_PREFS);
  checkPreferences({ common: true, protocol_1: true, protocol_2: true });

  info("Restore protocol_1 preferences");
  RecommendedPreferences.restorePreferences(PROTOCOL_1_RECOMMENDED_PREFS);
  checkPreferences({ common: true, protocol_1: false, protocol_2: true });

  info("Restore protocol_2 preferences");
  RecommendedPreferences.restorePreferences(PROTOCOL_2_RECOMMENDED_PREFS);
  checkPreferences({ common: true, protocol_1: false, protocol_2: false });

  info("Restore all the altered preferences");
  RecommendedPreferences.restoreAllPreferences();
  checkPreferences({ common: false, protocol_1: false, protocol_2: false });

  info("Attemps to restore again");
  RecommendedPreferences.restoreAllPreferences();
  checkPreferences({ common: false, protocol_1: false, protocol_2: false });

  cleanup();
});

add_task(async function test_disabled() {
  info("Disable RecommendedPreferences");
  Services.prefs.setBoolPref("remote.prefs.recommended", false);

  info("Check initial values for the test preferences");
  checkPreferences({ common: false, protocol_1: false, protocol_2: false });

  info("Recommended preferences are not applied, applyPreferences is a no-op");
  RecommendedPreferences.applyPreferences(PROTOCOL_1_RECOMMENDED_PREFS);
  checkPreferences({ common: false, protocol_1: false, protocol_2: false });

  cleanup();
});

add_task(async function test_noCustomPreferences() {
  info("Applying preferences without any custom preference should not throw");

  // First call invokes setting of default preferences
  RecommendedPreferences.applyPreferences();

  // Second call does nothing
  RecommendedPreferences.applyPreferences();

  cleanup();
});

// Check that protocols can override common preferences.
add_task(async function test_override() {
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

function checkPreferences({ common, protocol_1, protocol_2 }) {
  checkPreference(COMMON_PREF, { hasValue: common });
  checkPreference(PROTOCOL_1_PREF, { hasValue: protocol_1 });
  checkPreference(PROTOCOL_2_PREF, { hasValue: protocol_2 });
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
