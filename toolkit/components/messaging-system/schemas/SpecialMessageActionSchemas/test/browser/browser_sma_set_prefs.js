/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const HOMEPAGE_PREF = "browser.startup.homepage";
const PRIVACY_SEGMENTATION_PREF = "browser.dataFeatureRecommendations.enabled";

const PREFS = [HOMEPAGE_PREF, PRIVACY_SEGMENTATION_PREF];

add_setup(async function() {
  registerCleanupFunction(async () => {
    PREFS.forEach(pref => Services.prefs.clearUserPref(pref));
  });
});

add_task(async function test_set_privacy_segmentation_pref() {
  const action = {
    type: "SET_PREF",
    data: {
      pref: {
        name: PRIVACY_SEGMENTATION_PREF,
        value: true,
      },
    },
  };

  Assert.ok(
    !Services.prefs.prefHasUserValue(PRIVACY_SEGMENTATION_PREF),
    "Test setup ok"
  );

  await SMATestUtils.executeAndValidateAction(action);

  Assert.ok(
    Services.prefs.getBoolPref(PRIVACY_SEGMENTATION_PREF),
    `${PRIVACY_SEGMENTATION_PREF} pref successfully updated`
  );
});

add_task(async function test_clear_privacy_segmentation_pref() {
  Services.prefs.setBoolPref(PRIVACY_SEGMENTATION_PREF, true);
  Assert.ok(
    Services.prefs.prefHasUserValue(PRIVACY_SEGMENTATION_PREF),
    "Test setup ok"
  );

  const action = {
    type: "SET_PREF",
    data: {
      pref: {
        name: PRIVACY_SEGMENTATION_PREF,
      },
    },
  };

  await SMATestUtils.executeAndValidateAction(action);

  Assert.ok(
    !Services.prefs.prefHasUserValue(PRIVACY_SEGMENTATION_PREF),
    `${PRIVACY_SEGMENTATION_PREF} pref successfully cleared`
  );
});

add_task(async function test_set_homepage_pref() {
  const action = {
    type: "SET_PREF",
    data: {
      pref: {
        name: HOMEPAGE_PREF,
        value: "https://foo.example.com",
      },
    },
  };

  Assert.ok(!Services.prefs.prefHasUserValue(HOMEPAGE_PREF), "Test setup ok");

  await SMATestUtils.executeAndValidateAction(action);

  Assert.equal(
    Services.prefs.getStringPref(HOMEPAGE_PREF),
    "https://foo.example.com",
    `${HOMEPAGE_PREF} pref successfully updated`
  );
});

add_task(async function test_clear_homepage_pref() {
  Services.prefs.setStringPref(HOMEPAGE_PREF, "https://www.example.com");
  Assert.ok(Services.prefs.prefHasUserValue(HOMEPAGE_PREF), "Test setup ok");

  const action = {
    type: "SET_PREF",
    data: {
      pref: {
        name: HOMEPAGE_PREF,
      },
    },
  };

  await SMATestUtils.executeAndValidateAction(action);

  Assert.ok(
    !Services.prefs.prefHasUserValue(HOMEPAGE_PREF),
    `${HOMEPAGE_PREF} pref successfully updated`
  );
});
