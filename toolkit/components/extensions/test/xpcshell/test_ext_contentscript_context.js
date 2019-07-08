"use strict";

/* eslint-disable mozilla/balanced-listeners */

const server = createHttpServer({ hosts: ["example.com", "example.org"] });

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.write("<!DOCTYPE html><html></html>");
});

function loadExtension() {
  function contentScript() {
    browser.test.sendMessage("content-script-ready");

    window.addEventListener(
      "pagehide",
      () => {
        browser.test.sendMessage("content-script-hide");
      },
      true
    );
    window.addEventListener("pageshow", () => {
      browser.test.sendMessage("content-script-show");
    });
  }

  return ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/dummy*"],
          js: ["content_script.js"],
          run_at: "document_start",
        },
      ],
    },

    files: {
      "content_script.js": contentScript,
    },
  });
}

add_task(async function test_contentscript_context() {
  let extension = loadExtension();
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy"
  );
  await extension.awaitMessage("content-script-ready");
  await extension.awaitMessage("content-script-show");

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

    this.content.location = "http://example.org/dummy";
  });

  await extension.awaitMessage("content-script-hide");

  await contentPage.spawn(null, async () => {
    Assert.equal(
      this.context.contentWindow,
      null,
      "Context's contentWindow property is null"
    );

    // Navigate back so the content page is resurrected from the bfcache.
    this.content.history.back();
  });

  await extension.awaitMessage("content-script-show");

  await contentPage.spawn(null, async () => {
    Assert.equal(
      this.context.contentWindow,
      this.content,
      "Context's contentWindow property is correct"
    );
  });

  await contentPage.close();
  await extension.awaitMessage("content-script-hide");
  await extension.unload();
});

async function contentscript_context_incognito_not_allowed_test() {
  async function background() {
    await browser.contentScripts.register({
      js: [{ file: "registered_script.js" }],
      matches: ["http://example.com/dummy"],
      runAt: "document_start",
    });

    browser.test.sendMessage("background-ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/dummy"],
          js: ["content_script.js"],
          run_at: "document_start",
        },
      ],
      permissions: ["http://example.com/*"],
    },
    background,
    files: {
      "content_script.js": () => {
        browser.test.notifyFail("content_script_loaded");
      },
      "registered_script.js": () => {
        browser.test.notifyFail("registered_script_loaded");
      },
    },
  });

  await extension.startup();
  await extension.awaitMessage("background-ready");

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy",
    { privateBrowsing: true }
  );

  await contentPage.spawn(extension.id, async extensionId => {
    let { DocumentManager } = ChromeUtils.import(
      "resource://gre/modules/ExtensionContent.jsm",
      null
    );
    let context = DocumentManager.getContext(extensionId, this.content);
    Assert.equal(
      context,
      null,
      "Extension unable to use content_script in private browsing window"
    );
  });

  await contentPage.close();
  await extension.unload();
}

add_task(async function test_contentscript_context_incognito_not_allowed() {
  return runWithPrefs(
    [["extensions.allowPrivateBrowsingByDefault", false]],
    contentscript_context_incognito_not_allowed_test
  );
});

