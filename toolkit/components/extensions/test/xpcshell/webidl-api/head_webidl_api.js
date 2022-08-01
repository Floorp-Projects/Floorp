/* import-globals-from ../head.js */

/* exported getBackgroundServiceWorkerRegistration, waitForTerminatedWorkers,
 *          runExtensionAPITest */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  TestUtils: "resource://testing-common/TestUtils.jsm",
  ExtensionTestCommon: "resource://testing-common/ExtensionTestCommon.jsm",
});

add_setup(function checkExtensionsWebIDLEnabled() {
  equal(
    AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
    true,
    "WebExtensions WebIDL bindings build time flag should be enabled"
  );
});

function getBackgroundServiceWorkerRegistration(extension) {
  const swm = Cc["@mozilla.org/serviceworkers/manager;1"].getService(
    Ci.nsIServiceWorkerManager
  );

  const swRegs = swm.getAllRegistrations();
  const scope = `moz-extension://${extension.uuid}/`;

  for (let i = 0; i < swRegs.length; i++) {
    let regInfo = swRegs.queryElementAt(i, Ci.nsIServiceWorkerRegistrationInfo);
    if (regInfo.scope === scope) {
      return regInfo;
    }
  }
}

function waitForTerminatedWorkers(swRegInfo) {
  info(`Wait all ${swRegInfo.scope} workers to be terminated`);
  return TestUtils.waitForCondition(() => {
    const {
      evaluatingWorker,
      installingWorker,
      waitingWorker,
      activeWorker,
    } = swRegInfo;
    return !(
      evaluatingWorker ||
      installingWorker ||
      waitingWorker ||
      activeWorker
    );
  }, `wait workers for scope ${swRegInfo.scope} to be terminated`);
}

function unmockHandleAPIRequest(extPage) {
  return extPage.spawn([], () => {
    const { ExtensionAPIRequestHandler } = ChromeUtils.import(
      "resource://gre/modules/ExtensionProcessScript.jsm"
    );

    // Unmock ExtensionAPIRequestHandler.
    if (ExtensionAPIRequestHandler._handleAPIRequest_orig) {
      ExtensionAPIRequestHandler.handleAPIRequest =
        ExtensionAPIRequestHandler._handleAPIRequest_orig;
      delete ExtensionAPIRequestHandler._handleAPIRequest_orig;
    }
  });
}

function mockHandleAPIRequest(extPage, mockHandleAPIRequest) {
  mockHandleAPIRequest =
    mockHandleAPIRequest ||
    ((policy, request) => {
      const ExtError = request.window?.Error || Error;
      return {
        type: Ci.mozIExtensionAPIRequestResult.EXTENSION_ERROR,
        value: new ExtError(
          "mockHandleAPIRequest not defined by this test case"
        ),
      };
    });

  return extPage.spawn(
    [ExtensionTestCommon.serializeFunction(mockHandleAPIRequest)],
    mockFnText => {
      const { ExtensionAPIRequestHandler } = ChromeUtils.import(
        "resource://gre/modules/ExtensionProcessScript.jsm"
      );

      mockFnText = `(() => {
        return (${mockFnText});
      })();`;
      // eslint-disable-next-line no-eval
      const mockFn = eval(mockFnText);

      // Mock ExtensionAPIRequestHandler.
      if (!ExtensionAPIRequestHandler._handleAPIRequest_orig) {
        ExtensionAPIRequestHandler._handleAPIRequest_orig =
          ExtensionAPIRequestHandler.handleAPIRequest;
      }

      ExtensionAPIRequestHandler.handleAPIRequest = function(policy, request) {
        if (request.apiNamespace === "test") {
          return this._handleAPIRequest_orig(policy, request);
        }
        return mockFn.call(this, policy, request);
      };
    }
  );
}

/**
 * An helper function used to run unit test that are meant to test the
 * Extension API webidl bindings helpers shared by all the webextensions
 * API namespaces.
 *
 * @param {string} testDescription
 *        Brief description of the test.
 * @param {Object} [options]
 * @param {Function} backgroundScript
 *        Test function running in the extension global. This function
 *        does receive a parameter of type object with the following
 *        properties:
 *        - testLog(message): log a message on the terminal
 *        - testAsserts:
 *          - isErrorInstance(err): throw if err is not an Error instance
 *          - isInstanceOf(value, globalContructorName): throws if value
 *            is not an instance of global[globalConstructorName]
 *          - equal(val, exp, msg): throw an error including msg if
 *            val is not strictly equal to exp.
 * @param {Function} assertResults
 *        Function to be provided to assert the result returned by
 *        `backgroundScript`, or assert the error if it did throw.
 *        This function does receive a parameter of type object with
 *        the following properties:
 *        - testResult: the result returned (and resolved if the return
 *          value was a promise) from the call to `backgroundScript`
 *        - testError: the error raised (or rejected if the return value
 *          value was a promise) from the call to `backgroundScript`
 *        - extension: the extension wrapper created by this helper.
 * @param {Function} mockAPIRequestHandler
 *        Function to be used to mock mozIExtensionAPIRequestHandler.handleAPIRequest
 *        for the purpose of the test.
 *        This function received the same parameter that are listed in the idl
 *        definition (mozIExtensionAPIRequestHandling.webidl).
 * @param {string} [options.extensionId]
 *        Optional extension id for the test extension.
 */
