/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_broken_json() {
  await setupPolicyEngineWithJson("config_broken_json.json");

  is(
    Services.policies.status,
    Ci.nsIEnterprisePolicies.FAILED,
    "Engine was correctly set to the error state"
  );
});
