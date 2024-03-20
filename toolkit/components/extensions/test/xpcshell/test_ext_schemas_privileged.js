"use strict";

const { ExtensionAPI } = ExtensionCommon;

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
  const schema = [
    {
      namespace: "privileged",
      permissions: ["mozillaAddons"],
      properties: {
        test: {
          type: "any",
        },
      },
    },
  ];

  class API extends ExtensionAPI {
    getAPI() {
      return {
        privileged: {
          test: "hello",
        },
      };
    }
  }

  const modules = {
    privileged: {
      url: URL.createObjectURL(new Blob([API.toString()])),
      schema: `data:,${JSON.stringify(schema)}`,
      scopes: ["addon_parent"],
      paths: [["privileged"]],
    },
  };

  Services.catMan.addCategoryEntry(
    "webextension-modules",
    "test-privileged",
    `data:,${JSON.stringify(modules)}`,
    false,
    false
  );

  await AddonTestUtils.promiseStartupManager();

  registerCleanupFunction(async () => {
    await AddonTestUtils.promiseShutdownManager();
    Services.catMan.deleteCategoryEntry(
      "webextension-modules",
      "test-privileged",
      false
    );
  });
});

add_task(
  {
    // Some builds (e.g. thunderbird) have experiments enabled by default.
    pref_set: [["extensions.experiments.enabled", false]],
  },
  async function test_privileged_namespace_disallowed() {
    // Try accessing the privileged namespace.
    async function testOnce({
      isPrivileged = false,
      temporarilyInstalled = false,
    } = {}) {
      let extension = ExtensionTestUtils.loadExtension({
        manifest: {
          permissions: ["mozillaAddons", "tabs"],
        },
        background() {
          browser.test.sendMessage(
            "result",
            browser.privileged instanceof Object
          );
        },
        isPrivileged,
        temporarilyInstalled,
      });

      if (temporarilyInstalled && !isPrivileged) {
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
                  /Using the privileged permission 'mozillaAddons' requires a privileged add-on/,
              },
            ],
          },
          true
        );
        return null;
      }
      await extension.startup();
      let result = await extension.awaitMessage("result");
      await extension.unload();
      return result;
    }

    // Prevents startup
    let result = await testOnce({ temporarilyInstalled: true });
    equal(
      result,
      null,
      "Privileged namespace should not be accessible to a regular webextension"
    );

    result = await testOnce({ isPrivileged: true });
    equal(
      result,
      true,
      "Privileged namespace should be accessible to a webextension signed with Mozilla Extensions"
    );

    // Allows startup, no access
    result = await testOnce();
    equal(
      result,
      false,
      "Privileged namespace should not be accessible to a regular webextension"
    );
  }
);

// Test that Extension.sys.mjs and schema correctly match.
add_task(function test_privileged_permissions_match() {
  const { PRIVILEGED_PERMS } = ChromeUtils.importESModule(
    "resource://gre/modules/Extension.sys.mjs"
  );
  let perms = Schemas.getPermissionNames(["PermissionPrivileged"]);
  if (AppConstants.platform == "android") {
    perms.push("nativeMessaging");
  }
  Assert.deepEqual(
    Array.from(PRIVILEGED_PERMS).sort(),
    perms.sort(),
    "List of privileged permissions is correct."
  );
});
