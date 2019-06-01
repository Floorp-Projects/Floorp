"use strict";

const {
  createAppInfo,
} = AddonTestUtils;

AddonTestUtils.init(this);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "49");

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

// A small utility function used to test the expected behaviors of the userScripts API method
// wrapper.
async function test_userScript_APIMethod({
  apiScript, userScript, userScriptMetadata, testFn,
  runtimeMessageListener,
}) {
  async function backgroundScript(userScriptFn, scriptMetadata, messageListener) {
    await browser.userScripts.register({
      js: [{
        code: `(${userScriptFn})();`,
      }],
      runAt: "document_end",
      matches: ["http://localhost/*/file_sample.html"],
      scriptMetadata,
    });

    if (messageListener) {
      browser.runtime.onMessage.addListener(messageListener);
    }

    browser.test.sendMessage("background-ready");
  }

  function notifyFinish(failureReason) {
    browser.test.assertEq(undefined, failureReason, "should be completed without errors");
    browser.test.sendMessage("test_userScript_APIMethod:done");
  }

  function assertTrue(val, message) {
    browser.test.assertTrue(val, message);
    if (!val) {
      browser.test.sendMessage("test_userScript_APIMethod:done");
      throw message;
    }
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: [
        "http://localhost/*/file_sample.html",
      ],
      user_scripts: {
        api_script: "api-script.js",
      },
    },
    // Defines a background script that receives all the needed test parameters.
    background: `
        const metadata = ${JSON.stringify(userScriptMetadata)};
        (${backgroundScript})(${userScript}, metadata, ${runtimeMessageListener})
     `,
    files: {
      "api-script.js": `(${apiScript})({
        assertTrue: ${assertTrue},
        notifyFinish: ${notifyFinish}
      })`,
    },
  });

  // Load a page in a content process, register the user script and then load a
  // new page in the existing content process.
  let url = `${BASE_URL}/file_sample.html`;
  let contentPage = await ExtensionTestUtils.loadContentPage(`about:blank`);

  await extension.startup();
  await extension.awaitMessage("background-ready");
  await contentPage.loadURL(url);

  // Run any additional test-specific assertions.
  if (testFn) {
    await testFn({extension, contentPage, url});
  }

  await extension.awaitMessage("test_userScript_APIMethod:done");

  await extension.unload();
  await contentPage.close();
}

add_task(async function test_apiScript_exports_simple_sync_method() {
  function apiScript(sharedTestAPIMethods) {
    browser.userScripts.onBeforeScript.addListener(script => {
      const scriptMetadata = script.metadata;

      script.defineGlobals({
        ...sharedTestAPIMethods,
        testAPIMethod(stringParam, numberParam, boolParam, nullParam, undefinedParam, arrayParam) {
          browser.test.assertEq("test-user-script-exported-apis", scriptMetadata.name,
                                "Got the expected value for a string scriptMetadata property");
          browser.test.assertEq(null, scriptMetadata.nullProperty,
                                "Got the expected value for a null scriptMetadata property");
          browser.test.assertTrue(scriptMetadata.arrayProperty &&
                                  scriptMetadata.arrayProperty.length === 1 &&
                                  scriptMetadata.arrayProperty[0] === "el1",
                                  "Got the expected value for an array scriptMetadata property");
          browser.test.assertTrue(scriptMetadata.objectProperty &&
                                  scriptMetadata.objectProperty.nestedProp === "nestedValue",
                                  "Got the expected value for an object scriptMetadata property");

          browser.test.assertEq("param1", stringParam, "Got the expected string parameter value");
          browser.test.assertEq(123, numberParam, "Got the expected number parameter value");
          browser.test.assertEq(true, boolParam, "Got the expected boolean parameter value");
          browser.test.assertEq(null, nullParam, "Got the expected null parameter value");
          browser.test.assertEq(undefined, undefinedParam, "Got the expected undefined parameter value");

          browser.test.assertEq(3, arrayParam.length, "Got the expected length on the array param");
          browser.test.assertTrue(arrayParam.includes(1),
                                  "Got the expected result when calling arrayParam.includes");

          return "returned_value";
        },
      });
    });
  }

  function userScript() {
    const {assertTrue, notifyFinish, testAPIMethod} = this;

    // Redefine the includes method on the Array prototype, to explicitly verify that the method
    // redefined in the userScript is not used when accessing arrayParam.includes from the API script.
    Array.prototype.includes = () => { // eslint-disable-line no-extend-native
      throw new Error("Unexpected prototype leakage");
    };
    const arrayParam = new Array(1, 2, 3); // eslint-disable-line no-array-constructor
    const result = testAPIMethod("param1", 123, true, null, undefined, arrayParam);

    assertTrue(result === "returned_value", `userScript got an unexpected result value: ${result}`);

    notifyFinish();
  }

  const userScriptMetadata = {
    name: "test-user-script-exported-apis",
    arrayProperty: ["el1"],
    objectProperty: {nestedProp: "nestedValue"},
    nullProperty: null,
  };

  await test_userScript_APIMethod({
    userScript,
    apiScript,
    userScriptMetadata,
  });
});

