/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Check that CA is active if and only if:
// 1. browser.contentanalysis.enabled is true and
// 2. Either browser.contentanalysis.enabled was set by an enteprise
//    policy or the "-allow-content-analysis" command line arg was present
// We can't really test command line arguments so we instead use a test-only
// method to set the value the command-line is supposed to update.

"use strict";

const { EnterprisePolicyTesting, PoliciesPrefTracker } =
  ChromeUtils.importESModule(
    "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
  );

const kEnabledPref = "enabled";
const kPipeNamePref = "pipe_path_name";
const kTimeoutPref = "agent_timeout";
const kAllowUrlPref = "allow_url_regex_list";
const kDenyUrlPref = "deny_url_regex_list";
const kAgentNamePref = "agent_name";
const kClientSignaturePref = "client_signature";
const kPerUserPref = "is_per_user";
const kShowBlockedPref = "show_blocked_result";
const kDefaultAllowPref = "default_allow";

const ca = Cc["@mozilla.org/contentanalysis;1"].getService(
  Ci.nsIContentAnalysis
);

add_task(async function test_ca_active() {
  PoliciesPrefTracker.start();
  ok(!ca.isActive, "CA is inactive when pref and cmd line arg are missing");

  // Set the pref without enterprise policy.  CA should not be active.
  Services.prefs.setBoolPref("browser.contentanalysis." + kEnabledPref, true);
  ok(
    !ca.isActive,
    "CA is inactive when pref is set but cmd line arg is missing"
  );

  // Set the pref without enterprise policy but also set command line arg
  // property.  CA should be active.
  ca.testOnlySetCACmdLineArg(true);
  ok(ca.isActive, "CA is active when pref is set and cmd line arg is present");

  // Undo test-only value before later tests.
  ca.testOnlySetCACmdLineArg(false);
  ok(!ca.isActive, "properly unset cmd line arg value");

  // Disabled the pref with enterprise policy.  CA should not be active.
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      ContentAnalysis: { Enabled: false },
    },
  });
  ok(!ca.isActive, "CA is inactive when disabled by enterprise policy pref");

  // Enabled the pref with enterprise policy.  CA should be active.
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      ContentAnalysis: { Enabled: true },
    },
  });
  ok(ca.isActive, "CA is active when enabled by enterprise policy pref");
  PoliciesPrefTracker.stop();
});

add_task(async function test_ca_enterprise_config() {
  PoliciesPrefTracker.start();
  const string1 = "this is a string";
  const string2 = "this is another string";
  const string3 = "an agent name";
  const string4 = "a client signature";

  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      ContentAnalysis: {
        PipePathName: "abc",
        AgentTimeout: 99,
        AllowUrlRegexList: string1,
        DenyUrlRegexList: string2,
        AgentName: string3,
        ClientSignature: string4,
        IsPerUser: true,
        ShowBlockedResult: false,
        DefaultAllow: true,
      },
    },
  });

  is(
    Services.prefs.getStringPref("browser.contentanalysis." + kPipeNamePref),
    "abc",
    "pipe name match"
  );
  is(
    Services.prefs.getIntPref("browser.contentanalysis." + kTimeoutPref),
    99,
    "timeout match"
  );
  is(
    Services.prefs.getStringPref("browser.contentanalysis." + kAllowUrlPref),
    string1,
    "allow urls match"
  );
  is(
    Services.prefs.getStringPref("browser.contentanalysis." + kDenyUrlPref),
    string2,
    "deny urls match"
  );
  is(
    Services.prefs.getStringPref("browser.contentanalysis." + kAgentNamePref),
    string3,
    "agent names match"
  );
  is(
    Services.prefs.getStringPref(
      "browser.contentanalysis." + kClientSignaturePref
    ),
    string4,
    "client signatures match"
  );
  is(
    Services.prefs.getBoolPref("browser.contentanalysis." + kPerUserPref),
    true,
    "per user match"
  );
  is(
    Services.prefs.getBoolPref("browser.contentanalysis." + kShowBlockedPref),
    false,
    "show blocked match"
  );
  is(
    Services.prefs.getBoolPref("browser.contentanalysis." + kDefaultAllowPref),
    true,
    "default allow match"
  );
  PoliciesPrefTracker.stop();
});

add_task(async function test_cleanup() {
  ca.testOnlySetCACmdLineArg(false);
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {},
  });
  // These may have gotten set when ContentAnalysis was enabled through
  // the policy and do not get cleared if there is no ContentAnalysis
  // element - reset them manually here.
  ca.isSetByEnterprisePolicy = false;
  Services.prefs.setBoolPref("browser.contentanalysis." + kEnabledPref, false);
});
