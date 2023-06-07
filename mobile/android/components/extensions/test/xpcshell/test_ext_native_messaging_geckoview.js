"use strict";

const server = createHttpServer({ hosts: ["example.com"] });
server.registerPathHandler("/", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html; charset=utf-8", false);
  response.write("<!DOCTYPE html><html></html>");
});

ChromeUtils.defineESModuleGetters(this, {
  GeckoViewConnection: "resource://gre/modules/GeckoViewWebExtension.sys.mjs",
});

// Save reference to original implementations to restore later.
const { sendMessage, onConnect } = GeckoViewConnection.prototype;
add_setup(async () => {
  // This file replaces the implementation of GeckoViewConnection;
  // make sure that it is restored upon test completion.
  registerCleanupFunction(() => {
    GeckoViewConnection.prototype.sendMessage = sendMessage;
    GeckoViewConnection.prototype.onConnect = onConnect;
  });
});

// Mock the embedder communication port
class EmbedderPort {
  constructor(portId, messenger) {
    this.id = portId;
    this.messenger = messenger;
  }
  close() {
    Assert.ok(false, "close not expected to be called");
  }
  onPortDisconnect() {
    Assert.ok(false, "onPortDisconnect not expected to be called");
  }
  onPortMessage(holder) {
    Assert.ok(false, "onPortMessage not expected to be called");
  }
  triggerPortDisconnect() {
    this.messenger.sendPortDisconnect(this.id);
  }
}

function stubConnectNative() {
  let port;
  const firstCallPromise = new Promise(resolve => {
    let callCount = 0;
    GeckoViewConnection.prototype.onConnect = (portId, messenger) => {
      Assert.equal(++callCount, 1, "onConnect called once");
      port = new EmbedderPort(portId, messenger);
      resolve();
      return port;
    };
  });
  const triggerPortDisconnect = () => {
    if (!port) {
      Assert.ok(false, "Undefined port, connection must be established first");
    }
    port.triggerPortDisconnect();
  };
  const restore = () => {
    GeckoViewConnection.prototype.onConnect = onConnect;
  };
  return { firstCallPromise, triggerPortDisconnect, restore };
}

function stubSendNativeMessage() {
  let sendResponse;
  const returnPromise = new Promise(resolve => {
    sendResponse = resolve;
  });
  const firstCallPromise = new Promise(resolve => {
    let callCount = 0;
    GeckoViewConnection.prototype.sendMessage = data => {
      Assert.equal(++callCount, 1, "sendMessage called once");
      resolve(data);
      return returnPromise;
    };
  });
  const restore = () => {
    GeckoViewConnection.prototype.sendMessage = sendMessage;
  };
  return { firstCallPromise, sendResponse, restore };
}

function promiseExtensionEvent(wrapper, event) {
  return new Promise(resolve => {
    wrapper.extension.once(event, (...args) => resolve(args));
  });
}

// verify that when background sends a native message,
// the background will not be terminated to allow native messaging
add_task(async function test_sendNativeMessage_event_page() {
  const extension = ExtensionTestUtils.loadExtension({
    isPrivileged: true,
    manifest: {
      permissions: ["geckoViewAddons", "nativeMessaging"],
      background: { persistent: false },
    },
    async background() {
      const res = await browser.runtime.sendNativeMessage("fake", "msg");
      browser.test.assertEq("myResp", res, "expected response");
      browser.test.sendMessage("done");
      browser.runtime.onSuspend.addListener(async () => {
        browser.test.assertFail("unexpected onSuspend");
      });
    },
  });

  const stub = stubSendNativeMessage();
  await extension.startup();
  info("Wait for sendNativeMessage to be received");
  Assert.equal(
    (await stub.firstCallPromise).deserialize({}),
    "msg",
    "expected message"
  );

  info("Trigger background script idle timeout and expect to be reset");
  const promiseResetIdle = promiseExtensionEvent(
    extension,
    "background-script-reset-idle"
  );
  await extension.terminateBackground();
  info("Wait for 'background-script-reset-idle' event to be emitted");
  await promiseResetIdle;

  stub.sendResponse("myResp");

  info("Wait for extension to verify sendNativeMessage response");
  await extension.awaitMessage("done");
  await extension.unload();

  stub.restore();
});

