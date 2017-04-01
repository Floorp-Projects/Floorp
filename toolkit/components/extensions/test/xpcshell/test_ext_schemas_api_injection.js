"use strict";

Components.utils.import("resource://gre/modules/ExtensionCommon.jsm");
Components.utils.import("resource://gre/modules/Schemas.jsm");

let {
  BaseContext,
  SchemaAPIManager,
} = ExtensionCommon;

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

let global = this;
class StubContext extends BaseContext {
  constructor() {
    let fakeExtension = {id: "test@web.extension"};
    super("addon_child", fakeExtension);
    this.sandbox = Cu.Sandbox(global);
    this.viewType = "background";
  }

  get cloneScope() {
    return this.sandbox;
  }
}

add_task(function* testSchemaAPIInjection() {
  let url = "data:," + JSON.stringify(nestedNamespaceJson);

  // Load the schema of the fake APIs.
  yield Schemas.load(url);

  let apiManager = new SchemaAPIManager("addon");

  // Register an API that will skip the background page.
  apiManager.registerSchemaAPI("noBackgroundAPI.testnamespace", "addon_child", context => {
    // This API should not be available in this context, return null so that
    // the schema wrapper is removed as well.
    return null;
  });

  // Register an API that will skip any but the background page.
  apiManager.registerSchemaAPI("backgroundAPI.testnamespace", "addon_child", context => {
    if (context.viewType === "background") {
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

  let context = new StubContext();
  let browserObj = {};
  apiManager.generateAPIs(context, browserObj);

  do_check_eq(browserObj.noBackgroundAPI, undefined);
  const res = browserObj.backgroundAPI.testnamespace.create("param-value");
  do_check_eq(res, "param-value");
});
