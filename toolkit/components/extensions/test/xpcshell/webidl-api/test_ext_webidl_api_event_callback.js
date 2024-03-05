/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* import-globals-from ../head_service_worker.js */

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

add_task(async function test_api_event_manager_methods() {
  await runExtensionAPITest("extension event manager methods", {
    backgroundScript({ testAsserts }) {
      const api = browser.mockExtensionAPI;
      const listener = () => {};

      function assertHasListener(expect) {
        testAsserts.equal(
          api.onTestEvent.hasListeners(),
          expect,
          `onTestEvent.hasListeners should return {expect}`
        );
        testAsserts.equal(
          api.onTestEvent.hasListener(listener),
          expect,
          `onTestEvent.hasListeners should return {expect}`
        );
      }

      assertHasListener(false);
      api.onTestEvent.addListener(listener);
      assertHasListener(true);
      api.onTestEvent.removeListener(listener);
      assertHasListener(false);
    },
    mockAPIRequestHandler(policy, request) {
      if (!request.eventListener) {
        throw new Error(
          "Unexpected Error: missing ExtensionAPIRequest.eventListener"
        );
      }
    },
    assertResults({ testError }) {
      Assert.deepEqual(testError, null, "Got no error as expected");
    },
  });
});

add_task(async function test_api_event_eventListener_call() {
  await runExtensionAPITest(
    "extension event eventListener wrapper does forward calls parameters",
    {
      backgroundScript({ testAsserts, testLog }) {
        const api = browser.mockExtensionAPI;
        let listener;

        return new Promise((resolve, reject) => {
          testLog("addListener and wait for event to be fired");
          listener = (...args) => {
            testLog("onTestEvent");
            // Make sure the extension code can access the arguments.
            try {
              testAsserts.equal(args[1], "arg1");
              resolve(args);
            } catch (err) {
              reject(err);
            }
          };
          api.onTestEvent.addListener(listener);
        });
      },
      mockAPIRequestHandler(policy, request) {
        if (!request.eventListener) {
          throw new Error(
            "Unexpected Error: missing ExtensionAPIRequest.eventListener"
          );
        }
        if (request.requestType === "addListener") {
          let args = [{ arg: 0 }, "arg1"];
          request.eventListener.callListener(args);
        }
      },
      assertResults({ testError, testResult }) {
        Assert.deepEqual(testError, null, "Got no error as expected");
        Assert.deepEqual(
          testResult,
          [{ arg: 0 }, "arg1"],
          "Got the expected result"
        );
      },
    }
  );
});

add_task(async function test_api_event_eventListener_call_with_result() {
  await runExtensionAPITest(
    "extension event eventListener wrapper forwarded call result",
    {
      backgroundScript({ testLog }) {
        const api = browser.mockExtensionAPI;
        let listener;

        return new Promise((resolve, reject) => {
          testLog("addListener and wait for event to be fired");
          listener = (msg, value) => {
            testLog(`onTestEvent received: ${msg}`);
            switch (msg) {
              case "test-result-value":
                return value;
              case "test-promise-resolve":
                return Promise.resolve(value);
              case "test-promise-reject":
                return Promise.reject(new Error("test-reject"));
              case "test-done":
                resolve(value);
                break;
              default:
                reject(new Error(`Unexpected onTestEvent message: ${msg}`));
            }
          };
          api.onTestEvent.addListener(listener);
        });
      },
      assertResults({ testError, testResult }) {
        Assert.deepEqual(testError, null, "Got no error as expected");
        Assert.deepEqual(
          testResult?.resSync,
          { prop: "retval" },
          "Got result from eventListener returning a plain return value"
        );
        Assert.deepEqual(
          testResult?.resAsync,
          { prop: "promise" },
          "Got result from eventListener returning a resolved promise"
        );
        Assert.deepEqual(
          testResult?.resAsyncReject,
          {
            isInstanceOfError: true,
            errorMessage: "test-reject",
          },
          "got result from eventListener returning a rejected promise"
        );
      },
      mockAPIRequestHandler(policy, request) {
        if (!request.eventListener) {
          throw new Error(
            "Unexpected Error: missing ExtensionAPIRequest.eventListener"
          );
        }

        if (request.requestType === "addListener") {
          Promise.resolve().then(async () => {
            try {
              dump(`calling listener, expect a plain return value\n`);
              const resSync = await request.eventListener.callListener([
                "test-result-value",
                { prop: "retval" },
              ]);

              dump(
                `calling listener, expect a resolved promise return value\n`
              );
              const resAsync = await request.eventListener.callListener([
                "test-promise-resolve",
                { prop: "promise" },
              ]);

              dump(
                `calling listener, expect a rejected promise return value\n`
              );
              const resAsyncReject = await request.eventListener
                .callListener(["test-promise-reject"])
                .catch(err => err);

              // call API listeners once more to complete the test
              let args = {
                resSync,
                resAsync,
                resAsyncReject: {
                  isInstanceOfError: resAsyncReject instanceof Error,
                  errorMessage: resAsyncReject?.message,
                },
              };
              request.eventListener.callListener(["test-done", args]);
            } catch (err) {
              dump(`Unexpected error: ${err} :: ${err.stack}\n`);
              throw err;
            }
          });
        }
      },
    }
  );
});

