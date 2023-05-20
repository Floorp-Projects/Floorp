"use strict";

const server = createHttpServer({ hosts: ["example.com"] });
server.registerPathHandler("/dum", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html; charset=utf-8", false);
  response.write("<!DOCTYPE html><html></html>");
});

async function testNativeMessaging({
  isPrivileged = false,
  permissions,
  testBackground,
  testContent,
}) {
  async function runTest(testFn, completionMessage) {
    try {
      dump(`Running test before sending ${completionMessage}\n`);
      await testFn();
    } catch (e) {
      browser.test.fail(`Unexpected error: ${e}`);
    }
    browser.test.sendMessage(completionMessage);
  }
  const extension = ExtensionTestUtils.loadExtension({
    isPrivileged,
    background: `(${runTest})(${testBackground}, "background_done");`,
    manifest: {
      content_scripts: [
        {
          run_at: "document_end",
          js: ["test.js"],
          matches: ["http://example.com/dummy"],
        },
      ],
      permissions,
    },
    files: {
      "test.js": `(${runTest})(${testContent}, "content_done");`,
    },
  });

  // Run background script.
  await extension.startup();
  await extension.awaitMessage("background_done");

  // Run content script.
  const page = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy"
  );
  await extension.awaitMessage("content_done");
  await page.close();

  await extension.unload();
}

// Checks that unprivileged extensions cannot use any of the nativeMessaging
// APIs on Android.
add_task(async function test_nativeMessaging_unprivileged() {
  function testScript() {
    browser.test.assertEq(
      browser.runtime.connectNative,
      undefined,
      "connectNative should not be available in unprivileged extensions"
    );
    browser.test.assertEq(
      browser.runtime.sendNativeMessage,
      undefined,
      "sendNativeMessage should not be available in unprivileged extensions"
    );
  }

  const { messages } = await AddonTestUtils.promiseConsoleOutput(async () => {
    await testNativeMessaging({
      isPrivileged: false,
      permissions: [
        "geckoViewAddons",
        "nativeMessaging",
        "nativeMessagingFromContent",
      ],
      testBackground: testScript,
      testContent: testScript,
    });
  });
  AddonTestUtils.checkMessages(messages, {
    expected: [
      { message: /Invalid extension permission: geckoViewAddons/ },
      { message: /Invalid extension permission: nativeMessaging/ },
      { message: /Invalid extension permission: nativeMessagingFromContent/ },
    ],
  });
});

// Checks that privileged extensions can still not use native messaging without
// the geckoViewAddons permission.
add_task(async function test_geckoViewAddons_missing() {
  const ERROR_NATIVE_MESSAGE_FROM_BACKGROUND =
    "Native manifests are not supported on android";
  const ERROR_NATIVE_MESSAGE_FROM_CONTENT =
    /^Native messaging not allowed: \{.*"envType":"content_child","url":"http:\/\/example\.com\/dummy"\}$/;

  async function testBackground() {
    await browser.test.assertRejects(
      browser.runtime.sendNativeMessage("dummy_nativeApp", "DummyMsg"),
      // Redacted error: ERROR_NATIVE_MESSAGE_FROM_BACKGROUND
      "An unexpected error occurred",
      "Background script cannot use nativeMessaging without geckoViewAddons"
    );
  }
  async function testContent() {
    await browser.test.assertRejects(
      browser.runtime.sendNativeMessage("dummy_nativeApp", "DummyMsg"),
      // Redacted error: ERROR_NATIVE_MESSAGE_FROM_CONTENT
      "An unexpected error occurred",
      "Content script cannot use nativeMessaging without geckoViewAddons"
    );
  }

  const { messages } = await AddonTestUtils.promiseConsoleOutput(async () => {
    await testNativeMessaging({
      isPrivileged: true,
      permissions: ["nativeMessaging", "nativeMessagingFromContent"],
      testBackground,
      testContent,
    });
  });
  AddonTestUtils.checkMessages(messages, {
    expected: [
      { errorMessage: ERROR_NATIVE_MESSAGE_FROM_BACKGROUND },
      { errorMessage: ERROR_NATIVE_MESSAGE_FROM_CONTENT },
    ],
  });
});

// Checks that privileged extensions cannot use native messaging from content
// without the nativeMessagingFromContent permission.
add_task(async function test_nativeMessagingFromContent_missing() {
  const ERROR_NATIVE_MESSAGE_FROM_CONTENT_NO_PERM =
    /^Unexpected messaging sender: \{.*"envType":"content_child","url":"http:\/\/example\.com\/dummy"\}$/;
  function testBackground() {
    // sendNativeMessage / connectNative are expected to succeed, but we
    // are not testing that here because XpcshellTestRunnerService does not
    // have a WebExtension.MessageDelegate that handles the message.
    // There are plenty of mochitests that rely on connectNative, so we are
    // not testing that here.
  }
  async function testContent() {
    await browser.test.assertRejects(
      browser.runtime.sendNativeMessage("dummy_nativeApp", "DummyMsg"),
      // Redacted error: ERROR_NATIVE_MESSAGE_FROM_CONTENT_NO_PERM
      "An unexpected error occurred",
      "Trying to get through to native messaging but without luck"
    );
  }

  const { messages } = await AddonTestUtils.promiseConsoleOutput(async () => {
    await testNativeMessaging({
      isPrivileged: true,
      permissions: ["geckoViewAddons", "nativeMessaging"],
      testBackground,
      testContent,
    });
  });
  AddonTestUtils.checkMessages(messages, {
    expected: [{ errorMessage: ERROR_NATIVE_MESSAGE_FROM_CONTENT_NO_PERM }],
  });
});
