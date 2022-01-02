/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
const DOH_DOORHANGER_DECISION_PREF = "doh-rollout.doorhanger-decision";

add_task(async function test_disable_doh() {
  await SpecialPowers.pushPrefEnv({
    set: [[DOH_DOORHANGER_DECISION_PREF, ""]],
  });
  await SMATestUtils.executeAndValidateAction({ type: "ACCEPT_DOH" });
  Assert.equal(
    Services.prefs.getStringPref(DOH_DOORHANGER_DECISION_PREF, ""),
    "UIOk",
    "Pref should be set on accept"
  );
});
