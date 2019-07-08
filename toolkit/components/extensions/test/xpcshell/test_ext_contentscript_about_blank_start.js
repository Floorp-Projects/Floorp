"use strict";

const server = createHttpServer({ hosts: ["example.com"] });

server.registerPathHandler("/blank-iframe.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html; charset=utf-8", false);
  response.write("<iframe></iframe>");
});

add_task(async function content_script_at_document_start() {
  let extensionData = {
    manifest: {
      content_scripts: [
        {
          matches: ["<all_urls>"],
          js: ["start.js"],
          run_at: "document_start",
          match_about_blank: true,
        },
      ],
    },

    files: {
      "start.js": function() {
        browser.test.sendMessage("content-script-done");
      },
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(`about:blank`);
  await extension.awaitMessage("content-script-done");
  await contentPage.close();
  await extension.unload();
});

add_task(async function content_style_at_document_start() {
  let extensionData = {
    manifest: {
      content_scripts: [
        {
          matches: ["<all_urls>"],
          css: ["start.css"],
          run_at: "document_start",
          match_about_blank: true,
        },
        {
          matches: ["<all_urls>"],
          js: ["end.js"],
          run_at: "document_end",
          match_about_blank: true,
        },
      ],
    },

    files: {
      "start.css": "body { background: red; }",
      "end.js": function() {
        let style = window.getComputedStyle(document.body);
        browser.test.assertEq(
          "rgb(255, 0, 0)",
          style.backgroundColor,
          "document_start style should have been applied"
        );
        browser.test.sendMessage("content-script-done");
      },
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(`about:blank`);
  await extension.awaitMessage("content-script-done");
  await contentPage.close();
  await extension.unload();
});
