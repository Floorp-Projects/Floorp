/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_ext_context_does_have_webidl_bindings() {
  await runExtensionAPITest("should have a browser global object", {
    backgroundScript() {
      const { browser, chrome } = self;

      return {
        hasExtensionAPI: !!browser,
        hasExtensionMockAPI: !!browser?.mockExtensionAPI,
        hasChromeCompatGlobal: !!chrome,
        hasChromeMockAPI: !!chrome?.mockExtensionAPI,
      };
    },
    assertResults({ testResult, testError }) {
      Assert.deepEqual(testError, undefined);
      Assert.deepEqual(
        testResult,
        {
          hasExtensionAPI: true,
          hasExtensionMockAPI: true,
          hasChromeCompatGlobal: true,
          hasChromeMockAPI: true,
        },
        "browser and browser.test WebIDL API bindings found"
      );
    },
  });
});

add_task(async function test_propagated_extension_error() {
  await runExtensionAPITest(
    "should throw an extension error on ResultType::EXTENSION_ERROR",
    {
      backgroundScript({ testAsserts }) {
        try {
          const api = self.browser.mockExtensionAPI;
          api.methodSyncWithReturn("arg0", 1, { value: "arg2" });
        } catch (err) {
          testAsserts.isErrorInstance(err);
          throw err;
        }
      },
      mockAPIRequestHandler(policy, request) {
        return {
          type: Ci.mozIExtensionAPIRequestResult.EXTENSION_ERROR,
          value: new Error("Fake Extension Error"),
        };
      },
      assertResults({ testError }) {
        Assert.deepEqual(testError?.message, "Fake Extension Error");
      },
    }
  );
});

add_task(async function test_system_errors_donot_leak() {
  function assertResults({ testError }) {
    ok(
      testError?.message?.match(/An unexpected error occurred/),
      `Got the general unexpected error as expected: ${testError?.message}`
    );
  }

  function mockAPIRequestHandler(policy, request) {
    throw new Error("Fake handleAPIRequest exception");
  }

  const msg =
    "should throw an unexpected error occurred if handleAPIRequest throws";

  await runExtensionAPITest(`sync method ${msg}`, {
    backgroundScript({ testAsserts }) {
      try {
        self.browser.mockExtensionAPI.methodSyncWithReturn("arg0");
      } catch (err) {
        testAsserts.isErrorInstance(err);
        throw err;
      }
    },
    mockAPIRequestHandler,
    assertResults,
  });

  await runExtensionAPITest(`async method ${msg}`, {
    backgroundScript({ testAsserts }) {
      try {
        self.browser.mockExtensionAPI.methodAsync("arg0");
      } catch (err) {
        testAsserts.isErrorInstance(err);
        throw err;
      }
    },
    mockAPIRequestHandler,
    assertResults,
  });

  await runExtensionAPITest(`no return method ${msg}`, {
    backgroundScript({ testAsserts }) {
      try {
        self.browser.mockExtensionAPI.methodNoReturn("arg0");
      } catch (err) {
        testAsserts.isErrorInstance(err);
        throw err;
      }
    },
    mockAPIRequestHandler,
    assertResults,
  });
});

add_task(async function test_call_sync_function_result() {
  await runExtensionAPITest(
    "sync API methods should support structured clonable return values",
    {
      backgroundScript({ testAsserts }) {
        const api = self.browser.mockExtensionAPI;
        const results = {
          string: api.methodSyncWithReturn("string-result"),
          nested_prop: api.methodSyncWithReturn({
            string: "123",
            number: 123,
            date: new Date("2020-09-20"),
            map: new Map([
              ["a", 1],
              ["b", 2],
            ]),
          }),
        };

        testAsserts.isInstanceOf(results.nested_prop.date, "Date");
        testAsserts.isInstanceOf(results.nested_prop.map, "Map");
        return results;
      },
      mockAPIRequestHandler(policy, request) {
        if (request.apiName === "methodSyncWithReturn") {
          // Return the first argument unmodified, which will be checked in the
          // resultAssertFn above.
          return {
            type: Ci.mozIExtensionAPIRequestResult.RETURN_VALUE,
            value: request.args[0],
          };
        }
        throw new Error("Unexpected API method");
      },
      assertResults({ testResult, testError }) {
        Assert.deepEqual(testError, null, "Got no error as expected");
        Assert.deepEqual(testResult, {
          string: "string-result",
          nested_prop: {
            string: "123",
            number: 123,
            date: new Date("2020-09-20"),
            map: new Map([
              ["a", 1],
              ["b", 2],
            ]),
          },
        });
      },
    }
  );
});

