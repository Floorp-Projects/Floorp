/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_extension_incognito_spanning() {
  let wrapper = ExtensionTestUtils.loadExtension({});
  await wrapper.startup();

  let extension = wrapper.extension;
  let data = extension.serialize();
  equal(data.privateBrowsingAllowed, true, "Should have privateBrowsingAllowed in serialized extension");
  equal(extension.privateBrowsingAllowed, true, "Should have privateBrowsingAllowed");
  equal(extension.policy.privateBrowsingAllowed, true, "Should have privateBrowsingAllowed on policy");
  await wrapper.unload();
});

// This tests setting an extension to private-only mode which is useful for testing.
add_task(async function test_extension_incognito_test_mode() {
  let wrapper = ExtensionTestUtils.loadExtension({
    incognitoOverride: "not_allowed",
  });
  await wrapper.startup();

  let extension = wrapper.extension;
  let data = extension.serialize();
  equal(data.privateBrowsingAllowed, false, "Should not have privateBrowsingAllowed in serialized extension");
  equal(extension.privateBrowsingAllowed, false, "Should not have privateBrowsingAllowed");
  equal(extension.policy.privateBrowsingAllowed, false, "Should not have privateBrowsingAllowed on policy");
  await wrapper.unload();
});
