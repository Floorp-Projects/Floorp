"use strict";

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

// ExtensionContent.jsm needs to know when it's running from xpcshell,
// to use the right timeout for content scripts executed at document_idle.
ExtensionTestUtils.mockAppInfo();

add_task(async function test_contentscript_runAt() {
  function background() {
    browser.runtime.onMessage.addListener(
      ([msg, expectedStates, readyState], sender) => {
        if (msg == "chrome-namespace-ok") {
          browser.test.sendMessage(msg);
          return;
        }

        browser.test.assertEq("script-run", msg, "message type is correct");
        browser.test.assertTrue(
          expectedStates.includes(readyState),
          `readyState "${readyState}" is one of [${expectedStates}]`
        );
        browser.test.sendMessage("script-run-" + expectedStates[0]);
      }
    );
  }

  function contentScriptStart() {
    browser.runtime.sendMessage([
      "script-run",
      ["loading"],
      document.readyState,
    ]);
  }
  function contentScriptEnd() {
    browser.runtime.sendMessage([
      "script-run",
      ["interactive", "complete"],
      document.readyState,
    ]);
  }
  function contentScriptIdle() {
    browser.runtime.sendMessage([
      "script-run",
      ["complete"],
      document.readyState,
    ]);
  }

  function contentScript() {
    let manifest = browser.runtime.getManifest();
    void manifest.applications.gecko.id;
    browser.runtime.sendMessage(["chrome-namespace-ok"]);
  }

  let extensionData = {
    manifest: {
      applications: { gecko: { id: "contentscript@tests.mozilla.org" } },
      content_scripts: [
        {
          matches: ["http://*/*/file_sample.html"],
          js: ["content_script_start.js"],
          run_at: "document_start",
        },
        {
          matches: ["http://*/*/file_sample.html"],
          js: ["content_script_end.js"],
          run_at: "document_end",
        },
        {
          matches: ["http://*/*/file_sample.html"],
          js: ["content_script_idle.js"],
          run_at: "document_idle",
        },
        {
          matches: ["http://*/*/file_sample.html"],
          js: ["content_script_idle.js"],
          // Test default `run_at`.
        },
        {
          matches: ["http://*/*/file_sample.html"],
          js: ["content_script.js"],
          run_at: "document_idle",
        },
      ],
    },
    background,

    files: {
      "content_script_start.js": contentScriptStart,
      "content_script_end.js": contentScriptEnd,
      "content_script_idle.js": contentScriptIdle,
      "content_script.js": contentScript,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);

  let loadingCount = 0;
  let interactiveCount = 0;
  let completeCount = 0;
  extension.onMessage("script-run-loading", () => {
    loadingCount++;
  });
  extension.onMessage("script-run-interactive", () => {
    interactiveCount++;
  });

  let completePromise = new Promise(resolve => {
    extension.onMessage("script-run-complete", () => {
      completeCount++;
      if (completeCount > 1) {
        resolve();
      }
    });
  });

  let chromeNamespacePromise = extension.awaitMessage("chrome-namespace-ok");

  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );

  await Promise.all([completePromise, chromeNamespacePromise]);

  await contentPage.close();

  equal(loadingCount, 1, "document_start script ran exactly once");
  equal(interactiveCount, 1, "document_end script ran exactly once");
  equal(completeCount, 2, "document_idle script ran exactly twice");

  await extension.unload();
});

add_task(async function test_contentscript_window_open() {
  if (AppConstants.DEBUG && ExtensionTestUtils.remoteContentScripts) {
    return;
  }

  let script = async () => {
    /* globals x */
    browser.test.assertEq(1, x, "Should only run once");

    if (top !== window) {
      // Wait for our parent page to load, then set a timeout to wait for the
      // document.open call, so we make sure to not tear down the extension
      // until after we've done the document.open.
      await new Promise(resolve => {
        top.addEventListener("load", () => setTimeout(resolve, 0), {
          once: true,
        });
      });
    }

    browser.test.sendMessage("content-script", [location.href, top === window]);
  };

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: { gecko: { id: "contentscript@tests.mozilla.org" } },
      content_scripts: [
        {
          matches: ["<all_urls>"],
          js: ["content_script.js"],
          run_at: "document_start",
          match_about_blank: true,
          all_frames: true,
        },
      ],
    },

    files: {
      "content_script.js": `
        var x = (x || 0) + 1;
        (${script})();
      `,
    },
  });

  await extension.startup();

  let url = `${BASE_URL}/file_document_open.html`;
  let contentPage = await ExtensionTestUtils.loadContentPage(url);

  let [pageURL, pageIsTop] = await extension.awaitMessage("content-script");

  // Sometimes we get a content script load for the initial about:blank
  // top level frame here, sometimes we don't. Either way is fine, as long as we
  // don't get two loads into the same document.open() document.
  if (pageURL === "about:blank") {
    equal(pageIsTop, true);
    [pageURL, pageIsTop] = await extension.awaitMessage("content-script");
  }

  Assert.deepEqual([pageURL, pageIsTop], [url, true]);

  let [frameURL, isTop] = await extension.awaitMessage("content-script");
  Assert.deepEqual([frameURL, isTop], [url, false]);

  await contentPage.close();
  await extension.unload();
});

// This test verify that a cached script is still able to catch the document
// while it is still loading (when we do not block the document parsing as
// we do for a non cached script).
add_task(async function test_cached_contentscript_on_document_start() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://localhost/*/file_document_open.html"],
          js: ["content_script.js"],
          run_at: "document_start",
        },
      ],
    },

    files: {
      "content_script.js": `
        browser.test.sendMessage("content-script-loaded", {
          url: window.location.href,
          documentReadyState: document.readyState,
        });
      `,
    },
  });

  await extension.startup();

  let url = `${BASE_URL}/file_document_open.html`;
  let contentPage = await ExtensionTestUtils.loadContentPage(url);

  let msg = await extension.awaitMessage("content-script-loaded");
  Assert.deepEqual(
    msg,
    {
      url,
      documentReadyState: "loading",
    },
    "Got the expected url and document.readyState from a non cached script"
  );

  // Reload the page and check that the cached content script is still able to
  // run on document_start.
  await contentPage.loadURL(url);

  let msgFromCached = await extension.awaitMessage("content-script-loaded");
  Assert.deepEqual(
    msgFromCached,
    {
      url,
      documentReadyState: "loading",
    },
    "Got the expected url and document.readyState from a cached script"
  );

  await extension.unload();

  await contentPage.close();
});
