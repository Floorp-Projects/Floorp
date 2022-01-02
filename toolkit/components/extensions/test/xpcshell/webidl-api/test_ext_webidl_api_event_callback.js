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
  // Ensure that the profile-after-change message has been notified,
  // so that ServiceWokerRegistrar is going to be initialized.
  Services.obs.notifyObservers(
    null,
    "profile-after-change",
    "force-serviceworkerrestart-init"
  );
});

add_task(async function test_api_event_manager_methods() {
  await runExtensionAPITest("extension event manager methods", {
    backgroundScript({ testAsserts, testLog }) {
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
    assertResults({ testError, testResult }) {
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
      backgroundScript({ testAsserts, testLog }) {
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
      backgroundScript({ testAsserts, testLog }) {
        const api = browser.mockExtensionAPI;
        let listener;

        return new Promise((resolve, reject) => {
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
      backgroundScript({ testAsserts, testLog }) {
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
      assertResults({ testError, testResult }) {
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
      backgroundScript({ testAsserts, testLog }) {
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
    backgroundScript({ testAsserts, testLog }) {
      const api = browser.mockExtensionAPI;
      let listenerNoReply;
      let listenerSendResponseReply;

      return new Promise(resolve => {
        testLog("addListener and wait for event to be fired");
        listenerNoReply = (msg, sendResponse) => {
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
