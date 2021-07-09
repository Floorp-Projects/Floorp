/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const COMMON_PREF = "toolkit.startup.max_resumed_crashes";

const MARIONETTE_PREF = "dom.disable_beforeunload";
const MARIONETTE_RECOMMENDED_PREFS = new Map([[MARIONETTE_PREF, true]]);

const CDP_PREF = "browser.contentblocking.features.standard";
const CDP_RECOMMENDED_PREFS = new Map([
  [CDP_PREF, "-tp,tpPrivate,cookieBehavior0,-cm,-fp"],
]);

add_task(async function test_RecommendedPreferences() {
  info("Check initial values for the test preferences");
  checkPreferences({ cdp: false, common: false, marionette: false });

  info("Common preferences will be applied on load");
  const { RecommendedPreferences } = ChromeUtils.import(
    "chrome://remote/content/shared/RecommendedPreferences.jsm"
  );
  checkPreferences({ cdp: false, common: true, marionette: false });

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
});

add_task(async function test_RecommendedPreferences_disabled() {
  info("Disable RecommendedPreferences");
  Services.prefs.setBoolPref("remote.prefs.recommended", false);

  info("Check initial values for the test preferences");
  checkPreferences({ cdp: false, common: false, marionette: false });

  info("Recommended preferences will not be applied on load or per protocol");
  const { RecommendedPreferences } = ChromeUtils.import(
    "chrome://remote/content/shared/RecommendedPreferences.jsm"
  );
  RecommendedPreferences.applyPreferences(MARIONETTE_RECOMMENDED_PREFS);
  checkPreferences({ cdp: false, common: false, marionette: false });

  // Restore remote.prefs.recommended
  Services.prefs.clearUserPref("remote.prefs.recommended");
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
