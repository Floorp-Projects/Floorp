"use strict";

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

function contentScript() {
  window.x = 12;
  browser.test.assertEq(window.x, 12, "x is 12");
  browser.test.notifyPass("background test passed");
}

let extensionData = {
  manifest: {
    content_scripts: [
      {
        matches: ["http://localhost/*/file_sample.html"],
        js: ["content_script.js"],
        run_at: "document_idle",
      },
    ],
  },

  files: {
    "content_script.js": contentScript,
  },
};

add_task(async function test_contentscript() {
  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );

  await extension.awaitFinish();
  await contentPage.close();

  await extension.unload();
});
