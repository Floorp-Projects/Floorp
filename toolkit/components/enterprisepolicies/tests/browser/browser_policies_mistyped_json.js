/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_json_with_mistyped_policies() {
  // Note: The "polcies" string is intentionally mistyped
  await setupPolicyEngineWithJson({
    polcies: {},
  });

  is(
    Services.policies.status,
    Ci.nsIEnterprisePolicies.FAILED,
    "Engine was correctly set to the error state"
  );
});
