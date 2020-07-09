"use strict";

// See: https://bugzilla.mozilla.org/show_bug.cgi?id=1573456
add_task(async function test_mozextension_page_loaded_in_extension_process() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: [
        "webRequest",
        "webRequestBlocking",
        "https://example.com/*",
      ],
      web_accessible_resources: ["test.html"],
    },
    files: {
      "test.html": '<!DOCTYPE html><script src="test.js"></script>',
      "test.js": () => {
        browser.test.assertTrue(
          browser.webRequest,
          "webRequest API should be available"
        );

        browser.test.sendMessage("test_done");
      },
    },
    background: () => {
      browser.webRequest.onBeforeRequest.addListener(
        () => {
          return {
            redirectUrl: browser.runtime.getURL("test.html"),
          };
        },
        { urls: ["*://*/redir"] },
        ["blocking"]
      );
    },
  });
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "https://example.com/redir"
  );

  await extension.awaitMessage("test_done");

  await extension.unload();
  await contentPage.close();
});
