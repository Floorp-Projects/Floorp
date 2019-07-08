"use strict";

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

add_task(async function test_xhr_capabilities() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      let xhr = new XMLHttpRequest();
      xhr.open("GET", browser.extension.getURL("bad.xml"));

      browser.test.sendMessage("result", {
        name: "Background script XHRs should not be privileged",
        result: xhr.channel === undefined,
      });

      xhr.onload = () => {
        browser.test.sendMessage("result", {
          name: "Background script XHRs should not yield <parsererrors>",
          result: xhr.responseXML === null,
        });
      };
      xhr.send();
    },

    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/data/file_sample.html"],
          js: ["content_script.js"],
        },
      ],
      web_accessible_resources: ["bad.xml"],
    },

    files: {
      "bad.xml": "<xml",
      "content_script.js"() {
        let xhr = new XMLHttpRequest();
        xhr.open("GET", browser.extension.getURL("bad.xml"));

        browser.test.sendMessage("result", {
          name: "Content script XHRs should not be privileged",
          result: xhr.channel === undefined,
        });

        xhr.onload = () => {
          browser.test.sendMessage("result", {
            name: "Content script XHRs should not yield <parsererrors>",
            result: xhr.responseXML === null,
          });
        };
        xhr.send();
      },
    },
  });

  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/data/file_sample.html"
  );

  // We expect four test results from the content/background scripts.
  for (let i = 0; i < 4; ++i) {
    let result = await extension.awaitMessage("result");
    ok(result.result, result.name);
  }

  await contentPage.close();
  await extension.unload();
});
