"use strict";

/* globals browser */

Cu.import("resource://gre/modules/Extension.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const {LegacyExtensionContext} = Cu.import("resource://gre/modules/LegacyExtensionsUtils.jsm", {});

/**
 * This test case ensures that LegacyExtensionContext instances:
 *  - expose the expected API object and can join the messaging
 *    of a webextension given its addon id
 *  - the exposed API object can receive a port related to a `runtime.connect`
 *    Port created in the webextension's background page
 *  - the received Port instance can exchange messages with the background page
 *  - the received Port receive a disconnect event when the webextension is
 *    shutting down
 */
add_task(async function test_legacy_extension_context() {
  function background() {
    let bgURL = window.location.href;

    let extensionInfo = {
      bgURL,
      // Extract the assigned uuid from the background page url.
      uuid: window.location.hostname,
    };

    browser.test.sendMessage("webextension-ready", extensionInfo);

    let port;

    browser.test.onMessage.addListener(async msg => {
      if (msg == "do-send-message") {
        let reply = await browser.runtime.sendMessage("webextension -> legacy_extension message");

        browser.test.assertEq("legacy_extension -> webextension reply", reply,
                              "Got the expected message from the LegacyExtensionContext");
        browser.test.sendMessage("got-reply-message");
      } else if (msg == "do-connect") {
        port = browser.runtime.connect();

        port.onMessage.addListener(portMsg => {
          browser.test.assertEq("legacy_extension -> webextension port message", portMsg,
                                "Got the expected message from the LegacyExtensionContext");
          port.postMessage("webextension -> legacy_extension port message");
        });
      } else if (msg == "do-disconnect") {
        port.disconnect();
      }
    });
  }

  let extensionData = {
    background,
  };

  let extension = Extension.generate(extensionData);

  let waitForExtensionInfo = new Promise((resolve, reject) => {
    extension.on("test-message", function testMessageListener(kind, msg, ...args) {
      if (msg != "webextension-ready") {
        reject(new Error(`Got an unexpected test-message: ${msg}`));
      } else {
        extension.off("test-message", testMessageListener);
        resolve(args[0]);
      }
    });
  });

  // Connect to the target extension as an external context
  // using the given custom sender info.
  let legacyContext;

  extension.on("startup", function onStartup() {
    extension.off("startup", onStartup);
    legacyContext = new LegacyExtensionContext(extension);
    extension.callOnClose({
      close: () => legacyContext.unload(),
    });
  });

  await extension.startup();

  let extensionInfo = await waitForExtensionInfo;

  equal(legacyContext.envType, "legacy_extension",
     "LegacyExtensionContext instance has the expected type");

  ok(legacyContext.api, "Got the expected API object");
  ok(legacyContext.api.browser, "Got the expected browser property");

  let waitMessage = new Promise(resolve => {
    const {browser} = legacyContext.api;
    browser.runtime.onMessage.addListener((singleMsg, msgSender) => {
      resolve({singleMsg, msgSender});

      // Send a reply to the sender.
      return Promise.resolve("legacy_extension -> webextension reply");
    });
  });

  extension.testMessage("do-send-message");

  let {singleMsg, msgSender} = await waitMessage;
  equal(singleMsg, "webextension -> legacy_extension message",
     "Got the expected message");
  ok(msgSender, "Got a message sender object");

  equal(msgSender.id, extension.id, "The sender has the expected id property");
  equal(msgSender.url, extensionInfo.bgURL, "The sender has the expected url property");

  // Wait confirmation that the reply has been received.
  await new Promise((resolve, reject) => {
    extension.on("test-message", function testMessageListener(kind, msg, ...args) {
      if (msg != "got-reply-message") {
        reject(new Error(`Got an unexpected test-message: ${msg}`));
      } else {
        extension.off("test-message", testMessageListener);
        resolve();
      }
    });
  });

  let waitConnectPort = new Promise(resolve => {
    let {browser} = legacyContext.api;
    browser.runtime.onConnect.addListener(port => {
      resolve(port);
    });
  });

  extension.testMessage("do-connect");

  let port = await waitConnectPort;

  ok(port, "Got the Port API object");
  ok(port.sender, "The port has a sender property");
  equal(port.sender.id, extension.id,
     "The port sender has the expected id property");
  equal(port.sender.url, extensionInfo.bgURL,
     "The port sender has the expected url property");

  let waitPortMessage = new Promise(resolve => {
    port.onMessage.addListener((msg) => {
      resolve(msg);
    });
  });

  port.postMessage("legacy_extension -> webextension port message");

  let msg = await waitPortMessage;

  equal(msg, "webextension -> legacy_extension port message",
     "LegacyExtensionContext received the expected message from the webextension");

  let waitForDisconnect = new Promise(resolve => {
    port.onDisconnect.addListener(resolve);
  });

  extension.testMessage("do-disconnect");

  await waitForDisconnect;

  do_print("Got the disconnect event on unload");

  await extension.shutdown();
});