add_task(async function test_apiScript_async_method() {
  function apiScript(sharedTestAPIMethods) {
    browser.userScripts.onBeforeScript.addListener(script => {
      script.defineGlobals({
        ...sharedTestAPIMethods,
        testAPIMethod(param, cb, cb2, objWithCb) {
          browser.test.assertEq("function", typeof cb, "Got a callback function parameter");
          browser.test.assertTrue(cb === cb2, "Got the same cloned function for the same function parameter");

          browser.runtime.sendMessage(param).then(bgPageRes => {
            const cbResult = cb(script.export(bgPageRes));
            browser.test.sendMessage("user-script-callback-return", cbResult);
          });

          return "resolved_value";
        },
      });
    });
  }

  async function userScript() {
    // Redefine Promise to verify that it doesn't break the WebExtensions internals
    // that are going to use them.
    const {Promise} = this;
    Promise.resolve = function() {
      throw new Error("Promise.resolve poisoning");
    };
    this.Promise = function() {
      throw new Error("Promise constructor poisoning");
    };

    const {assertTrue, notifyFinish, testAPIMethod} = this;

    const cb = (cbParam) => {
      return `callback param: ${JSON.stringify(cbParam)}`;
    };
    const cb2 = cb;
    const asyncAPIResult = await testAPIMethod("param3", cb, cb2);

    assertTrue(asyncAPIResult === "resolved_value",
               `userScript got an unexpected resolved value: ${asyncAPIResult}`);

    notifyFinish();
  }

  async function runtimeMessageListener(param) {
    if (param !== "param3") {
      browser.test.fail(`Got an unexpected message: ${param}`);
    }

    return {bgPageReply: true};
  }

  await test_userScript_APIMethod({
    userScript,
    apiScript,
    runtimeMessageListener,
    async testFn({extension}) {
      const res = await extension.awaitMessage("user-script-callback-return");
      equal(res, `callback param: ${JSON.stringify({bgPageReply: true})}`,
            "Got the expected userScript callback return value");
    },
  });
});

add_task(async function test_apiScript_method_with_webpage_objects_params() {
  function apiScript(sharedTestAPIMethods) {
    browser.userScripts.onBeforeScript.addListener(script => {
      script.defineGlobals({
        ...sharedTestAPIMethods,
        testAPIMethod(windowParam, documentParam) {
          browser.test.assertEq(window, windowParam, "Got a reference to the native window as first param");
          browser.test.assertEq(window.document, documentParam,
                                "Got a reference to the native document as second param");

          // Return an uncloneable webpage object, which checks that if the returned object is from a principal
          // that is subsumed by the userScript sandbox principal, it is returned without being cloned.
          return windowParam;
        },
      });
    });
  }

  async function userScript() {
    const {assertTrue, notifyFinish, testAPIMethod} = this;

    const result = testAPIMethod(window, document);

    // We expect the returned value to be the uncloneable window object.
    assertTrue(result === window,
               `userScript got an unexpected returned value: ${result}`);
    notifyFinish();
  }

  await test_userScript_APIMethod({
    userScript,
    apiScript,
  });
});

