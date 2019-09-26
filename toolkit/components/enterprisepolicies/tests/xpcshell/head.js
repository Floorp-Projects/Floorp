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

// Any changes to this function should also be made to the corresponding version
// in toolkit/components/enterprisepolicies/tests/browser/head.js
async function setupPolicyEngineWithJson(json, customSchema) {
  let filePath;
  if (typeof json == "object") {
    filePath = FileTestUtils.getTempFile("policies.json").path;

    // This file gets automatically deleted by FileTestUtils
    // at the end of the test run.
    await OS.File.writeAtomic(filePath, JSON.stringify(json), {
      encoding: "utf-8",
    });
  } else {
    filePath = do_get_file(json ? json : "non-existing-file.json").path;
  }

  Services.prefs.setStringPref("browser.policies.alternatePath", filePath);

  let promise = new Promise(resolve => {
    Services.obs.addObserver(function observer() {
      Services.obs.removeObserver(
        observer,
        "EnterprisePolicies:AllPoliciesApplied"
      );
      resolve();
    }, "EnterprisePolicies:AllPoliciesApplied");
  });

  // Clear any previously used custom schema
  Cu.unload("resource:///modules/policies/schema.jsm");

  if (customSchema) {
    let schemaModule = ChromeUtils.import(
      "resource:///modules/policies/schema.jsm",
      null
    );
    schemaModule.schema = customSchema;
  }

  Services.obs.notifyObservers(null, "EnterprisePolicies:Restart");
  return promise;
}