// verify that when an extension tab sends a native message,
// the background will terminate as expected
add_task(async function test_sendNativeMessage_tab() {
  const extension = ExtensionTestUtils.loadExtension({
    isPrivileged: true,
    manifest: {
      permissions: ["geckoViewAddons", "nativeMessaging"],
      background: { persistent: false },
    },
    async background() {
      browser.runtime.onSuspend.addListener(async () => {
        browser.test.sendMessage("onSuspend_called");
      });
    },
    files: {
      "tab.html": `
        <!DOCTYPE html><meta charset="utf-8">
        <script src="tab.js"></script>
      `,
      "tab.js": async () => {
        const res = await browser.runtime.sendNativeMessage("fake", "msg");
        browser.test.assertEq("myResp", res, "expected response");
        browser.test.sendMessage("content_done");
      },
    },
  });

  const stub = stubSendNativeMessage();
  await extension.startup();

  const tab = await ExtensionTestUtils.loadContentPage(
    `moz-extension://${extension.uuid}/tab.html?tab`,
    { extension }
  );

  info("Wait for sendNativeMessage to be received");
  Assert.equal(
    (await stub.firstCallPromise).deserialize({}),
    "msg",
    "expected message"
  );

  info("Terminate extension");
  await extension.terminateBackground();
  await extension.awaitMessage("onSuspend_called");

  stub.sendResponse("myResp");

  info("Wait for extension to verify sendNativeMessage response");
  await extension.awaitMessage("content_done");
  await tab.close();
  await extension.unload();

  stub.restore();
});

// verify that when a content script sends a native message,
// the background will terminate as expected
add_task(async function test_sendNativeMessage_content_script() {
  const extension = ExtensionTestUtils.loadExtension({
    isPrivileged: true,
    manifest: {
      permissions: [
        "geckoViewAddons",
        "nativeMessaging",
        "nativeMessagingFromContent",
      ],
      background: { persistent: false },
      content_scripts: [
        {
          run_at: "document_end",
          js: ["test.js"],
          matches: ["http://example.com/"],
        },
      ],
    },
    files: {
      "test.js": async () => {
        const res = await browser.runtime.sendNativeMessage("fake", "msg");
        browser.test.assertEq("myResp", res, "expected response");
        browser.test.sendMessage("content_done");
      },
    },
    async background() {
      browser.runtime.onSuspend.addListener(async () => {
        browser.test.sendMessage("onSuspend_called");
      });
    },
  });

  const stub = stubSendNativeMessage();
  await extension.startup();

  info("Load content page");
  const page = await ExtensionTestUtils.loadContentPage("http://example.com/");

  info("Wait for message from extension");
  Assert.equal(
    (await stub.firstCallPromise).deserialize({}),
    "msg",
    "expected message"
  );

  info("Terminate extension");
  await extension.terminateBackground();
  await extension.awaitMessage("onSuspend_called");

  stub.sendResponse("myResp");

  info("Wait for extension to verify sendNativeMessage response");
  await extension.awaitMessage("content_done");
  await page.close();
  await extension.unload();

  stub.restore();
});

