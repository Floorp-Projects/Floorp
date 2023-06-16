/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "43"
);

add_task(async function test_simple() {
  let extensionData = {
    manifest: {
      name: "Simple extension test",
      version: "1.0",
      manifest_version: 2,
      description: "",
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();
  await extension.unload();
});

add_task(async function test_manifest_V3_disabled() {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", false);
  let extensionData = {
    manifest: {
      manifest_version: 3,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await Assert.rejects(
    extension.startup(),
    /Unsupported manifest version: 3/,
    "manifest V3 cannot be loaded"
  );
  Services.prefs.clearUserPref("extensions.manifestV3.enabled");
});

add_task(async function test_manifest_V3_enabled() {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
  let extensionData = {
    manifest: {
      manifest_version: 3,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();
  equal(extension.extension.manifest.manifest_version, 3, "manifest V3 loads");
  await extension.unload();
  Services.prefs.clearUserPref("extensions.manifestV3.enabled");
});

add_task(async function test_background() {
  function background() {
    browser.test.log("running background script");

    browser.test.onMessage.addListener((x, y) => {
      browser.test.assertEq(x, 10, "x is 10");
      browser.test.assertEq(y, 20, "y is 20");

      browser.test.notifyPass("background test passed");
    });

    browser.test.sendMessage("running", 1);
  }

  let extensionData = {
    background,
    manifest: {
      name: "Simple extension test",
      version: "1.0",
      manifest_version: 2,
      description: "",
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);

  let [, x] = await Promise.all([
    extension.startup(),
    extension.awaitMessage("running"),
  ]);
  equal(x, 1, "got correct value from extension");

  extension.sendMessage(10, 20);
  await extension.awaitFinish();
  await extension.unload();
});

add_task(async function test_extensionTypes() {
  let extensionData = {
    background: function () {
      browser.test.assertEq(
        typeof browser.extensionTypes,
        "object",
        "browser.extensionTypes exists"
      );
      browser.test.assertEq(
        typeof browser.extensionTypes.RunAt,
        "object",
        "browser.extensionTypes.RunAt exists"
      );
      browser.test.notifyPass("extentionTypes test passed");
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);

  await extension.startup();
  await extension.awaitFinish();
  await extension.unload();
});

add_task(async function test_policy_temporarilyInstalled() {
  await AddonTestUtils.promiseStartupManager();

  let extensionData = {
    manifest: {
      manifest_version: 2,
    },
  };

  async function runTest(useAddonManager) {
    let extension = ExtensionTestUtils.loadExtension({
      ...extensionData,
      useAddonManager,
    });

    const expected = useAddonManager === "temporary";
    await extension.startup();
    const { temporarilyInstalled } = WebExtensionPolicy.getByID(extension.id);
    equal(
      temporarilyInstalled,
      expected,
      `Got the expected WebExtensionPolicy.temporarilyInstalled value on "${useAddonManager}"`
    );
    await extension.unload();
  }

  await runTest("temporary");
  await runTest("permanent");
});

add_task(async function test_manifest_allowInsecureRequests() {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
  let extensionData = {
    allowInsecureRequests: true,
    manifest: {
      manifest_version: 3,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();
  equal(
    extension.extension.manifest.content_security_policy.extension_pages,
    `script-src 'self'`,
    "insecure allowed"
  );
  await extension.unload();
  Services.prefs.clearUserPref("extensions.manifestV3.enabled");
});

add_task(async function test_manifest_allowInsecureRequests_throws() {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
  let extensionData = {
    allowInsecureRequests: true,
    manifest: {
      manifest_version: 3,
      content_security_policy: {
        extension_pages: `script-src 'self'`,
      },
    },
  };

  await Assert.throws(
    () => ExtensionTestUtils.loadExtension(extensionData),
    /allowInsecureRequests cannot be used with manifest.content_security_policy/,
    "allowInsecureRequests with content_security_policy cannot be loaded"
  );
  Services.prefs.clearUserPref("extensions.manifestV3.enabled");
});

add_task(async function test_gecko_android_key_in_applications() {
  const extensionData = {
    manifest: {
      manifest_version: 2,
      applications: {
        gecko_android: {},
      },
    },
  };
  const extension = ExtensionTestUtils.loadExtension(extensionData);

  await Assert.rejects(
    extension.startup(),
    /applications: Property "gecko_android" is unsupported by Firefox/,
    "expected applications.gecko_android to be invalid"
  );
});