add_task(async function test_contentscript_context_unload_while_in_bfcache() {
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy?first"
  );
  let extension = loadExtension();
  await extension.startup();
  await extension.awaitMessage("content-script-ready");

  // Get the content script context and check that it points to the correct window.
  await contentPage.spawn(extension.id, async extensionId => {
    let { DocumentManager } = ChromeUtils.import(
      "resource://gre/modules/ExtensionContent.jsm",
      null
    );
    // Save context so we can verify that contentWindow is nulled after unload.
    this.context = DocumentManager.getContext(extensionId, this.content);

    Assert.equal(
      this.context.contentWindow,
      this.content,
      "Context's contentWindow property is correct"
    );

    this.contextUnloadedPromise = new Promise(resolve => {
      this.context.callOnClose({ close: resolve });
    });
    this.pageshownPromise = new Promise(resolve => {
      this.content.addEventListener(
        "pageshow",
        () => {
          // Yield to the event loop once more to ensure that all pageshow event
          // handlers have been dispatched before fulfilling the promise.
          let { setTimeout } = ChromeUtils.import(
            "resource://gre/modules/Timer.jsm"
          );
          setTimeout(resolve, 0);
        },
        { once: true, mozSystemGroup: true }
      );
    });

    // Navigate so that the content page is hidden in the bfcache.
    this.content.location = "http://example.org/dummy?second";
  });

  await extension.awaitMessage("content-script-hide");

  await extension.unload();
  await contentPage.spawn(null, async () => {
    await this.contextUnloadedPromise;
    Assert.equal(this.context.unloaded, true, "Context has been unloaded");

    // Normally, when a page is not in the bfcache, context.contentWindow is
    // not null when the callOnClose handler is invoked (this is checked by the
    // previous subtest).
    // Now wait a little bit and check again to ensure that the contentWindow
    // property is not somehow restored.
    await new Promise(resolve => this.content.setTimeout(resolve, 0));
    Assert.equal(
      this.context.contentWindow,
      null,
      "Context's contentWindow property is null"
    );

    // Navigate back so the content page is resurrected from the bfcache.
    this.content.history.back();

    await this.pageshownPromise;

    Assert.equal(
      this.context.contentWindow,
      null,
      "Context's contentWindow property is null after restore from bfcache"
    );
  });

  await contentPage.close();
});

add_task(async function test_contentscript_context_valid_during_execution() {
  // This test does the following:
  // - Load page
  // - Load extension; inject content script.
  // - Navigate page; pagehide triggered.
  // - Navigate back; pageshow triggered.
  // - Close page; pagehide, unload triggered.
  // At each of these last four events, the validity of the context is checked.

  function contentScript() {
    browser.test.sendMessage("content-script-ready");
    window.wrappedJSObject.checkContextIsValid("Context is valid on execution");

    window.addEventListener(
      "pagehide",
      () => {
        window.wrappedJSObject.checkContextIsValid(
          "Context is valid on pagehide"
        );
        browser.test.sendMessage("content-script-hide");
      },
      true
    );
    window.addEventListener("pageshow", () => {
      window.wrappedJSObject.checkContextIsValid(
        "Context is valid on pageshow"
      );

      // This unload listener is registered after pageshow, to ensure that the
      // page can be stored in the bfcache at the previous pagehide.
      window.addEventListener("unload", () => {
        window.wrappedJSObject.checkContextIsValid(
          "Context is valid on unload"
        );
        browser.test.sendMessage("content-script-unload");
      });

      browser.test.sendMessage("content-script-show");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/dummy*"],
          js: ["content_script.js"],
        },
      ],
    },

    files: {
      "content_script.js": contentScript,
    },
  });

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy?first"
  );
  await contentPage.spawn(extension.id, async extensionId => {
    let context;
    let checkContextIsValid = description => {
      if (!context) {
        let { DocumentManager } = ChromeUtils.import(
          "resource://gre/modules/ExtensionContent.jsm",
          null
        );
        context = DocumentManager.getContext(extensionId, this.content);
      }
      Assert.equal(
        context.contentWindow,
        this.content,
        `${description}: contentWindow`
      );
      Assert.equal(context.active, true, `${description}: active`);
    };
    Cu.exportFunction(checkContextIsValid, this.content, {
      defineAs: "checkContextIsValid",
    });
  });
  await extension.startup();
  await extension.awaitMessage("content-script-ready");

  await contentPage.spawn(extension.id, async extensionId => {
    // Navigate so that the content page is frozen in the bfcache.
    this.content.location = "http://example.org/dummy?second";
  });

  await extension.awaitMessage("content-script-hide");
  await contentPage.spawn(null, async () => {
    // Navigate back so the content page is resurrected from the bfcache.
    this.content.history.back();
  });

  await extension.awaitMessage("content-script-show");
  await contentPage.close();
  await extension.awaitMessage("content-script-hide");
  await extension.awaitMessage("content-script-unload");
  await extension.unload();
});