// verify that when native messaging ports are open, the background will not be terminated
// and once the ports disconnect, onSuspend can be called
add_task(async function test_connectNative_event_page() {
  const extension = ExtensionTestUtils.loadExtension({
    isPrivileged: true,
    manifest: {
      permissions: ["geckoViewAddons", "nativeMessaging"],
      background: { persistent: false },
    },
    async background() {
      const port = browser.runtime.connectNative("test");
      port.onDisconnect.addListener(() => {
        browser.test.assertEq(
          null,
          port.error,
          "port should be disconnected without errors"
        );
        browser.test.sendMessage("port_disconnected");
      });

      browser.runtime.onSuspend.addListener(async () => {
        browser.test.sendMessage("onSuspend_called");
      });
    },
  });

  const stub = stubConnectNative();
  await extension.startup();
  info("Waiting for connectNative request");
  await stub.firstCallPromise;

  info("Trigger background script idle timeout and expect to be reset");
  const promiseResetIdle = promiseExtensionEvent(
    extension,
    "background-script-reset-idle"
  );

  await extension.terminateBackground();
  info("Wait for 'background-script-reset-idle' event to be emitted");
  await promiseResetIdle;

  info("Trigger port disconnect, terminate background, and expect onSuspend()");
  stub.triggerPortDisconnect();
  await extension.awaitMessage("port_disconnected");

  info("Terminate extension");
  await extension.terminateBackground();
  await extension.awaitMessage("onSuspend_called");

  await extension.unload();
  stub.restore();
});

// verify that when an extension tab opens native messaging ports,
// the background will terminate as expected
add_task(async function test_connectNative_tab() {
  const extension = ExtensionTestUtils.loadExtension({
    isPrivileged: true,
    manifest: {
      permissions: ["geckoViewAddons", "nativeMessaging"],
      background: { persistent: false },
    },
    async background() {
      browser.runtime.onSuspend.addListener(async () => {
        browser.test.sendMessage("onSuspend_called");
      });
    },
    files: {
      "tab.html": `
        <!DOCTYPE html><meta charset="utf-8">
        <script src="tab.js"></script>
      `,
      "tab.js": async () => {
        const port = browser.runtime.connectNative("test");
        port.onDisconnect.addListener(() => {
          browser.test.assertEq(
            null,
            port.error,
            "port should be disconnected without errors"
          );
          browser.test.sendMessage("port_disconnected");
        });
        browser.test.sendMessage("content_done");
      },
    },
  });

  const stub = stubConnectNative();
  await extension.startup();

  const tab = await ExtensionTestUtils.loadContentPage(
    `moz-extension://${extension.uuid}/tab.html?tab`,
    { extension }
  );
  await extension.awaitMessage("content_done");
  await stub.firstCallPromise;

  info("Terminate extension");
  await extension.terminateBackground();
  await extension.awaitMessage("onSuspend_called");

  stub.triggerPortDisconnect();
  await extension.awaitMessage("port_disconnected");
  await tab.close();
  await extension.unload();

  stub.restore();
});

// verify that when a content script opens native messaging ports,
// the background will terminate as expected
add_task(async function test_connectNative_content_script() {
  const extension = ExtensionTestUtils.loadExtension({
    isPrivileged: true,
    manifest: {
      permissions: [
        "geckoViewAddons",
        "nativeMessaging",
        "nativeMessagingFromContent",
      ],
      background: { persistent: false },
      content_scripts: [
        {
          run_at: "document_end",
          js: ["test.js"],
          matches: ["http://example.com/"],
        },
      ],
    },
    files: {
      "test.js": async () => {
        const port = browser.runtime.connectNative("test");
        port.onDisconnect.addListener(() => {
          browser.test.assertEq(
            null,
            port.error,
            "port should be disconnected without errors"
          );
          browser.test.sendMessage("port_disconnected");
        });
        browser.test.sendMessage("content_done");
      },
    },
    async background() {
      browser.runtime.onSuspend.addListener(async () => {
        browser.test.sendMessage("onSuspend_called");
      });
    },
  });

  const stub = stubConnectNative();
  await extension.startup();

  info("Load content page");
  const page = await ExtensionTestUtils.loadContentPage("http://example.com/");
  await extension.awaitMessage("content_done");
  await stub.firstCallPromise;

  info("Terminate extension");
  await extension.terminateBackground();
  await extension.awaitMessage("onSuspend_called");

  stub.triggerPortDisconnect();
  await extension.awaitMessage("port_disconnected");
  await page.close();
  await extension.unload();

  stub.restore();
});
