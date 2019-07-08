"use strict";

// ExtensionContent.jsm needs to know when it's running from xpcshell,
// to use the right timeout for content scripts executed at document_idle.
ExtensionTestUtils.mockAppInfo();

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

add_task(async function test_contentscript_xrays() {
  async function contentScript() {
    let unwrapped = window.wrappedJSObject;

    browser.test.assertEq(
      "undefined",
      typeof test,
      "Should not have named X-ray property access"
    );
    browser.test.assertEq(
      undefined,
      window.test,
      "Should not have named X-ray property access"
    );
    browser.test.assertEq(
      "object",
      typeof unwrapped.test,
      "Should always have non-X-ray named property access"
    );

    browser.test.notifyPass("contentScriptXrays");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://*/*/file_sample.html"],
          js: ["content_script.js"],
        },
      ],
    },

    files: {
      "content_script.js": contentScript,
    },
  });

  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );

  await extension.awaitFinish("contentScriptXrays");

  await contentPage.close();
  await extension.unload();
});
