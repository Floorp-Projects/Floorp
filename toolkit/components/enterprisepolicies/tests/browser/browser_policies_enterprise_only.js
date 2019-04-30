/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PREF_DISALLOW_ENTERPRISE = "browser.policies.testing.disallowEnterprise";

add_task(async function test_enterprise_only_policies() {
  let { Policies } = ChromeUtils.import("resource:///modules/policies/Policies.jsm");

  let normalPolicyRan = false, enterprisePolicyRan = false;

  Policies.NormalPolicy = {
    onProfileAfterChange(manager, param) {
      normalPolicyRan = true;
    },
  };

  Policies.EnterpriseOnlyPolicy = {
    onProfileAfterChange(manager, param) {
      enterprisePolicyRan = true;
    },
  };

  Services.prefs.setBoolPref(PREF_DISALLOW_ENTERPRISE, true);

  await setupPolicyEngineWithJson(
    // policies.json
    {
      "policies": {
        "NormalPolicy": true,
        "EnterpriseOnlyPolicy": true,
      },
    },

    // custom schema
    {
      properties: {
        "NormalPolicy": {
          "type": "boolean",
        },

        "EnterpriseOnlyPolicy": {
          "type": "boolean",
          "enterprise_only": true,
        },
      },
    }
  );

  is(Services.policies.status, Ci.nsIEnterprisePolicies.ACTIVE, "Engine is active");
  is(normalPolicyRan, true, "Normal policy ran as expected");
  is(enterprisePolicyRan, false, "Enterprise-only policy was prevented from running");

  // Clean-up
  delete Policies.NormalPolicy;
  delete Policies.EnterpriseOnlyPolicy;
  Services.prefs.clearUserPref(PREF_DISALLOW_ENTERPRISE);
});
