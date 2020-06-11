"use strict";

const BASE = "http://example.com/data/";

const server = createHttpServer({ hosts: ["example.com"] });

server.registerDirectory("/data/", do_get_file("data"));

add_task(async function test_stylesheet_cache() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "<all_urls>"],
    },
    background() {
      const SHEET_URI = "http://example.com/data/file_stylesheet_cache.css";
      let firstFound = false;
      browser.webRequest.onBeforeRequest.addListener(
        details => {
          browser.test.assertEq(
            details.url,
            firstFound ? SHEET_URI + "?2" : SHEET_URI
          );
          firstFound = true;
          browser.test.sendMessage("stylesheet found");
        },
        { urls: ["<all_urls>"], types: ["stylesheet"] },
        ["blocking"]
      );
    },
  });

  await extension.startup();

  let cp = await ExtensionTestUtils.loadContentPage(
    BASE + "file_stylesheet_cache.html"
  );

  await extension.awaitMessage("stylesheet found");

  // Need to use the same ContentPage so that the remote process the page ends
  // up in is the same.
  await cp.loadURL(BASE + "file_stylesheet_cache_2.html");

  await extension.awaitMessage("stylesheet found");

  await cp.close();

  await extension.unload();
});
