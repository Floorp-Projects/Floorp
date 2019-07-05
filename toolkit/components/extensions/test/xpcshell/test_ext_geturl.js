"use strict";

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

add_task(async function test_contentscript() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.runtime.onMessage.addListener(([url1, url2]) => {
        let url3 = browser.runtime.getURL("test_file.html");
        let url4 = browser.extension.getURL("test_file.html");

        browser.test.assertTrue(url1 !== undefined, "url1 defined");

        browser.test.assertTrue(
          url1.startsWith("moz-extension://"),
          "url1 has correct scheme"
        );
        browser.test.assertTrue(
          url1.endsWith("test_file.html"),
          "url1 has correct leaf name"
        );

        browser.test.assertEq(url1, url2, "url2 matches");
        browser.test.assertEq(url1, url3, "url3 matches");
        browser.test.assertEq(url1, url4, "url4 matches");

        browser.test.notifyPass("geturl");
      });
    },

    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/data/file_sample.html"],
          js: ["content_script.js"],
          run_at: "document_idle",
        },
      ],
    },

    files: {
      "content_script.js"() {
        let url1 = browser.runtime.getURL("test_file.html");
        let url2 = browser.extension.getURL("test_file.html");
        browser.runtime.sendMessage([url1, url2]);
      },
    },
  });
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/data/file_sample.html"
  );

  await extension.awaitFinish("geturl");

  await contentPage.close();

  await extension.unload();
});
