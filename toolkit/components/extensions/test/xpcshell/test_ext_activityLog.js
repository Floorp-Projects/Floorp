"use strict";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.usePrivilegedSignatures = false;
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

add_setup(async () => {
  await AddonTestUtils.promiseStartupManager();
});

// This test should produce a warning, but still startup
add_task(async function test_api_restricted() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: {
        gecko: { id: "activityLog-permission@tests.mozilla.org" },
      },
      permissions: ["activityLog"],
    },
    async background() {
      browser.test.assertEq(
        undefined,
        browser.activityLog,
        "activityLog is privileged"
      );
    },
    useAddonManager: "permanent",
  });
  await extension.startup();
  await extension.unload();
});

// This test should produce a error and not startup
add_task(
  {
    // Some builds (e.g. thunderbird) have experiments enabled by default.
    pref_set: [["extensions.experiments.enabled", false]],
  },
  async function test_api_restricted_temporary_without_privilege() {
    let extension = ExtensionTestUtils.loadExtension({
      temporarilyInstalled: true,
      isPrivileged: false,
      manifest: {
        browser_specific_settings: {
          gecko: { id: "activityLog-permission@tests.mozilla.org" },
        },
        permissions: ["activityLog"],
      },
    });
    ExtensionTestUtils.failOnSchemaWarnings(false);
    let { messages } = await promiseConsoleOutput(async () => {
      await Assert.rejects(
        extension.startup(),
        /Using the privileged permission/,
        "Startup failed with privileged permission"
      );
    });
    ExtensionTestUtils.failOnSchemaWarnings(true);
    AddonTestUtils.checkMessages(
      messages,
      {
        expected: [
          {
            message:
              /Using the privileged permission 'activityLog' requires a privileged add-on/,
          },
        ],
      },
      true
    );
  }
);