add_task(async function test_apiScript_method_got_param_with_methods() {
  function apiScript(sharedTestAPIMethods) {
    browser.userScripts.onBeforeScript.addListener(script => {
      const scriptGlobal = script.global;
      const ScriptFunction = scriptGlobal.Function;

      script.defineGlobals({
        ...sharedTestAPIMethods,
        testAPIMethod(objWithMethods) {
          browser.test.assertEq("objPropertyValue", objWithMethods && objWithMethods.objProperty,
                                "Got the expected property on the object passed as a parameter");
          browser.test.assertEq(undefined, typeof objWithMethods && objWithMethods.objMethod,
                                "XrayWrapper should deny access to a callable property");

          browser.test.assertTrue(
            objWithMethods && objWithMethods.wrappedJSObject &&
            objWithMethods.wrappedJSObject.objMethod instanceof ScriptFunction.wrappedJSObject,
            "The callable property is accessible on the wrappedJSObject");

          browser.test.assertEq("objMethodResult: p1", objWithMethods && objWithMethods.wrappedJSObject &&
                                objWithMethods.wrappedJSObject.objMethod("p1"),
                                "Got the expected result when calling the method on the wrappedJSObject");
          return true;
        },
      });
    });
  }

  async function userScript() {
    const {assertTrue, notifyFinish, testAPIMethod} = this;

    let result = testAPIMethod({
      objProperty: "objPropertyValue",
      objMethod(param) {
        return `objMethodResult: ${param}`;
      },
    });

    assertTrue(result === true, `userScript got an unexpected returned value: ${result}`);
    notifyFinish();
  }

  await test_userScript_APIMethod({
    userScript,
    apiScript,
  });
});

