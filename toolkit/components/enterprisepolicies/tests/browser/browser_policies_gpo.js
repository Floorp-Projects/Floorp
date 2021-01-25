/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function setup_preferences() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.policies.alternateGPO", "SOFTWARE\\Mozilla\\PolicyTesting"],
    ],
  });
});

add_task(async function test_gpo_policies() {
  let { Policies } = ChromeUtils.import(
    "resource:///modules/policies/Policies.jsm"
  );

  let gpoPolicyRan = false;

  Policies.gpo_policy = {
    onProfileAfterChange(manager, param) {
      is(param, true, "Param matches what was in the registry");
      gpoPolicyRan = true;
    },
  };

  let wrk = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
    Ci.nsIWindowsRegKey
  );
  let regLocation =
    "SOFTWARE\\Mozilla\\PolicyTesting\\Mozilla\\" + Services.appinfo.name;
  wrk.create(wrk.ROOT_KEY_CURRENT_USER, regLocation, wrk.ACCESS_WRITE);
  wrk.writeIntValue("gpo_policy", 1);
  wrk.close();

  await setupPolicyEngineWithJson(
    // empty policies.json since we are using GPO
    {
      policies: {},
    },

    // custom schema
    {
      properties: {
        gpo_policy: {
          type: "boolean",
        },
      },
    }
  );

  is(
    Services.policies.status,
    Ci.nsIEnterprisePolicies.ACTIVE,
    "Engine is active"
  );

  ok(gpoPolicyRan, "GPO Policy ran correctly though onProfileAfterChange");

  delete Policies.gpo_policy;

  wrk.open(wrk.ROOT_KEY_CURRENT_USER, "SOFTWARE\\Mozilla", wrk.ACCESS_WRITE);
  wrk.removeChild("PolicyTesting\\Mozilla\\" + Services.appinfo.name);
  wrk.removeChild("PolicyTesting\\Mozilla");
  wrk.removeChild("PolicyTesting");
  wrk.close();
});
