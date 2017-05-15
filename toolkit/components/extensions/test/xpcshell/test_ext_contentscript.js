"use strict";

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

ExtensionTestUtils.mockAppInfo();

add_task(async function test_contentscript_runAt() {
  function background() {
    browser.runtime.onMessage.addListener(([msg, expectedStates, readyState], sender) => {
      if (msg == "chrome-namespace-ok") {
        browser.test.sendMessage(msg);
        return;
      }

      browser.test.assertEq("script-run", msg, "message type is correct");
      browser.test.assertTrue(expectedStates.includes(readyState),
                              `readyState "${readyState}" is one of [${expectedStates}]`);
      browser.test.sendMessage("script-run-" + expectedStates[0]);
    });
  }

  function contentScriptStart() {
    browser.runtime.sendMessage(["script-run", ["loading"], document.readyState]);
  }
  function contentScriptEnd() {
    browser.runtime.sendMessage(["script-run", ["interactive", "complete"], document.readyState]);
  }
  function contentScriptIdle() {
    browser.runtime.sendMessage(["script-run", ["complete"], document.readyState]);
  }

  function contentScript() {
    let manifest = browser.runtime.getManifest();
    void manifest.applications.gecko.id;
    browser.runtime.sendMessage(["chrome-namespace-ok"]);
  }

  let extensionData = {
    manifest: {
      applications: {gecko: {id: "contentscript@tests.mozilla.org"}},
      content_scripts: [
        {
          "matches": ["http://*/*/file_sample.html"],
          "js": ["content_script_start.js"],
          "run_at": "document_start",
        },
        {
          "matches": ["http://*/*/file_sample.html"],
          "js": ["content_script_end.js"],
          "run_at": "document_end",
        },
        {
          "matches": ["http://*/*/file_sample.html"],
          "js": ["content_script_idle.js"],
          "run_at": "document_idle",
        },
        {
          "matches": ["http://*/*/file_sample.html"],
          "js": ["content_script_idle.js"],
          // Test default `run_at`.
        },
        {
          "matches": ["http://*/*/file_sample.html"],
          "js": ["content_script.js"],
          "run_at": "document_idle",
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
  extension.onMessage("script-run-loading", () => { loadingCount++; });
  extension.onMessage("script-run-interactive", () => { interactiveCount++; });

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

  let contentPage = await ExtensionTestUtils.loadContentPage(`${BASE_URL}/file_sample.html`);

  await Promise.all([completePromise, chromeNamespacePromise]);

  await contentPage.close();

  equal(loadingCount, 1, "document_start script ran exactly once");
  equal(interactiveCount, 1, "document_end script ran exactly once");
  equal(completeCount, 2, "document_idle script ran exactly twice");

  await extension.unload();
});