add_task(async function test_apiScript_method_throws_errors() {
  function apiScript({notifyFinish}) {
    let proxyTrapsCount = 0;

    browser.userScripts.onBeforeScript.addListener(script => {
      const scriptGlobals = {
        Error: script.global.Error,
        TypeError: script.global.TypeError,
        Proxy: script.global.Proxy,
      };

      script.defineGlobals({
        notifyFinish,
        testAPIMethod(errorTestName, returnRejectedPromise) {
          let err;

          switch (errorTestName) {
            case "apiScriptError":
              err = new Error(`${errorTestName} message`);
              break;
            case "apiScriptThrowsPlainString":
              err = `${errorTestName} message`;
              break;
            case "apiScriptThrowsNull":
              err = null;
              break;
            case "userScriptError":
              err = new scriptGlobals.Error(`${errorTestName} message`);
              break;
            case "userScriptTypeError":
              err = new scriptGlobals.TypeError(`${errorTestName} message`);
              break;
            case "userScriptProxyObject":
              let proxyTarget = script.export({
                name: "ProxyObject", message: "ProxyObject message",
              });
              let proxyHandlers = script.export({
                get(target, prop) {
                  proxyTrapsCount++;
                  switch (prop) {
                    case "name":
                      return "ProxyObjectGetName";
                    case "message":
                      return "ProxyObjectGetMessage";
                  }
                  return undefined;
                },
                getPrototypeOf() {
                  proxyTrapsCount++;
                  return scriptGlobals.TypeError;
                },
              });
              err = new scriptGlobals.Proxy(proxyTarget, proxyHandlers);
              break;
            default:
              browser.test.fail(`Unknown ${errorTestName} error testname`);
              return undefined;
          }

          if (returnRejectedPromise) {
            return Promise.reject(err);
          }

          throw err;
        },
        assertNoProxyTrapTriggered() {
          browser.test.assertEq(0, proxyTrapsCount, "Proxy traps should not be triggered");
        },
        resetProxyTrapCounter() {
          proxyTrapsCount = 0;
        },
        sendResults(results) {
          browser.test.sendMessage("test-results", results);
        },
      });
    });
  }

  async function userScript() {
    const {
      assertNoProxyTrapTriggered,
      notifyFinish,
      resetProxyTrapCounter,
      sendResults,
      testAPIMethod,
    } = this;

    let apiThrowResults = {};
    let apiThrowTestCases = [
      "apiScriptError",
      "apiScriptThrowsPlainString",
      "apiScriptThrowsNull",
      "userScriptError",
      "userScriptTypeError",
      "userScriptProxyObject",
    ];
    for (let errorTestName of apiThrowTestCases) {
      try {
        testAPIMethod(errorTestName);
      } catch (err) {
        // We expect that no proxy traps have been triggered by the WebExtensions internals.
        if (errorTestName === "userScriptProxyObject") {
          assertNoProxyTrapTriggered();
        }

        if (err instanceof Error) {
          apiThrowResults[errorTestName] = {name: err.name, message: err.message};
        } else {
          apiThrowResults[errorTestName] = {
            name: err && err.name,
            message: err && err.message,
            typeOf: typeof err,
            value: err,
          };
        }
      }
    }

    sendResults(apiThrowResults);

    resetProxyTrapCounter();

    let apiRejectsResults = {};
    for (let errorTestName of apiThrowTestCases) {
      try {
        await testAPIMethod(errorTestName, true);
      } catch (err) {
        // We expect that no proxy traps have been triggered by the WebExtensions internals.
        if (errorTestName === "userScriptProxyObject") {
          assertNoProxyTrapTriggered();
        }

        if (err instanceof Error) {
          apiRejectsResults[errorTestName] = {name: err.name, message: err.message};
        } else {
          apiRejectsResults[errorTestName] = {
            name: err && err.name,
            message: err && err.message,
            typeOf: typeof err,
            value: err,
          };
        }
      }
    }

    sendResults(apiRejectsResults);

    notifyFinish();
  }

  await test_userScript_APIMethod({
    userScript,
    apiScript,
    async testFn({extension}) {
      const expectedResults = {
        // Any error not explicitly raised as a userScript objects or error instance is
        // expected to be turned into a generic error message.
        "apiScriptError": {name: "Error", message: "An unexpected apiScript error occurred"},

        // When the api script throws a primitive value, we expect to receive it unmodified on
        // the userScript side.
        "apiScriptThrowsPlainString": {
          typeOf: "string", value: "apiScriptThrowsPlainString message",
          name: undefined, message: undefined,
        },
        "apiScriptThrowsNull": {
          typeOf: "object", value: null,
          name: undefined, message: undefined,
        },

        // Error messages that the apiScript has explicitly created as userScript's Error
        // global instances are expected to be passing through unmodified.
        "userScriptError": {name: "Error", message: "userScriptError message"},
        "userScriptTypeError": {name: "TypeError", message: "userScriptTypeError message"},

        // Error raised from the apiScript as userScript proxy objects are expected to
        // be passing through unmodified.
        "userScriptProxyObject": {
          typeOf: "object",  name: "ProxyObjectGetName", message: "ProxyObjectGetMessage",
        },
      };

      info("Checking results from errors raised from an apiScript exported function");

      const apiThrowResults = await extension.awaitMessage("test-results");

      for (let [key, expected] of Object.entries(expectedResults)) {
        Assert.deepEqual(apiThrowResults[key], expected,
                         `Got the expected error object for test case "${key}"`);
      }

      Assert.deepEqual(Object.keys(expectedResults).sort(),
                       Object.keys(apiThrowResults).sort(),
                       "the expected and actual test case names matches");

      info("Checking expected results from errors raised from an apiScript exported function");

      // Verify expected results from rejected promises returned from an apiScript exported function.
      const apiThrowRejections = await extension.awaitMessage("test-results");

      for (let [key, expected] of Object.entries(expectedResults)) {
        Assert.deepEqual(apiThrowRejections[key], expected,
                         `Got the expected rejected object for test case "${key}"`);
      }

      Assert.deepEqual(Object.keys(expectedResults).sort(),
                       Object.keys(apiThrowRejections).sort(),
                       "the expected and actual test case names matches");
    },
  });
});

