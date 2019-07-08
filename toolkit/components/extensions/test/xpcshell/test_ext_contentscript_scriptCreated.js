"use strict";

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

// ExtensionContent.jsm needs to know when it's running from xpcshell,
// to use the right timeout for content scripts executed at document_idle.
ExtensionTestUtils.mockAppInfo();

// Test that document_start content scripts don't block script-created
// parsers.
add_task(async function test_contentscript_scriptCreated() {
  let extensionData = {
    manifest: {
      content_scripts: [
        {
          matches: ["http://*/*/file_document_write.html"],
          js: ["content_script.js"],
          run_at: "document_start",
          match_about_blank: true,
          all_frames: true,
        },
      ],
    },

    files: {
      "content_script.js": function() {
        if (window === top) {
          addEventListener(
            "message",
            msg => {
              browser.test.assertEq(
                "ok",
                msg.data,
                "document.write() succeeded"
              );
              browser.test.sendMessage("content-script-done");
            },
            { once: true }
          );
        }
      },
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);

  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_document_write.html`
  );

  await extension.awaitMessage("content-script-done");

  await contentPage.close();

  await extension.unload();
});
