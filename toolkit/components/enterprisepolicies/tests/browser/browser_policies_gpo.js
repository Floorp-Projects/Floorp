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
  let { Policies } = ChromeUtils.importESModule(
    "resource:///modules/policies/Policies.sys.mjs"
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

add_task(async function test_gpo_json_policies() {
  let { Policies } = ChromeUtils.importESModule(
    "resource:///modules/policies/Policies.sys.mjs"
  );

  let gpoPolicyRan = false;
  let jsonPolicyRan = false;
  let coexistPolicyRan = false;

  Policies.gpo_policy = {
    onProfileAfterChange(manager, param) {
      is(param, true, "Param matches what was in the registry");
      gpoPolicyRan = true;
    },
  };
  Policies.json_policy = {
    onProfileAfterChange(manager, param) {
      is(param, true, "Param matches what was in the JSON");
      jsonPolicyRan = true;
    },
  };
  Policies.coexist_policy = {
    onProfileAfterChange(manager, param) {
      is(param, false, "Param matches what was in the registry (over JSON)");
      coexistPolicyRan = true;
    },
  };

  let wrk = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
    Ci.nsIWindowsRegKey
  );
  let regLocation =
    "SOFTWARE\\Mozilla\\PolicyTesting\\Mozilla\\" + Services.appinfo.name;
  wrk.create(wrk.ROOT_KEY_CURRENT_USER, regLocation, wrk.ACCESS_WRITE);
  wrk.writeIntValue("gpo_policy", 1);
  wrk.writeIntValue("coexist_policy", 0);
  wrk.close();

  await setupPolicyEngineWithJson(
    {
      policies: {
        json_policy: true,
        coexist_policy: true,
      },
    },

    // custom schema
    {
      properties: {
        gpo_policy: {
          type: "boolean",
        },
        json_policy: {
          type: "boolean",
        },
        coexist_policy: {
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
  ok(jsonPolicyRan, "JSON Policy ran correctly though onProfileAfterChange");
  ok(
    coexistPolicyRan,
    "Coexist Policy ran correctly though onProfileAfterChange"
  );

  delete Policies.gpo_policy;
  delete Policies.json_policy;
  delete Policies.coexist_policy;

  wrk.open(wrk.ROOT_KEY_CURRENT_USER, "SOFTWARE\\Mozilla", wrk.ACCESS_WRITE);
  wrk.removeChild("PolicyTesting\\Mozilla\\" + Services.appinfo.name);
  wrk.removeChild("PolicyTesting\\Mozilla");
  wrk.removeChild("PolicyTesting");
  wrk.close();
});

add_task(async function test_gpo_broken_json_policies() {
  let { Policies } = ChromeUtils.importESModule(
    "resource:///modules/policies/Policies.sys.mjs"
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
    "config_broken_json.json",
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