add_task(async function test_apiScript_method_ensure_xraywrapped_proxy_in_params() {
  function apiScript(sharedTestAPIMethods) {
    browser.userScripts.onBeforeScript.addListener(script => {
      script.defineGlobals({
        ...sharedTestAPIMethods,
        testAPIMethod(...args) {
          // Proxies are opaque when wrapped in Xrays, and the proto of an opaque object
          // is supposed to be Object.prototype.
          browser.test.assertEq(
            script.global.Object.prototype,
            Object.getPrototypeOf(args[0]),
            "Calling getPrototypeOf on the XrayWrapped proxy object doesn't run the proxy trap");

          browser.test.assertTrue(Array.isArray(args[0]),
                                  "Got an array object for the XrayWrapped proxy object param");
          browser.test.assertEq(undefined, args[0].length,
                                "XrayWrappers deny access to the length property");
          browser.test.assertEq(undefined, args[0][0],
                                "Got the expected item in the array object");
          return true;
        },
      });
    });
  }

  async function userScript() {
    const {
      assertTrue,
      notifyFinish,
      testAPIMethod,
    } = this;

    let proxy = new Proxy(["expectedArrayValue"], {
      getPrototypeOf() {
        throw new Error("Proxy's getPrototypeOf trap");
      },
      get(target, prop, receiver) {
        throw new Error("Proxy's get trap");
      },
    });

    let result = testAPIMethod(proxy);

    assertTrue(result, `userScript got an unexpected returned value: ${result}`);
    notifyFinish();
  }

  await test_userScript_APIMethod({
    userScript,
    apiScript,
  });
});

add_task(async function test_apiScript_method_return_proxy_object() {
  function apiScript(sharedTestAPIMethods) {
    let proxyTrapsCount = 0;
    let scriptTrapsCount = 0;

    browser.userScripts.onBeforeScript.addListener(script => {
      script.defineGlobals({
        ...sharedTestAPIMethods,
        testAPIMethodError() {
          return new Proxy(["expectedArrayValue"], {
            getPrototypeOf(target) {
              proxyTrapsCount++;
              return Object.getPrototypeOf(target);
            },
          });
        },
        testAPIMethodOk() {
          return new script.global.Proxy(
            script.export(["expectedArrayValue"]),
            script.export({
              getPrototypeOf(target) {
                scriptTrapsCount++;
                return script.global.Object.getPrototypeOf(target);
              },
            }));
        },
        assertNoProxyTrapTriggered() {
          browser.test.assertEq(0, proxyTrapsCount, "Proxy traps should not be triggered");
        },
        assertScriptProxyTrapsCount(expected) {
          browser.test.assertEq(expected, scriptTrapsCount, "Script Proxy traps should have been triggered");
        },
      });
    });
  }

  async function userScript() {
    const {
      assertTrue,
      assertNoProxyTrapTriggered,
      assertScriptProxyTrapsCount,
      notifyFinish,
      testAPIMethodError,
      testAPIMethodOk,
    } = this;

    let error;
    try {
      let result = testAPIMethodError();
      notifyFinish(`Unexpected returned value while expecting error: ${result}`);
      return;
    } catch (err) {
      error = err;
    }

    assertTrue(error && error.message.includes("Return value not accessible to the userScript"),
               `Got an unexpected error message: ${error}`);

    error = undefined;
    try {
      let result = testAPIMethodOk();
      assertScriptProxyTrapsCount(0);
      if (!(result instanceof Array)) {
        notifyFinish(`Got an unexpected result: ${result}`);
        return;
      }
      assertScriptProxyTrapsCount(1);
    } catch (err) {
      error = err;
    }

    assertTrue(!error, `Got an unexpected error: ${error}`);

    assertNoProxyTrapTriggered();

    notifyFinish();
  }

  await test_userScript_APIMethod({
    userScript,
    apiScript,
  });
});