add_task(async function test_call_sync_fn_missing_return() {
  await runExtensionAPITest(
    "should throw an unexpected error occurred on missing return value",
    {
      backgroundScript() {
        self.browser.mockExtensionAPI.methodSyncWithReturn("arg0");
      },
      mockAPIRequestHandler(policy, request) {
        return undefined;
      },
      assertResults({ testError }) {
        ok(
          testError?.message?.match(/An unexpected error occurred/),
          `Got the general unexpected error as expected: ${testError?.message}`
        );
      },
    }
  );
});

add_task(async function test_call_async_throw_extension_error() {
  await runExtensionAPITest(
    "an async function can throw an error occurred for param validation errors",
    {
      backgroundScript({ testAsserts }) {
        try {
          self.browser.mockExtensionAPI.methodAsync("arg0");
        } catch (err) {
          testAsserts.isErrorInstance(err);
          throw err;
        }
      },
      mockAPIRequestHandler(policy, request) {
        return {
          type: Ci.mozIExtensionAPIRequestResult.EXTENSION_ERROR,
          value: new Error("Fake Param Validation Error"),
        };
      },
      assertResults({ testError }) {
        Assert.deepEqual(testError?.message, "Fake Param Validation Error");
      },
    }
  );
});

add_task(async function test_call_async_reject_error() {
  await runExtensionAPITest(
    "an async function rejected promise should propagate extension errors",
    {
      async backgroundScript({ testAsserts }) {
        try {
          await self.browser.mockExtensionAPI.methodAsync("arg0");
        } catch (err) {
          testAsserts.isErrorInstance(err);
          throw err;
        }
      },
      mockAPIRequestHandler(policy, request) {
        return {
          type: Ci.mozIExtensionAPIRequestResult.RETURN_VALUE,
          value: Promise.reject(new Error("Fake API rejected error object")),
        };
      },
      assertResults({ testError }) {
        Assert.deepEqual(testError?.message, "Fake API rejected error object");
      },
    }
  );
});

add_task(async function test_call_async_function_result() {
  await runExtensionAPITest(
    "async API methods should support structured clonable resolved values",
    {
      async backgroundScript({ testAsserts }) {
        const api = self.browser.mockExtensionAPI;
        const results = {
          string: await api.methodAsync("string-result"),
          nested_prop: await api.methodAsync({
            string: "123",
            number: 123,
            date: new Date("2020-09-20"),
            map: new Map([
              ["a", 1],
              ["b", 2],
            ]),
          }),
        };

        testAsserts.isInstanceOf(results.nested_prop.date, "Date");
        testAsserts.isInstanceOf(results.nested_prop.map, "Map");
        return results;
      },
      mockAPIRequestHandler(policy, request) {
        if (request.apiName === "methodAsync") {
          // Return the first argument unmodified, which will be checked in the
          // resultAssertFn above.
          return {
            type: Ci.mozIExtensionAPIRequestResult.RETURN_VALUE,
            value: Promise.resolve(request.args[0]),
          };
        }
        throw new Error("Unexpected API method");
      },
      assertResults({ testResult, testError }) {
        Assert.deepEqual(testError, null, "Got no error as expected");
        Assert.deepEqual(testResult, {
          string: "string-result",
          nested_prop: {
            string: "123",
            number: 123,
            date: new Date("2020-09-20"),
            map: new Map([
              ["a", 1],
              ["b", 2],
            ]),
          },
        });
      },
    }
  );
});

add_task(async function test_call_no_return_throw_extension_error() {
  await runExtensionAPITest(
    "no return function call throw an error occurred for param validation errors",
    {
      backgroundScript({ testAsserts }) {
        try {
          self.browser.mockExtensionAPI.methodNoReturn("arg0");
        } catch (err) {
          testAsserts.isErrorInstance(err);
          throw err;
        }
      },
      mockAPIRequestHandler(policy, request) {
        return {
          type: Ci.mozIExtensionAPIRequestResult.EXTENSION_ERROR,
          value: new Error("Fake Param Validation Error"),
        };
      },
      assertResults({ testError }) {
        Assert.deepEqual(testError?.message, "Fake Param Validation Error");
      },
    }
  );
});

