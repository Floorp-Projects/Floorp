/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_tcp_action() {
  let error = null;
  try {
    await SMATestUtils.executeAndValidateAction({
      type: "ENABLE_TOTAL_COOKIE_PROTECTION",
    });
  } catch (e) {
    error = e;
  }
  ok(!error, "The action should be valid");
});

add_task(async function test_tcp_section_action() {
  let PREF =
    "privacy.restrict3rdpartystorage.rollout.preferences.TCPToggleInStandard";
  let error = null;
  try {
    await SMATestUtils.executeAndValidateAction({
      type: "ENABLE_TOTAL_COOKIE_PROTECTION_SECTION",
    });
  } catch (e) {
    error = e;
  }
  ok(!error, "The action should be valid");
  Services.prefs.clearUserPref(PREF);
});

add_task(async function test_tcp_action_pref() {
  let PREF = "privacy.restrict3rdpartystorage.rollout.enabledByDefault";
  Services.prefs.setBoolPref(PREF, false);
  await SMATestUtils.executeAndValidateAction({
    type: "ENABLE_TOTAL_COOKIE_PROTECTION",
  });
  Assert.ok(Services.prefs.getBoolPref(PREF), "Pref is enabled by the action");
  Services.prefs.clearUserPref(PREF);
});

add_task(async function test_tcp_section_action_pref() {
  let PREF =
    "privacy.restrict3rdpartystorage.rollout.preferences.TCPToggleInStandard";
  Assert.ok(!Services.prefs.prefHasUserValue(PREF));
  await SMATestUtils.executeAndValidateAction({
    type: "ENABLE_TOTAL_COOKIE_PROTECTION_SECTION",
  });
  Assert.ok(Services.prefs.prefHasUserValue(PREF));
  Assert.ok(Services.prefs.getBoolPref(PREF), "Pref is enabled by the action");
  Services.prefs.clearUserPref(PREF);
});
