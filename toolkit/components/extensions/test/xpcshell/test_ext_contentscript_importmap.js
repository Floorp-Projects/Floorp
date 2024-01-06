"use strict";

// Currently import maps are not supported for web extensions, neither for
// content scripts nor moz-extension documents.
// For content scripts that's because they use their own sandbox module loaders,
// which is different from the DOM module loader.
// As for moz-extension documents, that's because inline script tags is not
// allowed by CSP. (Currently import maps can be only added through inline
// script tag.)
//
// This test is used to verified import maps are not supported for web
// extensions.
// See Bug 1765275: Enable Import maps for web extension content scripts.

const server = createHttpServer({ hosts: ["example.com"] });

const importMapString = `
  <script type="importmap">
  {
    "imports": {
      "simple": "./simple.js",
      "simple2": "./simple2.js"
    }
  }
  </script>`;

const importMapHtml = `
  <!DOCTYPE html>
  <html>
  <meta charset=utf-8>
  <title>Test a simple import map in normal webpage</title>
  <body>
  ${importMapString}
  </body></html>`;

// page.html will load page.js, which will call import();
const pageHtml = `
  <!DOCTYPE html>
  <html>
  <meta charset=utf-8>
  <title>Test a simple import map in moz-extension documents</title>
  <body>
  ${importMapString}
  <script src="page.js"></script>
  </body></html>`;

const simple2JS = `export let foo = 2;`;

server.registerPathHandler("/importmap.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.write(importMapHtml);
});

server.registerPathHandler("/simple.js", (request, response) => {
  ok(false, "Unexpected request to /simple.js");
});

server.registerPathHandler("/simple2.js", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/javascript", false);
  response.write(simple2JS);
});

add_task(async function test_importMaps_not_supported() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/importmap.html"],
          js: ["main.js"],
        },
      ],
    },

    files: {
      "main.js": async function () {
        // Content scripts shouldn't be able to use the bare specifier from
        // the import map.
        await browser.test.assertRejects(
          import("simple"),
          /The specifier “simple” was a bare specifier/,
          `should reject import("simple")`
        );

        browser.test.sendMessage("done");
      },
      "page.html": pageHtml,
      "page.js": async function () {
        await browser.test.assertRejects(
          import("simple"),
          /The specifier “simple” was a bare specifier/,
          `should reject import("simple")`
        );
        browser.test.sendMessage("page-done");
      },
    },
  });

  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/importmap.html"
  );
  await extension.awaitMessage("done");

  await contentPage.spawn([], async () => {
    // Import maps should work for documents.
    let promise = content.eval(`import("simple2")`);
    let mod = (await promise.wrappedJSObject).wrappedJSObject;
    Assert.equal(mod.foo, 2, "mod.foo should be 2");
  });

  // moz-extension documents doesn't allow inline scripts, so the import map
  // script tag won't be processed.
  let url = `moz-extension://${extension.uuid}/page.html`;
  let page = await ExtensionTestUtils.loadContentPage(url, { extension });
  await extension.awaitMessage("page-done");

  await page.close();
  await extension.unload();
  await contentPage.close();
});
