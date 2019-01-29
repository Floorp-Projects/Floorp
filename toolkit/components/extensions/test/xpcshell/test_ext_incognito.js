/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

async function runIncognitoTest(extensionData, privateBrowsingAllowed, allowPrivateBrowsingByDefault) {
  Services.prefs.setBoolPref("extensions.allowPrivateBrowsingByDefault", allowPrivateBrowsingByDefault);

  let wrapper = ExtensionTestUtils.loadExtension(extensionData);
  await wrapper.startup();
  let {extension} = wrapper;

  if (!allowPrivateBrowsingByDefault) {
    // Check the permission if we're not allowPrivateBrowsingByDefault.
    equal(extension.permissions.has("internal:privateBrowsingAllowed"), privateBrowsingAllowed,
          "privateBrowsingAllowed in serialized extension");
  }
  equal(extension.privateBrowsingAllowed, privateBrowsingAllowed,
        "privateBrowsingAllowed in extension");
  equal(extension.policy.privateBrowsingAllowed, privateBrowsingAllowed,
        "privateBrowsingAllowed on policy");

  await wrapper.unload();
  Services.prefs.clearUserPref("extensions.allowPrivateBrowsingByDefault");
}

add_task(async function test_extension_incognito_spanning() {
  await runIncognitoTest({}, false, false);
  await runIncognitoTest({}, true, true);
});

// Test that when we are restricted, we can override the restriction for tests.
add_task(async function test_extension_incognito_override_spanning() {
  let extensionData = {
    incognitoOverride: "spanning",
  };
  await runIncognitoTest(extensionData, true, false);
});

// This tests that a privileged extension will always have private browsing.
add_task(async function test_extension_incognito_privileged() {
  let extensionData = {
    isPrivileged: true,
  };
  await runIncognitoTest(extensionData, true, true);
  await runIncognitoTest(extensionData, true, false);
});
