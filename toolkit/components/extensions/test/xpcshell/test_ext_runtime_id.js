"use strict";

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

add_task(async function test_runtime_id() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/data/file_sample.html"],
          js: ["content_script.js"],
        },
      ],
    },

    background() {
      browser.test.sendMessage("background-id", browser.runtime.id);
    },

    files: {
      "content_script.js"() {
        browser.test.sendMessage("content-id", browser.runtime.id);
      },
    },
  });

  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/data/file_sample.html"
  );

  let backgroundId = await extension.awaitMessage("background-id");
  equal(
    backgroundId,
    extension.id,
    "runtime.id from background script is correct"
  );

  let contentId = await extension.awaitMessage("content-id");
  equal(contentId, extension.id, "runtime.id from content script is correct");

  await contentPage.close();
  await extension.unload();
});
