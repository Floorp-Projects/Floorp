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

add_task(async function test_method_return_runtime_port() {
  await runExtensionAPITest("API method returns an ExtensionPort instance", {
    backgroundScript({ testAsserts }) {
      try {
        browser.mockExtensionAPI.methodReturnsPort("port-create-error");
        throw new Error("methodReturnsPort should have raised an exception");
      } catch (err) {
        testAsserts.equal(
          err?.message,
          "An unexpected error occurred",
          "Got the expected error"
        );
      }
      const port = browser.mockExtensionAPI.methodReturnsPort(
        "port-create-success"
      );
      testAsserts.equal(!!port, true, "Got a port");
      testAsserts.equal(
        typeof port.name,
        "string",
        "port.name should be a string"
      );
      testAsserts.equal(
        typeof port.sender,
        "object",
        "port.sender should be an object"
      );
      testAsserts.equal(
        typeof port.disconnect,
        "function",
        "port.disconnect method"
      );
      testAsserts.equal(
        typeof port.postMessage,
        "function",
        "port.postMessage method"
      );
      testAsserts.equal(
        typeof port.onDisconnect?.addListener,
        "function",
        "port.onDisconnect.addListener method"
      );
      testAsserts.equal(
        typeof port.onMessage?.addListener,
        "function",
        "port.onDisconnect.addListener method"
      );
      return new Promise(resolve => {
        let messages = [];
        port.onDisconnect.addListener(() => resolve(messages));
        port.onMessage.addListener((...args) => {
          messages.push(args);
        });
      });
    },
    assertResults({ testError, testResult }) {
      Assert.deepEqual(testError, null, "Got no error as expected");
      Assert.deepEqual(
        testResult,
        [
          [1, 2],
          [3, 4],
          [5, 6],
        ],
        "Got the expected results"
      );
    },
    mockAPIRequestHandler(policy, request) {
      if (request.apiName == "methodReturnsPort") {
        if (request.args[0] == "port-create-error") {
          return {
            type: Ci.mozIExtensionAPIRequestResult.RETURN_VALUE,
            value: "not-a-valid-port",
          };
        }
        return {
          type: Ci.mozIExtensionAPIRequestResult.RETURN_VALUE,
          value: {
            portId: "port-id-1",
            name: "a-port-name",
          },
        };
      } else if (request.requestType == "addListener") {
        if (request.apiObjectType !== "Port") {
          throw new Error(`Unexpected objectType ${request}`);
        }

        switch (request.apiName) {
          case "onDisconnect":
            this._onDisconnectCb = request.eventListener;
            return;
          case "onMessage":
            Promise.resolve().then(async () => {
              await request.eventListener.callListener([1, 2]);
              await request.eventListener.callListener([3, 4]);
              await request.eventListener.callListener([5, 6]);
              this._onDisconnectCb.callListener([]);
            });
            return;
        }
      } else if (
        request.requestType == "getProperty" &&
        request.apiObjectType == "Port" &&
        request.apiName == "sender"
      ) {
        return {
          type: Ci.mozIExtensionAPIRequestResult.RETURN_VALUE,
          value: { id: "fake-sender-id-prop" },
        };
      }

      throw new Error(`Unexpected request: ${request}`);
    },
  });
});

add_task(async function test_port_as_event_listener_eventListener_param() {
  await runExtensionAPITest(
    "API event eventListener received an ExtensionPort parameter",
    {
      backgroundScript({ testAsserts, testLog }) {
        const api = browser.mockExtensionAPI;
        let listener;

        return new Promise((resolve, reject) => {
          testLog("addListener and wait for event to be fired");
          listener = port => {
            try {
              testAsserts.equal(!!port, true, "Got a port parameter");
              testAsserts.equal(
                port.name,
                "a-port-name-2",
                "Got expected port.name value"
              );
              testAsserts.equal(
                typeof port.disconnect,
                "function",
                "port.disconnect method"
              );
              testAsserts.equal(
                typeof port.postMessage,
                "function",
                "port.disconnect method"
              );
              port.onMessage.addListener((msg, portArg) => {
                if (msg === "test-done") {
                  testLog("Got a port.onMessage event");
                  testAsserts.equal(
                    portArg?.name,
                    "a-port-name-2",
                    "Got port as last argument"
                  );
                  testAsserts.equal(
                    portArg === port,
                    true,
                    "Got the same port instance as expected"
                  );
                  resolve();
                } else {
                  reject(
                    new Error(
                      `port.onMessage got an unexpected message: ${msg}`
                    )
                  );
                }
              });
            } catch (err) {
              reject(err);
            }
          };
          api.onTestEvent.addListener(listener);
        });
      },
      assertResults({ testError }) {
        Assert.deepEqual(testError, null, "Got no error as expected");
      },
      mockAPIRequestHandler(policy, request) {
        if (
          request.requestType == "addListener" &&
          request.apiName == "onTestEvent"
        ) {
          request.eventListener.callListener(["arg0", "arg1"], {
            apiObjectType: Ci.mozIExtensionListenerCallOptions.RUNTIME_PORT,
            apiObjectDescriptor: { portId: "port-id-2", name: "a-port-name-2" },
            apiObjectPrepended: true,
          });
          return;
        } else if (
          request.requestType == "addListener" &&
          request.apiObjectType == "Port" &&
          request.apiObjectId == "port-id-2"
        ) {
          request.eventListener.callListener(["test-done"], {
            apiObjectType: Ci.mozIExtensionListenerCallOptions.RUNTIME_PORT,
            apiObjectDescriptor: { portId: "port-id-2", name: "a-port-name-2" },
          });
          return;
        }

        throw new Error(`Unexpected request: ${request}`);
      },
    }
  );
});