add_task(async function test_api_event_eventListener_result_rejected() {
  await runExtensionAPITest(
    "extension event eventListener throws (mozIExtensionCallback.call)",
    {
      backgroundScript({ testLog }) {
        const api = browser.mockExtensionAPI;
        let listener;

        return new Promise(resolve => {
          testLog("addListener and wait for event to be fired");
          listener = (msg, arg1) => {
            if (msg === "test-done") {
              testLog(`Resolving result: ${JSON.stringify(arg1)}`);
              resolve(arg1);
              return;
            }
            throw new Error("FAKE eventListener exception");
          };
          api.onTestEvent.addListener(listener);
        });
      },
      assertResults({ testError, testResult }) {
        Assert.deepEqual(testError, null, "Got no error as expected");
        Assert.deepEqual(
          testResult,
          {
            isPromise: true,
            rejectIsError: true,
            errorMessage: "FAKE eventListener exception",
          },
          "Got the expected rejected promise"
        );
      },
      mockAPIRequestHandler(policy, request) {
        if (!request.eventListener) {
          throw new Error(
            "Unexpected Error: missing ExtensionAPIRequest.eventListener"
          );
        }

        if (request.requestType === "addListener") {
          Promise.resolve().then(async () => {
            const promiseResult = request.eventListener.callListener([]);
            const isPromise = promiseResult instanceof Promise;
            const err = await promiseResult.catch(e => e);
            const rejectIsError = err instanceof Error;
            request.eventListener.callListener([
              "test-done",
              { isPromise, rejectIsError, errorMessage: err?.message },
            ]);
          });
        }
      },
    }
  );
});

add_task(async function test_api_event_eventListener_throws_on_call() {
  await runExtensionAPITest(
    "extension event eventListener throws (mozIExtensionCallback.call)",
    {
      backgroundScript({ testLog }) {
        const api = browser.mockExtensionAPI;
        let listener;

        return new Promise(resolve => {
          testLog("addListener and wait for event to be fired");
          listener = (msg, arg1) => {
            if (msg === "test-done") {
              testLog(`Resolving result: ${JSON.stringify(arg1)}`);
              resolve();
              return;
            }
            throw new Error("FAKE eventListener exception");
          };
          api.onTestEvent.addListener(listener);
        });
      },
      assertResults({ testError }) {
        Assert.deepEqual(testError, null, "Got no error as expected");
      },
      mockAPIRequestHandler(policy, request) {
        if (!request.eventListener) {
          throw new Error(
            "Unexpected Error: missing ExtensionAPIRequest.eventListener"
          );
        }

        if (request.requestType === "addListener") {
          Promise.resolve().then(async () => {
            request.eventListener.callListener([]);
            request.eventListener.callListener(["test-done"]);
          });
        }
      },
    }
  );
});

