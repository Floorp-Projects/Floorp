"use strict";

Components.utils.import("resource://gre/modules/Schemas.jsm");

let {
  Management,
} = Components.utils.import("resource://gre/modules/Extension.jsm", {});

let nestedNamespaceJson = [
  {
    "namespace": "backgroundAPI.testnamespace",
    "functions": [
      {
        "name": "create",
        "type": "function",
        "parameters": [
          {
            "name": "title",
            "type": "string",
          },
        ],
        "returns": {
          "type": "string",
        },
      },
    ],
  },
  {
    "namespace": "noBackgroundAPI.testnamespace",
    "functions": [
      {
        "name": "create",
        "type": "function",
        "parameters": [
          {
            "name": "title",
            "type": "string",
          },
        ],
      },
    ],
  },
];

add_task(function* testSchemaAPIInjection() {
  let url = "data:," + JSON.stringify(nestedNamespaceJson);

  // Load the schema of the fake APIs.
  yield Schemas.load(url);

  // Register an API that will skip the background page.
  Management.registerSchemaAPI("noBackgroundAPI.testnamespace", "addon_child", context => {
    if (context.type !== "background") {
      return {
        noBackgroundAPI: {
          testnamespace: {
            create(title) {},
          },
        },
      };
    }

    // This API should not be available in this context, return null so that
    // the schema wrapper is removed as well.
    return null;
  });

  // Register an API that will skip any but the background page.
  Management.registerSchemaAPI("backgroundAPI.testnamespace", "addon_child", context => {
    if (context.type === "background") {
      return {
        backgroundAPI: {
          testnamespace: {
            create(title) {
              return title;
            },
          },
        },
      };
    }

    // This API should not be available in this context, return null so that
    // the schema wrapper is removed as well.
    return null;
  });

  let extension = ExtensionTestUtils.loadExtension({
    background() {
      if (browser.noBackgroundAPI) {
        browser.test.notifyFail("skipAPIonNull.done");
      } else {
        const res = browser.backgroundAPI.testnamespace.create("param-value");
        browser.test.assertEq("param-value", res,
                              "Got the expected result from the fake API method");
        browser.test.notifyPass("skipAPIonNull.done");
      }
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("skipAPIonNull.done");
  yield extension.unload();
});
