"use strict";

const { ExtensionCommon } = ChromeUtils.import(
  "resource://gre/modules/ExtensionCommon.jsm"
);
const { ExtensionAPI } = ExtensionCommon;

add_task(async function() {
  const schema = [
    {
      namespace: "manifest",
      types: [
        {
          $extend: "WebExtensionManifest",
          properties: {
            a_manifest_property: {
              type: "object",
              optional: true,
              properties: {
                nested: {
                  optional: true,
                  type: "any",
                },
              },
              additionalProperties: { $ref: "UnrecognizedProperty" },
            },
          },
        },
      ],
    },
    {
      namespace: "testManifestPermission",
      permissions: ["manifest:a_manifest_property"],
      functions: [
        {
          name: "testMethod",
          type: "function",
          async: true,
          parameters: [],
          permissions: ["manifest:a_manifest_property.nested"],
        },
      ],
    },
  ];

  class FakeAPI extends ExtensionAPI {
    getAPI(context) {
      return {
        testManifestPermission: {
          get testProperty() {
            return "value";
          },
          testMethod() {
            return Promise.resolve("value");
          },
        },
      };
    }
  }

  const modules = {
    testNamespace: {
      url: URL.createObjectURL(new Blob([FakeAPI.toString()])),
      schema: `data:,${JSON.stringify(schema)}`,
      scopes: ["addon_parent", "addon_child"],
      paths: [["testManifestPermission"]],
    },
  };

  Services.catMan.addCategoryEntry(
    "webextension-modules",
    "test-manifest-permission",
    `data:,${JSON.stringify(modules)}`,
    false,
    false
  );

  async function testExtension(extensionDef, assertFn) {
    let extension = ExtensionTestUtils.loadExtension(extensionDef);

    await extension.startup();
    await assertFn(extension);
    await extension.unload();
  }

  await testExtension(
    {
      manifest: {
        a_manifest_property: {},
      },
      background() {
        // Test hasPermission method implemented in ExtensionChild.jsm.
        browser.test.assertTrue(
          "testManifestPermission" in browser,
          "The API namespace is defined as expected"
        );
        browser.test.assertEq(
          undefined,
          browser.testManifestPermission &&
            browser.testManifestPermission.testMethod,
          "The property with nested manifest property permission should not be available "
        );
        browser.test.notifyPass("test-extension-manifest-without-nested-prop");
      },
    },
    async extension => {
      await extension.awaitFinish(
        "test-extension-manifest-without-nested-prop"
      );

      // Test hasPermission method implemented in Extension.jsm.
      equal(
        extension.extension.hasPermission("manifest:a_manifest_property"),
        true,
        "Got the expected Extension's hasPermission result on existing property"
      );
      equal(
        extension.extension.hasPermission(
          "manifest:a_manifest_property.nested"
        ),
        false,
        "Got the expected Extension's hasPermission result on existing subproperty"
      );
    }
  );

  await testExtension(
    {
      manifest: {
        a_manifest_property: {
          nested: {},
        },
      },
      background() {
        // Test hasPermission method implemented in ExtensionChild.jsm.
        browser.test.assertTrue(
          "testManifestPermission" in browser,
          "The API namespace is defined as expected"
        );
        browser.test.assertEq(
          "function",
          browser.testManifestPermission &&
            typeof browser.testManifestPermission.testMethod,
          "The property with nested manifest property permission should be available "
        );
        browser.test.notifyPass("test-extension-manifest-with-nested-prop");
      },
    },
    async extension => {
      await extension.awaitFinish("test-extension-manifest-with-nested-prop");

      // Test hasPermission method implemented in Extension.jsm.
      equal(
        extension.extension.hasPermission("manifest:a_manifest_property"),
        true,
        "Got the expected Extension's hasPermission result on existing property"
      );
      equal(
        extension.extension.hasPermission(
          "manifest:a_manifest_property.nested"
        ),
        true,
        "Got the expected Extension's hasPermission result on existing subproperty"
      );
      equal(
        extension.extension.hasPermission(
          "manifest:a_manifest_property.unexisting"
        ),
        false,
        "Got the expected Extension's hasPermission result on non existing subproperty"
      );
    }
  );
});
