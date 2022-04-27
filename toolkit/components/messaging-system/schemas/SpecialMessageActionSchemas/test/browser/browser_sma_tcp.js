/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_tcp_action() {
  let PREF = "privacy.restrict3rdpartystorage.rollout.enabledByDefault";

  let error = null;
  try {
    await SMATestUtils.executeAndValidateAction({
      type: "ENABLE_TOTAL_COOKIE_PROTECTION",
    });
  } catch (e) {
    error = e;
  }
  ok(!error, "The action should be valid");

  Services.prefs.clearUserPref(PREF);
});

add_task(async function test_tcp_section_action() {
  let PREF =
    "privacy.restrict3rdpartystorage.rollout.preferences.TCPToggleInStandard";
  let PREF_TCP_ENABLED_BY_DEFAULT =
    "privacy.restrict3rdpartystorage.rollout.enabledByDefault";

  let error = null;
  try {
    await SMATestUtils.executeAndValidateAction({
      type: "ENABLE_TOTAL_COOKIE_PROTECTION_SECTION_AND_OPT_OUT",
    });
  } catch (e) {
    error = e;
  }
  ok(!error, "The action should be valid");

  Services.prefs.clearUserPref(PREF);
  Services.prefs.clearUserPref(PREF_TCP_ENABLED_BY_DEFAULT);
});

add_task(async function test_tcp_action_pref() {
  let PREF = "privacy.restrict3rdpartystorage.rollout.enabledByDefault";
  Services.prefs.setBoolPref(PREF, false);

  await SMATestUtils.executeAndValidateAction({
    type: "ENABLE_TOTAL_COOKIE_PROTECTION",
  });
  Assert.ok(
    Services.prefs.getBoolPref(PREF),
    `Pref '${PREF}' is enabled by the action`
  );

  Services.prefs.clearUserPref(PREF);
});

add_task(async function test_tcp_section_action_pref() {
  let PREF =
    "privacy.restrict3rdpartystorage.rollout.preferences.TCPToggleInStandard";
  let PREF_TCP_ENABLED_BY_DEFAULT =
    "privacy.restrict3rdpartystorage.rollout.enabledByDefault";

  Assert.ok(!Services.prefs.prefHasUserValue(PREF));
  Assert.ok(!Services.prefs.prefHasUserValue(PREF_TCP_ENABLED_BY_DEFAULT));

  await SMATestUtils.executeAndValidateAction({
    type: "ENABLE_TOTAL_COOKIE_PROTECTION_SECTION_AND_OPT_OUT",
  });
  Assert.ok(Services.prefs.prefHasUserValue(PREF));
  Assert.ok(
    Services.prefs.getBoolPref(PREF),
    `Pref '${PREF}' is enabled by the action`
  );

  Assert.ok(Services.prefs.prefHasUserValue(PREF_TCP_ENABLED_BY_DEFAULT));
  Assert.ok(
    !Services.prefs.getBoolPref(PREF_TCP_ENABLED_BY_DEFAULT),
    `Pref '${PREF_TCP_ENABLED_BY_DEFAULT}' is disabled by the action`
  );

  Services.prefs.clearUserPref(PREF);
  Services.prefs.clearUserPref(PREF_TCP_ENABLED_BY_DEFAULT);
});