add_task(async function test_send_response_eventListener() {
  await runExtensionAPITest(
    "extension event eventListener sendResponse eventListener argument",
    {
      backgroundScript({ testLog }) {
        const api = browser.mockExtensionAPI;
        let listener;

        return new Promise(resolve => {
          testLog("addListener and wait for event to be fired");
          listener = (msg, sendResponse) => {
            if (msg === "call-sendResponse") {
              // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
              setTimeout(() => sendResponse("sendResponse-value"), 20);
              return true;
            }

            resolve(msg);
          };
          api.onTestEvent.addListener(listener);
        });
      },
      assertResults({ testError, testResult }) {
        Assert.deepEqual(testError, null, "Got no error as expected");
        Assert.equal(testResult, "sendResponse-value", "Got expected value");
      },
      mockAPIRequestHandler(policy, request) {
        if (!request.eventListener) {
          throw new Error(
            "Unexpected Error: missing ExtensionAPIRequest.eventListener"
          );
        }

        if (request.requestType === "addListener") {
          Promise.resolve().then(async () => {
            const res = await request.eventListener.callListener(
              ["call-sendResponse"],
              {
                callbackType:
                  Ci.mozIExtensionListenerCallOptions.CALLBACK_SEND_RESPONSE,
              }
            );
            request.eventListener.callListener([res]);
          });
        }
      },
    }
  );
});

add_task(async function test_send_response_multiple_eventListener() {
  await runExtensionAPITest("multiple extension event eventListeners", {
    backgroundScript({ testLog }) {
      const api = browser.mockExtensionAPI;
      let listenerNoReply;
      let listenerSendResponseReply;

      return new Promise(resolve => {
        testLog("addListener and wait for event to be fired");
        listenerNoReply = () => {
          return false;
        };
        listenerSendResponseReply = (msg, sendResponse) => {
          if (msg === "call-sendResponse") {
            // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
            setTimeout(() => sendResponse("sendResponse-value"), 20);
            return true;
          }

          resolve(msg);
        };
        api.onTestEvent.addListener(listenerNoReply);
        api.onTestEvent.addListener(listenerSendResponseReply);
      });
    },
    assertResults({ testError, testResult }) {
      Assert.deepEqual(testError, null, "Got no error as expected");
      Assert.equal(testResult, "sendResponse-value", "Got expected value");
    },
    mockAPIRequestHandler(policy, request) {
      if (!request.eventListener) {
        throw new Error(
          "Unexpected Error: missing ExtensionAPIRequest.eventListener"
        );
      }

      if (request.requestType === "addListener") {
        this._listeners = this._listeners || [];
        this._listeners.push(request.eventListener);
        if (this._listeners.length === 2) {
          Promise.resolve().then(async () => {
            const { _listeners } = this;
            this._listeners = undefined;

            // Reference to the listener to which we should send the
            // final message to complete the test.
            const replyListener = _listeners[1];

            const res = await Promise.race(
              _listeners.map(l =>
                l.callListener(["call-sendResponse"], {
                  callbackType:
                    Ci.mozIExtensionListenerCallOptions.CALLBACK_SEND_RESPONSE,
                })
              )
            );
            replyListener.callListener([res]);
          });
        }
      }
    },
  });
});