add_task(async function test_apiScript_returns_functions() {
  function apiScript(sharedTestAPIMethods) {
    browser.userScripts.onBeforeScript.addListener(script => {
      script.defineGlobals({
        ...sharedTestAPIMethods,
        testAPIReturnsFunction() {
          // Return a function with provides the same kind of behavior
          // of the API methods exported as globals.
          return script.export(() => window);
        },
        testAPIReturnsObjWithMethod() {
          return script.export({
            getWindow() {
              return window;
            },
          });
        },
      });
    });
  }

  async function userScript() {
    const {
      assertTrue,
      notifyFinish,
      testAPIReturnsFunction,
      testAPIReturnsObjWithMethod,
    } = this;

    let resultFn = testAPIReturnsFunction();
    assertTrue(typeof resultFn === "function",
               `userScript got an unexpected returned value: ${typeof resultFn}`);

    let fnRes = resultFn();
    assertTrue(fnRes === window,
               `Got an unexpected value from the returned function: ${fnRes}`);

    let resultObj = testAPIReturnsObjWithMethod();
    let actualTypeof = resultObj && typeof resultObj.getWindow;
    assertTrue(actualTypeof === "function",
               `Returned object does not have the expected getWindow method: ${actualTypeof}`);

    let methodRes = resultObj.getWindow();
    assertTrue(methodRes === window,
               `Got an unexpected value from the returned method: ${methodRes}`);

    notifyFinish();
  }

  await test_userScript_APIMethod({
    userScript,
    apiScript,
  });
});

add_task(async function test_apiScript_method_clone_non_subsumed_returned_values() {
  function apiScript(sharedTestAPIMethods) {
    browser.userScripts.onBeforeScript.addListener(script => {
      script.defineGlobals({
        ...sharedTestAPIMethods,
        testAPIMethodReturnOk() {
          return script.export({
            objKey1: {
              nestedProp: "nestedvalue",
            },
            window,
          });
        },
        testAPIMethodExplicitlyClonedError() {
          let result = script.export({apiScopeObject: undefined});

          browser.test.assertThrows(
            () => {
              result.apiScopeObject = {disallowedProp: "disallowedValue"};
            },
            /Not allowed to define cross-origin object as property on .* XrayWrapper/,
            "Assigning a property to a xRayWrapper is expected to throw");

          // Let the exception to be raised, so that we check that the actual underlying
          // error message is not leaking in the userScript (replaced by the generic
          // "An unexpected apiScript error occurred" error message).
          result.apiScopeObject = {disallowedProp: "disallowedValue"};
        },
      });
    });
  }

  async function userScript() {
    const {
      assertTrue,
      notifyFinish,
      testAPIMethodReturnOk,
      testAPIMethodExplicitlyClonedError,
    } = this;

    let result = testAPIMethodReturnOk();

    assertTrue(result && ("objKey1" in result) && result.objKey1.nestedProp === "nestedvalue",
               `userScript got an unexpected returned value: ${result}`);

    assertTrue(result.window === window,
               `userScript should have access to the window property: ${result.window}`);

    let error;
    try {
      result = testAPIMethodExplicitlyClonedError();
      notifyFinish(`Unexpected returned value while expecting error: ${result}`);
      return;
    } catch (err) {
      error = err;
    }

    // We expect the generic "unexpected apiScript error occurred" to be raised to the
    // userScript code.
    assertTrue(error && error.message.includes("An unexpected apiScript error occurred"),
               `Got an unexpected error message: ${error}`);

    notifyFinish();
  }

  await test_userScript_APIMethod({
    userScript,
    apiScript,
  });
});

