"use strict";

const { ExtensionCommon } = ChromeUtils.import(
  "resource://gre/modules/ExtensionCommon.jsm"
);
const { ExtensionAPI } = ExtensionCommon;

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

add_task(async function() {
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
    getAPI(context) {
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

  AddonTestUtils.createAppInfo(
    "xpcshell@tests.mozilla.org",
    "XPCShell",
    "1",
    "42"
  );
  await AddonTestUtils.promiseStartupManager();

  // Try accessing the privileged namespace.
  async function testOnce() {
    let extension = ExtensionTestUtils.loadExtension({
      manifest: {
        applications: { gecko: { id: "privilegedapi@tests.mozilla.org" } },
        permissions: ["mozillaAddons"],
      },
      background() {
        browser.test.sendMessage(
          "result",
          browser.privileged instanceof Object
        );
      },
      useAddonManager: "permanent",
    });

    await extension.startup();
    let result = await extension.awaitMessage("result");
    await extension.unload();
    return result;
  }

  AddonTestUtils.usePrivilegedSignatures = false;
  let result = await testOnce();
  equal(
    result,
    false,
    "Privileged namespace should not be accessible to a regular webextension"
  );

  AddonTestUtils.usePrivilegedSignatures = true;
  result = await testOnce();
  equal(
    result,
    true,
    "Privileged namespace should be accessible to a webextension signed with Mozilla Extensions"
  );

  await AddonTestUtils.promiseShutdownManager();
  Services.catMan.deleteCategoryEntry(
    "webextension-modules",
    "test-privileged",
    false
  );
});
