/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// In order to use the policy engine inside the xpcshell harness, we need to set
// up a dummy app info.  In the backgroundtask itself, the application under
// test will configure real app info.  This opens a possibility for some
// incompatibility, but there doesn't appear to be such an issue at this time.
const { updateAppInfo } = ChromeUtils.import(
  "resource://testing-common/AppInfo.jsm"
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

add_task(async function test_backgroundtask_policies() {
  let url = "https://www.example.com/";
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      AppUpdateURL: url,
    },
  });

  let filePath = Services.prefs.getStringPref("browser.policies.alternatePath");

  let exitCode = await do_backgroundtask("policies", {
    extraArgs: [filePath, url],
  });
  Assert.equal(0, exitCode);
});