add_task(async function test_apiScript_method_export_primitive_types() {
  function apiScript(sharedTestAPIMethods) {
    browser.userScripts.onBeforeScript.addListener(script => {
      script.defineGlobals({
        ...sharedTestAPIMethods,
        testAPIMethod(typeToExport) {
          switch (typeToExport) {
            case "boolean": return script.export(true);
            case "number": return script.export(123);
            case "string": return script.export("a string");
            case "symbol": return script.export(Symbol("a symbol"));
          }
          return undefined;
        },
      });
    });
  }

  async function userScript() {
    const {assertTrue, notifyFinish, testAPIMethod} = this;

    let v = testAPIMethod("boolean");
    assertTrue(v === true, `Should export a boolean`);

    v = testAPIMethod("number");
    assertTrue(v === 123, `Should export a number`);

    v = testAPIMethod("string");
    assertTrue(v === "a string", `Should export a string`);

    v = testAPIMethod("symbol");
    assertTrue(typeof v === "symbol", `Should export a symbol`);

    notifyFinish();
  }

  await test_userScript_APIMethod({
    userScript,
    apiScript,
  });
});

add_task(async function test_apiScript_method_avoid_unnecessary_params_cloning() {
  function apiScript(sharedTestAPIMethods) {
    browser.userScripts.onBeforeScript.addListener(script => {
      script.defineGlobals({
        ...sharedTestAPIMethods,
        testAPIMethodReturnsParam(param) {
          return param;
        },
        testAPIMethodReturnsUnwrappedParam(param) {
          return param.wrappedJSObject;
        },
      });
    });
  }

  async function userScript() {
    const {
      assertTrue,
      notifyFinish,
      testAPIMethodReturnsParam,
      testAPIMethodReturnsUnwrappedParam,
    } = this;

    let obj = {};

    let result = testAPIMethodReturnsParam(obj);

    assertTrue(result === obj,
               `Expect returned value to be strictly equal to the API method parameter`);

    result = testAPIMethodReturnsUnwrappedParam(obj);

    assertTrue(result === obj,
               `Expect returned value to be strictly equal to the unwrapped API method parameter`);

    notifyFinish();
  }

  await test_userScript_APIMethod({
    userScript,
    apiScript,
  });
});

add_task(async function test_apiScript_method_export_sparse_arrays() {
  function apiScript(sharedTestAPIMethods) {
    browser.userScripts.onBeforeScript.addListener(script => {
      script.defineGlobals({
        ...sharedTestAPIMethods,
        testAPIMethod() {
          const sparseArray = [];
          sparseArray[3] = "third-element";
          sparseArray[5] = "fifth-element";
          return script.export(sparseArray);
        },
      });
    });
  }

  async function userScript() {
    const {assertTrue, notifyFinish, testAPIMethod} = this;

    const result = testAPIMethod(window, document);

    // We expect the returned value to be the uncloneable window object.
    assertTrue(result && result.length === 6,
               `the returned value should be an array of the expected length: ${result}`);
    assertTrue(result[3] === "third-element",
               `the third array element should have the expected value: ${result[3]}`);
    assertTrue(result[5] === "fifth-element",
               `the fifth array element should have the expected value: ${result[5]}`);
    assertTrue(result[0] === undefined,
               `the first array element should have the expected value: ${result[0]}`);
    assertTrue(!("0" in result), "Holey array should still be holey");

    notifyFinish();
  }

  await test_userScript_APIMethod({
    userScript,
    apiScript,
  });
});
