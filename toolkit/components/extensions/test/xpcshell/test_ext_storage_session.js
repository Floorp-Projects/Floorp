"use strict";

AddonTestUtils.init(this);

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

add_setup(async function setup() {
  await ExtensionTestUtils.startAddonManager();
});

add_task(async function test_storage_session() {
  await test_background_page_storage("session");
});

add_task(async function test_storage_session_onChanged_event_page() {
  await test_storage_change_event_page("session");
});

add_task(async function test_storage_session_persistance() {
  await test_storage_after_reload("session", { expectPersistency: false });
});

add_task(async function test_storage_session_empty_events() {
  await test_storage_empty_events("session");
});

add_task(async function test_storage_session_contentscript() {
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/data/file_sample.html"
  );

  let extensionData = {
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/data/file_sample.html"],
          js: ["content_script.js"],
        },
      ],
      permissions: ["storage"],
    },
    files: {
      "content_script.js"() {
        browser.test.assertEq(
          typeof browser.storage.session,
          "undefined",
          "Expect storage.session to not be available in content scripts"
        );
        browser.test.sendMessage("done");
      },
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  await extension.awaitMessage("done");

  await extension.unload();
  await contentPage.close();
});
