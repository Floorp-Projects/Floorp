"use strict";

const server = createHttpServer({ hosts: ["example.com"] });

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.write("<!DOCTYPE html><html></html>");
});

add_task(async function test_content_script_css() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/dummy"],
          css: ["content.css"],
          run_at: "document_start",
        },
      ],
    },

    files: {
      "content.css": "body { max-width: 42px; }",
    },
  });

  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy"
  );

  function task() {
    let style = this.content.getComputedStyle(this.content.document.body);
    return style.maxWidth;
  }

  let maxWidth = await contentPage.spawn(null, task);
  equal(maxWidth, "42px", "Stylesheet correctly applied");

  await extension.unload();

  maxWidth = await contentPage.spawn(null, task);
  equal(maxWidth, "none", "Stylesheet correctly removed");

  await contentPage.close();
});
