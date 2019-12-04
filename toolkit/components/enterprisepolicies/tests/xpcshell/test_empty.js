/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_app_update_URL() {
  await setupPolicyEngineWithJson({
    policies: {},
  });

  equal(
    Services.policies.status,
    Ci.nsIEnterprisePolicies.INACTIVE,
    "Engine is not active"
  );
});
