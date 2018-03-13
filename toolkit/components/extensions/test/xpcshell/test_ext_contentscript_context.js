"use strict";

/* eslint-disable mozilla/balanced-listeners */

const server = createHttpServer({hosts: ["example.com", "example.org"]});

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.write("<!DOCTYPE html><html></html>");
});

add_task(async function test_contentscript_context() {
  function contentScript() {
    browser.test.sendMessage("content-script-ready");

    window.addEventListener("pagehide", () => {
      browser.test.sendMessage("content-script-hide");
    }, true);
    window.addEventListener("pageshow", () => {
      browser.test.sendMessage("content-script-show");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [{
        "matches": ["http://example.com/dummy"],
        "js": ["content_script.js"],
        "run_at": "document_start",
      }],
    },

    files: {
      "content_script.js": contentScript,
    },
  });

  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage("http://example.com/dummy");
  await extension.awaitMessage("content-script-ready");
  await extension.awaitMessage("content-script-show");

  // Get the content script context and check that it points to the correct window.
  await contentPage.spawn(extension.id, async extensionId => {
    let {DocumentManager} = ChromeUtils.import("resource://gre/modules/ExtensionContent.jsm", {});
    this.context = DocumentManager.getContext(extensionId, this.content);

    Assert.ok(this.context, "Got content script context");

    Assert.equal(this.context.contentWindow, this.content, "Context's contentWindow property is correct");

    // Navigate so that the content page is hidden in the bfcache.

    this.content.location = "http://example.org/dummy";
  });

  await extension.awaitMessage("content-script-hide");

  await contentPage.spawn(null, async () => {
    Assert.equal(this.context.contentWindow, null, "Context's contentWindow property is null");

    // Navigate back so the content page is resurrected from the bfcache.
    this.content.history.back();
  });

  await extension.awaitMessage("content-script-show");

  await contentPage.spawn(null, async () => {
    Assert.equal(this.context.contentWindow, this.content, "Context's contentWindow property is correct");
  });

  await contentPage.close();
  await extension.awaitMessage("content-script-hide");
  await extension.unload();
});
