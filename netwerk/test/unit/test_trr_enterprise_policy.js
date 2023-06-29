/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

trr_test_setup();
registerCleanupFunction(async () => {
  trr_clear_prefs();
});

const { updateAppInfo } = ChromeUtils.importESModule(
  "resource://testing-common/AppInfo.sys.mjs"
);

updateAppInfo({
  name: "XPCShell",
  ID: "xpcshell@tests.mozilla.org",
  version: "48",
  platformVersion: "48",
});

const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
  "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
);

// This initializes the policy engine for xpcshell tests
let policies = Cc["@mozilla.org/enterprisepolicies;1"].getService(
  Ci.nsIObserver
);
policies.observe(null, "policies-startup", null);

add_task(async function test_enterprise_policy_unlocked() {
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      DNSOverHTTPS: {
        Enabled: false,
        ProviderURL: "https://example.org/provider",
        ExcludedDomains: ["example.com", "example.org"],
      },
    },
  });

  equal(Services.prefs.getIntPref("network.trr.mode"), 5);
  equal(Services.prefs.prefIsLocked("network.trr.mode"), false);
  equal(
    Services.prefs.getStringPref("network.trr.uri"),
    "https://example.org/provider"
  );
  equal(Services.prefs.prefIsLocked("network.trr.uri"), false);
  equal(
    Services.prefs.getStringPref("network.trr.excluded-domains"),
    "example.com,example.org"
  );
  equal(Services.prefs.prefIsLocked("network.trr.excluded-domains"), false);
  equal(Services.dns.currentTrrMode, 5);
  equal(Services.dns.currentTrrURI, "https://example.org/provider");
  Services.dns.setDetectedTrrURI("https://autodetect.example.com/provider");
  equal(Services.dns.currentTrrMode, 5);
  equal(Services.dns.currentTrrURI, "https://example.org/provider");
});

add_task(async function test_enterprise_policy_locked() {
  // Read dns.currentTrrMode to make DNS service initialized earlier.
  info("Services.dns.currentTrrMode:" + Services.dns.currentTrrMode);
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      DNSOverHTTPS: {
        Enabled: true,
        ProviderURL: "https://example.com/provider",
        ExcludedDomains: ["example.com", "example.org"],
        Locked: true,
      },
    },
  });

  equal(Services.prefs.getIntPref("network.trr.mode"), 2);
  equal(Services.prefs.prefIsLocked("network.trr.mode"), true);
  equal(
    Services.prefs.getStringPref("network.trr.uri"),
    "https://example.com/provider"
  );
  equal(Services.prefs.prefIsLocked("network.trr.uri"), true);
  equal(
    Services.prefs.getStringPref("network.trr.excluded-domains"),
    "example.com,example.org"
  );
  equal(Services.prefs.prefIsLocked("network.trr.excluded-domains"), true);
  equal(Services.dns.currentTrrMode, 2);
  equal(Services.dns.currentTrrURI, "https://example.com/provider");
  Services.dns.setDetectedTrrURI("https://autodetect.example.com/provider");
  equal(Services.dns.currentTrrURI, "https://example.com/provider");
});
