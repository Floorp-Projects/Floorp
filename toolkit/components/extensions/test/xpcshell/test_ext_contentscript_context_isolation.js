"use strict";

/* globals exportFunction */
/* eslint-disable mozilla/balanced-listeners */

const server = createHttpServer({ hosts: ["example.com", "example.org"] });

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.write("<!DOCTYPE html><html></html>");
});

server.registerPathHandler("/bfcachetestpage", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html;charset=utf-8", false);
  response.write(`<!DOCTYPE html>
<script>
  window.addEventListener("pageshow", (event) => {
    event.stopImmediatePropagation();
    if (window.browserTestSendMessage) {
      browserTestSendMessage("content-script-show");
    }
  });
  window.addEventListener("pagehide", (event) => {
    event.stopImmediatePropagation();
    if (window.browserTestSendMessage) {
      if (event.persisted) {
        browserTestSendMessage("content-script-hide");
      } else {
        browserTestSendMessage("content-script-unload");
      }
    }
  }, true);
</script>`);
});

add_task(async function test_contentscript_context_isolation() {
  function contentScript() {
    browser.test.sendMessage("content-script-ready");

    exportFunction(browser.test.sendMessage, window, {
      defineAs: "browserTestSendMessage",
    });

    window.addEventListener("pageshow", () => {
      browser.test.fail(
        "pageshow should have been suppressed by stopImmediatePropagation"
      );
    });
    window.addEventListener(
      "pagehide",
      () => {
        browser.test.fail(
          "pagehide should have been suppressed by stopImmediatePropagation"
        );
      },
      true
    );
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/bfcachetestpage"],
          js: ["content_script.js"],
        },
      ],
    },

    files: {
      "content_script.js": contentScript,
    },
  });

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/bfcachetestpage"
  );
  await extension.startup();
  await extension.awaitMessage("content-script-ready");

  // Get the content script context and check that it points to the correct window.
  await contentPage.spawn(extension.id, async extensionId => {
    let { DocumentManager } = ChromeUtils.import(
      "resource://gre/modules/ExtensionContent.jsm",
      null
    );
    this.context = DocumentManager.getContext(extensionId, this.content);

    Assert.ok(this.context, "Got content script context");

    Assert.equal(
      this.context.contentWindow,
      this.content,
      "Context's contentWindow property is correct"
    );

    // Navigate so that the content page is hidden in the bfcache.

    this.content.location = "http://example.org/dummy?noscripthere1";
  });

  await extension.awaitMessage("content-script-hide");

  await contentPage.spawn(null, async () => {
    Assert.equal(
      this.context.contentWindow,
      null,
      "Context's contentWindow property is null"
    );
    Assert.ok(this.context.sandbox, "Context's sandbox exists");

    // Navigate back so the content page is resurrected from the bfcache.
    this.content.history.back();
  });

  await extension.awaitMessage("content-script-show");

  async function testWithoutBfcache() {
    return contentPage.spawn(null, async () => {
      Assert.equal(
        this.context.contentWindow,
        this.content,
        "Context's contentWindow property is correct"
      );
      Assert.ok(this.context.sandbox, "Context's sandbox exists before unload");

      let contextUnloadedPromise = new Promise(resolve => {
        this.context.callOnClose({ close: resolve });
      });

      // Now add an "unload" event listener, which should prevent a page from entering the bfcache.
      await new Promise(resolve => {
        this.content.addEventListener("unload", () => {
          Assert.equal(
            this.context.contentWindow,
            this.content,
            "Context's contentWindow property should be non-null at unload"
          );
          resolve();
        });
        this.content.location = "http://example.org/dummy?noscripthere2";
      });

      await contextUnloadedPromise;
    });
  }
  await runWithPrefs(
    [["docshell.shistory.bfcache.allow_unload_listeners", false]],
    testWithoutBfcache
  );

  await extension.awaitMessage("content-script-unload");

  await contentPage.spawn(null, async () => {
    Assert.equal(
      this.context.sandbox,
      null,
      "Context's sandbox has been destroyed after unload"
    );
  });

  await contentPage.close();
  await extension.unload();
});
