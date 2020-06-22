/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
const DOH_DOORHANGER_DECISION_PREF = "doh-rollout.doorhanger-decision";
const NETWORK_TRR_MODE_PREF = "network.trr.mode";

add_task(async function test_disable_doh() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [DOH_DOORHANGER_DECISION_PREF, "mochitest"],
      [NETWORK_TRR_MODE_PREF, 0],
    ],
  });

  SpecialMessageActions.blockMessageById = messageId =>
    Assert.equal(
      messageId,
      "DOH_ROLLOUT_CONFIRMATION",
      "Block the correct message"
    );

  await SpecialMessageActions.handleAction({ type: "DISABLE_DOH" }, gBrowser);

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
});
