"use strict";

const PREF_DISABLE_SECURITY =
  "security.turn_off_all_security_so_that_" +
  "viruses_can_take_over_this_computer";

const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
  "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
);

// Setting PREF_DISABLE_SECURITY tells the policy engine that we are in testing
// mode and enables restarting the policy engine without restarting the browser.
Services.prefs.setBoolPref(PREF_DISABLE_SECURITY, true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref(PREF_DISABLE_SECURITY);
});

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
      applications: {
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