// Unit test nsIServiceWorkerManager.wakeForExtensionAPIEvent method.
add_task(async function test_serviceworkermanager_wake_for_api_event_helper() {
  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      version: "1.0",
      background: {
        service_worker: "sw.js",
      },
      browser_specific_settings: {
        gecko: { id: "test-bg-sw-wakeup@mochi.test" },
      },
    },
    files: {
      "sw.js": `
        dump("Background ServiceWorker - executing\\n");
        const lifecycleEvents = [];
        self.oninstall = () => {
          dump('Background ServiceWorker - oninstall\\n');
          lifecycleEvents.push("install");
        };
        self.onactivate = () => {
          dump('Background ServiceWorker - onactivate\\n');
          lifecycleEvents.push("activate");
        };
        browser.test.onMessage.addListener(msg => {
          if (msg === "bgsw-getSWEvents") {
            browser.test.sendMessage("bgsw-gotSWEvents", lifecycleEvents);
            return;
          }

          browser.test.fail("Got unexpected test message: " + msg);
        });

        const fakeListener01 = () => {};
        const fakeListener02 = () => {};

        // Adding and removing the same listener, and so we expect
        // ExtensionEventWakeupMap to not have any wakeup listener
        // for the runtime.onInstalled event.
        browser.runtime.onInstalled.addListener(fakeListener01);
        browser.runtime.onInstalled.removeListener(fakeListener01);
        // Removing the same listener more than ones should make any
        // difference, and it shouldn't trigger any assertion in
        // debug builds.
        browser.runtime.onInstalled.removeListener(fakeListener01);

        browser.runtime.onStartup.addListener(fakeListener02);
        // Removing an unrelated listener, runtime.onStartup is expected to
        // still have one wakeup listener tracked by ExtensionEventWakeupMap.
        browser.runtime.onStartup.removeListener(fakeListener01);

        browser.test.sendMessage("bgsw-executed");
        dump("Background ServiceWorker - executed\\n");
      `,
    },
  });

  const testWorkerWatcher = new TestWorkerWatcher("../data");
  let watcher = await testWorkerWatcher.watchExtensionServiceWorker(extension);

  await extension.startup();

  info("Wait for the background service worker to be spawned");
  ok(
    await watcher.promiseWorkerSpawned,
    "The extension service worker has been spawned as expected"
  );

  await extension.awaitMessage("bgsw-executed");

  extension.sendMessage("bgsw-getSWEvents");
  let lifecycleEvents = await extension.awaitMessage("bgsw-gotSWEvents");
  Assert.deepEqual(
    lifecycleEvents,
    ["install", "activate"],
    "Got install and activate lifecycle events as expected"
  );

  info("Wait for the background service worker to be terminated");
  ok(
    await watcher.terminate(),
    "The extension service worker has been terminated as expected"
  );

  const swReg = testWorkerWatcher.getRegistration(extension);
  ok(swReg, "Got a service worker registration");
  ok(swReg?.activeWorker, "Got an active worker");

  watcher = await testWorkerWatcher.watchExtensionServiceWorker(extension);

  const extensionBaseURL = extension.extension.baseURI.spec;

  async function testWakeupOnAPIEvent(eventName, expectedResult) {
    const result = await testWorkerWatcher.swm.wakeForExtensionAPIEvent(
      extensionBaseURL,
      "runtime",
      eventName
    );
    equal(
      result,
      expectedResult,
      `Got expected result from wakeForExtensionAPIEvent for ${eventName}`
    );
    info(
      `Wait for the background service worker to be spawned for ${eventName}`
    );
    ok(
      await watcher.promiseWorkerSpawned,
      "The extension service worker has been spawned as expected"
    );
    await extension.awaitMessage("bgsw-executed");
  }

  info("Wake up active worker for API event");
  // Extension API event listener has been added and removed synchronously by
  // the worker script, and so we expect the promise to resolve successfully
  // to `false`.
  await testWakeupOnAPIEvent("onInstalled", false);

  extension.sendMessage("bgsw-getSWEvents");
  lifecycleEvents = await extension.awaitMessage("bgsw-gotSWEvents");
  Assert.deepEqual(
    lifecycleEvents,
    [],
    "No install and activate lifecycle events expected on spawning active worker"
  );

  info("Wait for the background service worker to be terminated");
  ok(
    await watcher.terminate(),
    "The extension service worker has been terminated as expected"
  );

  info("Wakeup again with an API event that has been subscribed");
  // Extension API event listener has been added synchronously (and not removed)
  // by the worker script, and so we expect the promise to resolve successfully
  // to `true`.
  await testWakeupOnAPIEvent("onStartup", true);

  info("Wait for the background service worker to be terminated");
  ok(
    await watcher.terminate(),
    "The extension service worker has been terminated as expected"
  );

  await extension.unload();

  await Assert.rejects(
    testWorkerWatcher.swm.wakeForExtensionAPIEvent(
      extensionBaseURL,
      "runtime",
      "onStartup"
    ),
    /Not an extension principal or extension disabled/,
    "Got the expected rejection on wakeForExtensionAPIEvent called for an uninstalled extension"
  );
});