add_task(async function test_call_no_return_without_errors() {
  await runExtensionAPITest(
    "handleAPIHandler can return undefined on api calls to methods with no return",
    {
      backgroundScript() {
        self.browser.mockExtensionAPI.methodNoReturn("arg0");
      },
      mockAPIRequestHandler(policy, request) {
        return undefined;
      },
      assertResults({ testError }) {
        Assert.deepEqual(testError, null, "Got no error as expected");
      },
    }
  );
});

add_task(async function test_async_method_chrome_compatible_callback() {
  function mockAPIRequestHandler(policy, request) {
    if (request.args[0] === "fake-async-method-failure") {
      return {
        type: Ci.mozIExtensionAPIRequestResult.RETURN_VALUE,
        value: Promise.reject("this-should-not-be-passed-to-cb-as-parameter"),
      };
    }

    return {
      type: Ci.mozIExtensionAPIRequestResult.RETURN_VALUE,
      value: Promise.resolve(request.args),
    };
  }

  await runExtensionAPITest(
    "async method should support an optional chrome-compatible callback",
    {
      mockAPIRequestHandler,
      async backgroundScript({ testAsserts }) {
        const api = self.browser.mockExtensionAPI;
        const success_cb_params = await new Promise(resolve => {
          const res = api.methodAsync(
            { prop: "fake-async-method-success" },
            (...results) => {
              resolve(results);
            }
          );
          testAsserts.equal(res, undefined, "no promise should be returned");
        });
        const error_cb_params = await new Promise(resolve => {
          const res = api.methodAsync(
            "fake-async-method-failure",
            (...results) => {
              resolve(results);
            }
          );
          testAsserts.equal(res, undefined, "no promise should be returned");
        });
        return { success_cb_params, error_cb_params };
      },
      assertResults({ testError, testResult }) {
        Assert.deepEqual(testError, null, "Got no error as expected");
        Assert.deepEqual(
          testResult,
          {
            success_cb_params: [[{ prop: "fake-async-method-success" }]],
            error_cb_params: [],
          },
          "Got the expected results from the chrome compatible callbacks"
        );
      },
    }
  );

  await runExtensionAPITest(
    "async method with ambiguous args called with a chrome-compatible callback",
    {
      mockAPIRequestHandler,
      async backgroundScript({ testAsserts }) {
        const api = self.browser.mockExtensionAPI;
        const success_cb_params = await new Promise(resolve => {
          const res = api.methodAmbiguousArgsAsync(
            "arg0",
            { prop: "arg1" },
            3,
            (...results) => {
              resolve(results);
            }
          );
          testAsserts.equal(res, undefined, "no promise should be returned");
        });
        const error_cb_params = await new Promise(resolve => {
          const res = api.methodAmbiguousArgsAsync(
            "fake-async-method-failure",
            (...results) => {
              resolve(results);
            }
          );
          testAsserts.equal(res, undefined, "no promise should be returned");
        });
        return { success_cb_params, error_cb_params };
      },
      assertResults({ testError, testResult }) {
        Assert.deepEqual(testError, null, "Got no error as expected");
        Assert.deepEqual(
          testResult,
          {
            success_cb_params: [["arg0", { prop: "arg1" }, 3]],
            error_cb_params: [],
          },
          "Got the expected results from the chrome compatible callbacks"
        );
      },
    }
  );
});

add_task(async function test_get_property() {
  await runExtensionAPITest(
    "getProperty API request does return a value synchrously",
    {
      backgroundScript() {
        return self.browser.mockExtensionAPI.propertyAsString;
      },
      mockAPIRequestHandler(policy, request) {
        return {
          type: Ci.mozIExtensionAPIRequestResult.RETURN_VALUE,
          value: "property-value",
        };
      },
      assertResults({ testError, testResult }) {
        Assert.deepEqual(testError, null, "Got no error as expected");
        Assert.deepEqual(
          testResult,
          "property-value",
          "Got the expected result"
        );
      },
    }
  );

  await runExtensionAPITest(
    "getProperty API request can return an error object",
    {
      backgroundScript({ testAsserts }) {
        const errObj = self.browser.mockExtensionAPI.propertyAsErrorObject;
        testAsserts.isErrorInstance(errObj);
        testAsserts.equal(errObj.message, "fake extension error");
      },
      mockAPIRequestHandler(policy, request) {
        let savedFrame = request.calledSavedFrame;
        return {
          type: Ci.mozIExtensionAPIRequestResult.RETURN_VALUE,
          value: ChromeUtils.createError("fake extension error", savedFrame),
        };
      },
      assertResults({ testError, testResult }) {
        Assert.deepEqual(testError, null, "Got no error as expected");
      },
    }
  );
});
