/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_handle_multi_action() {
  const action = {
    type: "MULTI_ACTION",
    data: {
      actions: [
        {
          type: "DISABLE_DOH",
        },
        {
          type: "OPEN_AWESOME_BAR",
        },
      ],
    },
  };
  const DOH_DOORHANGER_DECISION_PREF = "doh-rollout.doorhanger-decision";
  const NETWORK_TRR_MODE_PREF = "network.trr.mode";

  await SpecialPowers.pushPrefEnv({
    set: [
      [DOH_DOORHANGER_DECISION_PREF, "mochitest"],
      [NETWORK_TRR_MODE_PREF, 0],
    ],
  });

  await SMATestUtils.executeAndValidateAction(action);

  Assert.equal(
    Services.prefs.getStringPref(DOH_DOORHANGER_DECISION_PREF, ""),
    "UIDisabled",
    "Pref should be set on disabled"
  );
  Assert.equal(
    Services.prefs.getIntPref(NETWORK_TRR_MODE_PREF, 0),
    5,
    "Pref should be set on disabled"
  );

  Assert.ok(gURLBar.focused, "Focus should be on awesome bar");
});

add_task(async function test_handle_multi_action_with_invalid_action() {
  const action = {
    type: "MULTI_ACTION",
    data: {
      actions: [
        {
          type: "NONSENSE",
        },
      ],
    },
  };

  await SMATestUtils.validateAction(action);

  let error;
  try {
    await SpecialMessageActions.handleAction(action, gBrowser);
  } catch (e) {
    error = e;
  }

  ok(error, "should throw if an unexpected event is handled");
  Assert.equal(
    error.message,
    "Error in MULTI_ACTION event: Special message action with type NONSENSE is unsupported."
  );
});
