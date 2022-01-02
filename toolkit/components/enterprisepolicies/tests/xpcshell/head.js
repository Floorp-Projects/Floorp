/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);
const { updateAppInfo, getAppInfo } = ChromeUtils.import(
  "resource://testing-common/AppInfo.jsm"
);
const { FileTestUtils } = ChromeUtils.import(
  "resource://testing-common/FileTestUtils.jsm"
);
const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);
const { EnterprisePolicyTesting } = ChromeUtils.import(
  "resource://testing-common/EnterprisePolicyTesting.jsm"
);

updateAppInfo({
  name: "XPCShell",
  ID: "xpcshell@tests.mozilla.org",
  version: "48",
  platformVersion: "48",
});

// This initializes the policy engine for xpcshell tests
let policies = Cc["@mozilla.org/enterprisepolicies;1"].getService(
  Ci.nsIObserver
);
policies.observe(null, "policies-startup", null);

async function setupPolicyEngineWithJson(json, customSchema) {
  if (typeof json != "object") {
    let filePath = do_get_file(json ? json : "non-existing-file.json").path;
    return EnterprisePolicyTesting.setupPolicyEngineWithJson(
      filePath,
      customSchema
    );
  }
  return EnterprisePolicyTesting.setupPolicyEngineWithJson(json, customSchema);
}
