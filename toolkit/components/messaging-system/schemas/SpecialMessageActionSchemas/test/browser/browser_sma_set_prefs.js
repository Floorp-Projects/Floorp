/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const HOMEPAGE_PREF = "browser.startup.homepage";
const PRIVACY_SEGMENTATION_PREF = "browser.dataFeatureRecommendations.enabled";
const MESSAGING_ACTION_PREF = "special-message-testpref";

const PREFS_TO_CLEAR = [
  HOMEPAGE_PREF,
  PRIVACY_SEGMENTATION_PREF,
  `messaging-system-action.${MESSAGING_ACTION_PREF}`,
];

add_setup(async function () {
  registerCleanupFunction(async () => {
    PREFS_TO_CLEAR.forEach(pref => Services.prefs.clearUserPref(pref));
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

// Set a pref not listed in "allowed prefs"
add_task(async function test_set_messaging_system_pref() {
  const action = {
    type: "SET_PREF",
    data: {
      pref: {
        name: MESSAGING_ACTION_PREF,
        value: true,
      },
    },
  };

  await SMATestUtils.executeAndValidateAction(action);

  Assert.equal(
    Services.prefs.getBoolPref(
      `messaging-system-action.${MESSAGING_ACTION_PREF}`
    ),
    true,
    `messaging-system-action.${MESSAGING_ACTION_PREF} pref successfully updated to correct value`
  );
});

// Clear a pref not listed in "allowed prefs" that was initially set by
// the SET_PREF special messaging action
add_task(async function test_clear_messaging_system_pref() {
  Services.prefs.setBoolPref(
    `messaging-system-action.${MESSAGING_ACTION_PREF}`,
    true
  );
  Assert.ok(
    Services.prefs.prefHasUserValue(
      `messaging-system-action.${MESSAGING_ACTION_PREF}`
    ),
    "Test setup ok"
  );

  const action = {
    type: "SET_PREF",
    data: {
      pref: {
        name: MESSAGING_ACTION_PREF,
      },
    },
  };

  await SMATestUtils.executeAndValidateAction(action);

  Assert.ok(
    !Services.prefs.prefHasUserValue(
      `messaging-system-action.${MESSAGING_ACTION_PREF}`
    ),
    `messaging-system-action.${MESSAGING_ACTION_PREF} pref successfully cleared`
  );
});
