"use strict";

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.write("<!DOCTYPE html><html></html>");
});

// Background and content script for testSendMessage_*
function sendMessage_background(delayedNotifyPass) {
  browser.runtime.onMessage.addListener((msg, sender, sendResponse) => {
    browser.test.assertEq("from frame", msg, "Expected message from frame");
    sendResponse("msg from back"); // Should not throw or anything like that.
    delayedNotifyPass("Received sendMessage from closing frame");
  });
}
function sendMessage_contentScript(testType) {
  browser.runtime.sendMessage("from frame", reply => {
    // The frame has been removed, so we should not get this callback!
    browser.test.fail(`Unexpected reply: ${reply}`);
  });
  if (testType == "frame") {
    frameElement.remove();
  } else {
    browser.test.sendMessage("close-window");
  }
}

// Background and content script for testConnect_*
function connect_background(delayedNotifyPass) {
  browser.runtime.onConnect.addListener(port => {
    browser.test.assertEq("port from frame", port.name);

    let disconnected = false;
    let hasMessage = false;
    port.onDisconnect.addListener(() => {
      browser.test.assertFalse(disconnected, "onDisconnect should fire once");
      disconnected = true;
      browser.test.assertTrue(
        hasMessage,
        "Expected onMessage before onDisconnect"
      );
      browser.test.assertEq(
        null,
        port.error,
        "The port is implicitly closed without errors when the other context unloads"
      );
      delayedNotifyPass("Received onDisconnect from closing frame");
    });
    port.onMessage.addListener(msg => {
      browser.test.assertFalse(hasMessage, "onMessage should fire once");
      hasMessage = true;
      browser.test.assertFalse(
        disconnected,
        "Should get message before disconnect"
      );
      browser.test.assertEq("from frame", msg, "Expected message from frame");
    });

    port.postMessage("reply to closing frame");
  });
}
function connect_contentScript(testType) {
  let isUnloading = false;
  addEventListener(
    "pagehide",
    () => {
      isUnloading = true;
    },
    { once: true }
  );

  let port = browser.runtime.connect({ name: "port from frame" });
  port.onMessage.addListener(msg => {
    // The background page sends a reply as soon as we call runtime.connect().
    // It is possible that the reply reaches this frame before the
    // window.close() request has been processed.
    if (!isUnloading) {
      browser.test.log(
        `Ignorting unexpected reply ("${msg}") because the page is not being unloaded.`
      );
      return;
    }

    // The frame has been removed, so we should not get a reply.
    browser.test.fail(`Unexpected reply: ${msg}`);
  });
  port.postMessage("from frame");

  // Removing the frame or window should disconnect the port.
  if (testType == "frame") {
    frameElement.remove();
  } else {
    browser.test.sendMessage("close-window");
  }
}

// `testType` is "window" or "frame".
function createTestExtension(testType, backgroundScript, contentScript) {
  // Make a roundtrip between the background page and the test runner (which is
  // in the same process as the content script) to make sure that we record a
  // failure in case the content script's sendMessage or onMessage handlers are
  // called even after the frame or window was removed.
  function delayedNotifyPass(msg) {
    browser.test.onMessage.addListener((type, echoMsg) => {
      if (type == "pong") {
        browser.test.assertEq(msg, echoMsg, "Echoed reply should be the same");
        browser.test.notifyPass(msg);
      }
    });
    browser.test.log("Starting ping-pong to flush messages...");
    browser.test.sendMessage("ping", msg);
  }
  let extension = ExtensionTestUtils.loadExtension({
    background: `(${backgroundScript})(${delayedNotifyPass});`,
    manifest: {
      content_scripts: [
        {
          js: ["contentscript.js"],
          all_frames: testType == "frame",
          matches: ["http://example.com/data/file_sample.html"],
        },
      ],
    },
    files: {
      "contentscript.js": `(${contentScript})("${testType}");`,
    },
  });
  extension.awaitMessage("ping").then(msg => {
    extension.sendMessage("pong", msg);
  });
  return extension;
}

add_task(async function testSendMessage_and_remove_frame() {
  let extension = createTestExtension(
    "frame",
    sendMessage_background,
    sendMessage_contentScript
  );
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy"
  );

  await contentPage.spawn(null, () => {
    let { document } = this.content;
    let frame = document.createElement("iframe");
    frame.src = "/data/file_sample.html";
    document.body.appendChild(frame);
  });

  await extension.awaitFinish("Received sendMessage from closing frame");
  await contentPage.close();
  await extension.unload();
});

add_task(async function testConnect_and_remove_frame() {
  let extension = createTestExtension(
    "frame",
    connect_background,
    connect_contentScript
  );
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy"
  );

  await contentPage.spawn(null, () => {
    let { document } = this.content;
    let frame = document.createElement("iframe");
    frame.src = "/data/file_sample.html";
    document.body.appendChild(frame);
  });

  await extension.awaitFinish("Received onDisconnect from closing frame");
  await contentPage.close();
  await extension.unload();
});

add_task(async function testSendMessage_and_remove_window() {
  if (AppConstants.MOZ_BUILD_APP !== "browser") {
    // We can't rely on this timing on Android.
    return;
  }

  let extension = createTestExtension(
    "window",
    sendMessage_background,
    sendMessage_contentScript
  );
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/data/file_sample.html"
  );
  await extension.awaitMessage("close-window");
  await contentPage.close();

  await extension.awaitFinish("Received sendMessage from closing frame");
  await extension.unload();
});

add_task(async function testConnect_and_remove_window() {
  if (AppConstants.MOZ_BUILD_APP !== "browser") {
    // We can't rely on this timing on Android.
    return;
  }

  let extension = createTestExtension(
    "window",
    connect_background,
    connect_contentScript
  );
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/data/file_sample.html"
  );
  await extension.awaitMessage("close-window");
  await contentPage.close();

  await extension.awaitFinish("Received onDisconnect from closing frame");
  await extension.unload();
});
