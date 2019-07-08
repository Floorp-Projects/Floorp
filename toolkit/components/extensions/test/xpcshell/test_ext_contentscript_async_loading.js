"use strict";

const server = createHttpServer({ hosts: ["example.com"] });

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.write("<!DOCTYPE html><html></html>");
});

add_task(async function test_async_loading() {
  const adder = `(function add(a = 1) { this.count += a; })();\n`;

  const extension = {
    manifest: {
      content_scripts: [
        {
          run_at: "document_start",
          matches: ["http://example.com/dummy"],
          js: ["first.js", "second.js"],
        },
        {
          run_at: "document_end",
          matches: ["http://example.com/dummy"],
          js: ["third.js"],
        },
      ],
    },
    files: {
      "first.js": `
        this.count = 0;
        ${adder.repeat(50000)};  // 2Mb
        browser.test.assertEq(this.count, 50000, "A 50k line script");

        this.order = (this.order || 0) + 1;
        browser.test.sendMessage("first", this.order);
      `,
      "second.js": `
        this.order = (this.order || 0) + 1;
        browser.test.sendMessage("second", this.order);
      `,
      "third.js": `
        this.order = (this.order || 0) + 1;
        browser.test.sendMessage("third", this.order);
      `,
    },
  };

  async function checkOrder(ext) {
    const [first, second, third] = await Promise.all([
      ext.awaitMessage("first"),
      ext.awaitMessage("second"),
      ext.awaitMessage("third"),
    ]);

    equal(first, 1, "first.js finished execution first.");
    equal(second, 2, "second.js finished execution second.");
    equal(third, 3, "third.js finished execution third.");
  }

  info("Test pages observed while extension is running");
  const observed = ExtensionTestUtils.loadExtension(extension);
  await observed.startup();

  const contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy"
  );
  await checkOrder(observed);
  await observed.unload();

  info("Test pages already existing on extension startup");
  const existing = ExtensionTestUtils.loadExtension(extension);

  await existing.startup();
  await checkOrder(existing);
  await existing.unload();

  await contentPage.close();
});
