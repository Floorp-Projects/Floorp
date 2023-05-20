/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExtensionAPI } = ExtensionCommon;

const API_CLASS = class extends ExtensionAPI {
  getAPI(context) {
    return {
      testMockAPI: {
        async anAsyncAPIMethod(...args) {
          const callContextDataBeforeAwait = context.callContextData;
          await Promise.resolve();
          const callContextDataAfterAwait = context.callContextData;
          return {
            args,
            callContextDataBeforeAwait,
            callContextDataAfterAwait,
          };
        },
      },
    };
  }
};

const API_SCRIPT = `
  this.testMockAPI = ${API_CLASS.toString()};
`;

const API_SCHEMA = [
  {
    namespace: "testMockAPI",
    functions: [
      {
        name: "anAsyncAPIMethod",
        type: "function",
        async: true,
        parameters: [
          {
            name: "param1",
            type: "object",
            additionalProperties: {
              type: "string",
            },
          },
          {
            name: "param2",
            type: "string",
          },
        ],
      },
    ],
  },
];

const MODULE_INFO = {
  testMockAPI: {
    schema: `data:,${JSON.stringify(API_SCHEMA)}`,
    scopes: ["addon_parent"],
    paths: [["testMockAPI"]],
    url: URL.createObjectURL(new Blob([API_SCRIPT])),
  },
};

add_setup(async function () {
  // The blob:-URL registered above in MODULE_INFO gets loaded at
  // https://searchfox.org/mozilla-central/rev/0fec57c05d3996cc00c55a66f20dd5793a9bfb5d/toolkit/components/extensions/ExtensionCommon.jsm#1649
  Services.prefs.setBoolPref(
    "security.allow_parent_unrestricted_js_loads",
    true
  );
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.allow_parent_unrestricted_js_loads");
  });

  ExtensionParent.apiManager.registerModules(MODULE_INFO);
});

add_task(
  async function test_propagated_isHandlingUserInput_on_async_api_methods_calls() {
    const extension = ExtensionTestUtils.loadExtension({
      manifest: {
        browser_specific_settings: { gecko: { id: "@test-ext" } },
      },
      background() {
        browser.test.onMessage.addListener(async (msg, args) => {
          if (msg !== "async-method-call") {
            browser.test.fail(`Unexpected test message: ${msg}`);
            return;
          }

          try {
            let result = await browser.testMockAPI.anAsyncAPIMethod(...args);
            browser.test.sendMessage("async-method-call:result", result);
          } catch (err) {
            browser.test.sendMessage("async-method-call:error", err.message);
          }
        });
      },
    });

    await extension.startup();

    const callArgs = [{ param1: "param1" }, "param2"];

    info("Test API method called without handling user input");

    extension.sendMessage("async-method-call", callArgs);
    const result = await extension.awaitMessage("async-method-call:result");
    Assert.deepEqual(
      result?.args,
      callArgs,
      "Got the expected parameters when called without handling user input"
    );
    Assert.deepEqual(
      result?.callContextDataBeforeAwait,
      { isHandlingUserInput: false },
      "Got the expected callContextData before awaiting on a promise"
    );
    Assert.deepEqual(
      result?.callContextDataAfterAwait,
      null,
      "context.callContextData should have been nullified after awaiting on a promise"
    );

    await withHandlingUserInput(extension, async () => {
      extension.sendMessage("async-method-call", callArgs);
      const result = await extension.awaitMessage("async-method-call:result");
      Assert.deepEqual(
        result?.args,
        callArgs,
        "Got the expected parameters when called while handling user input"
      );
      Assert.deepEqual(
        result?.callContextDataBeforeAwait,
        { isHandlingUserInput: true },
        "Got the expected callContextData before awaiting on a promise"
      );
      Assert.deepEqual(
        result?.callContextDataAfterAwait,
        null,
        "context.callContextData should have been nullified after awaiting on a promise"
      );
    });

    await extension.unload();
  }
);
