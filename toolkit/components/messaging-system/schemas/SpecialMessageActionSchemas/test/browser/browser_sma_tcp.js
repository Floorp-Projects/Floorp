/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const OPT_IN_SEARCH_PREFS = [
  ["browser.search.param.google_channel_us", "tus7"],
  ["browser.search.param.google_channel_row", "trow7"],
  ["browser.search.param.bing_ptag", "MOZZ0000000031"],
];

const OPT_OUT_SEARCH_PREFS = [
  ["browser.search.param.google_channel_us", "tus7"],
  ["browser.search.param.google_channel_row", "trow7"],
  ["browser.search.param.bing_ptag", "MOZZ0000000031"],
];

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
  OPT_IN_SEARCH_PREFS.forEach(([key]) => Services.prefs.clearUserPref(key));
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
  OPT_OUT_SEARCH_PREFS.forEach(([key]) => Services.prefs.clearUserPref(key));
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

  OPT_IN_SEARCH_PREFS.forEach(([key, value]) => {
    Assert.ok(Services.prefs.prefHasUserValue(key));
    is(
      Services.prefs.getStringPref(key),
      value,
      `Pref '${key}' is set by the action`
    );
  });

  Services.prefs.clearUserPref(PREF);
  OPT_IN_SEARCH_PREFS.forEach(([key]) => Services.prefs.clearUserPref(key));
});

add_task(async function test_tcp_section_action_pref() {
  let PREF =
    "privacy.restrict3rdpartystorage.rollout.preferences.TCPToggleInStandard";
  let PREF_TCP_ENABLED_BY_DEFAULT =
    "privacy.restrict3rdpartystorage.rollout.enabledByDefault";

  Assert.ok(!Services.prefs.prefHasUserValue(PREF));
  Assert.ok(!Services.prefs.prefHasUserValue(PREF_TCP_ENABLED_BY_DEFAULT));
  OPT_OUT_SEARCH_PREFS.forEach(([key]) =>
    Assert.ok(!Services.prefs.prefHasUserValue(key))
  );

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

  OPT_OUT_SEARCH_PREFS.forEach(([key, value]) => {
    Assert.ok(Services.prefs.prefHasUserValue(key));
    is(
      Services.prefs.getStringPref(key),
      value,
      `Pref '${key}' is set by the action`
    );
  });

  Services.prefs.clearUserPref(PREF);
  Services.prefs.clearUserPref(PREF_TCP_ENABLED_BY_DEFAULT);
  OPT_OUT_SEARCH_PREFS.forEach(([key]) => Services.prefs.clearUserPref(key));
});
