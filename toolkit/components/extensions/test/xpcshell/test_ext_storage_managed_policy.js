"use strict";

const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
  "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
);

// Load policy engine
Services.policies; // eslint-disable-line no-unused-expressions

AddonTestUtils.init(this);

add_task(async function test_storage_managed_policy() {
  await ExtensionTestUtils.startAddonManager();

  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      "3rdparty": {
        Extensions: {
          "test-storage-managed-policy@mozilla.com": {
            string: "value",
          },
        },
      },
    },
  });

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: {
        gecko: { id: "test-storage-managed-policy@mozilla.com" },
      },
      permissions: ["storage"],
    },

    async background() {
      let str = await browser.storage.managed.get("string");
      browser.test.sendMessage("results", str);
    },
  });

  await extension.startup();
  deepEqual(await extension.awaitMessage("results"), { string: "value" });
  await extension.unload();
});