async function runExtensionAPITest(
  testDescription,
  {
    backgroundScript,
    assertResults,
    mockAPIRequestHandler,
    extensionId = "test-ext-api-request-forward@mochitest",
  }
) {
  // Wraps the `backgroundScript` function to be execute in the target
  // extension global (currently only in a background service worker,
  // in follow-ups the same function should also be execute in
  // other supported extension globals, e.g. an extension page and
  // a content script).
  //
  // The test wrapper does also provide to `backgroundScript` some
  // helpers to be used as part of the test, these tests are meant to
  // only cover internals shared by all webidl API bindings through a
  // mock API namespace only available in tests (and so none of the tests
  // written with this helpers should be using the browser.test API namespace).
  function backgroundScriptWrapper(testParams, testFn) {
    const testLog = msg => {
      // console messages emitted by workers are not visible in the test logs if not
      // explicitly collected, and so this testLog helper method does use dump for now
      // (this way the logs will be visibile as part of the test logs).
      dump(`"${testParams.extensionId}": ${msg}\n`);
    };

    const testAsserts = {
      isErrorInstance(err) {
        if (!(err instanceof Error)) {
          throw new Error("Unexpected error: not an instance of Error");
        }
        return true;
      },
      isInstanceOf(value, globalConstructorName) {
        if (!(value instanceof self[globalConstructorName])) {
          throw new Error(
            `Unexpected error: expected instance of ${globalConstructorName}`
          );
        }
        return true;
      },
      equal(val, exp, msg) {
        if (val !== exp) {
          throw new Error(
            `Unexpected error: expected ${exp} but got ${val}. ${msg}`
          );
        }
      },
    };

    testLog(`Evaluating - test case "${testParams.testDescription}"`);
    self.onmessage = async evt => {
      testLog(`Running test case "${testParams.testDescription}"`);

      let testError = null;
      let testResult;
      try {
        testResult = await testFn({ testLog, testAsserts });
      } catch (err) {
        testError = { message: err.message, stack: err.stack };
        testLog(`Unexpected test error: ${err} :: ${err.stack}\n`);
      }

      evt.ports[0].postMessage({ success: !testError, testError, testResult });

      testLog(`Test case "${testParams.testDescription}" executed`);
    };
    testLog(`Wait onmessage event - test case "${testParams.testDescription}"`);
  }

  async function assertTestResult(result) {
    if (assertResults) {
      await assertResults(result);
    } else {
      equal(result.testError, undefined, "Expect no errors");
      ok(result.success, "Test completed successfully");
    }
  }

  async function runTestCaseInWorker({ page, extension }) {
    info(`*** Run test case in an extension service worker`);
    const result = await page.spawn([], async () => {
      const { active } = await content.navigator.serviceWorker.ready;
      const { port1, port2 } = new MessageChannel();

      return new Promise(resolve => {
        port1.onmessage = evt => resolve(evt.data);
        active.postMessage("run-test", [port2]);
      });
    });
    info(`*** Assert test case results got from extension service worker`);
    await assertTestResult({ ...result, extension });
  }

  // NOTE: prefixing this with `function ` is needed because backgroundScript
  // is an object property and so it is going to be stringified as
  // `backgroundScript() { ... }` (which would be detected as a syntax error
  // on the worker script evaluation phase).
  const scriptFnParam = ExtensionTestCommon.serializeFunction(backgroundScript);
  const testOptsParam = `${JSON.stringify({ testDescription, extensionId })}`;

  const testExtData = {
    useAddonManager: "temporary",
    manifest: {
      version: "1",
      background: {
        service_worker: "test-sw.js",
      },
      applications: {
        gecko: { id: extensionId },
      },
    },
    files: {
      "page.html": `<!DOCTYPE html>
        <head><meta charset="utf-8"></head>
        <body>
          <script src="test-sw.js"></script>
        </body>`,
      "test-sw.js": `
        (${backgroundScriptWrapper})(${testOptsParam}, ${scriptFnParam});
      `,
    },
  };

  let cleanupCalled = false;
  let extension;
  let page;
  let swReg;

  async function testCleanup() {
    if (cleanupCalled) {
      return;
    }

    cleanupCalled = true;
    await unmockHandleAPIRequest(page);
    await page.close();
    await extension.unload();
    await waitForTerminatedWorkers(swReg);
  }

  info(`Start test case "${testDescription}"`);
  extension = ExtensionTestUtils.loadExtension(testExtData);
  await extension.startup();

  swReg = getBackgroundServiceWorkerRegistration(extension);
  ok(swReg, "Extension background.service_worker should be registered");

  page = await ExtensionTestUtils.loadContentPage(
    `moz-extension://${extension.uuid}/page.html`,
    { extension }
  );

  registerCleanupFunction(testCleanup);

  await mockHandleAPIRequest(page, mockAPIRequestHandler);
  await runTestCaseInWorker({ page, extension });
  await testCleanup();
  info(`End test case "${testDescription}"`);
}
