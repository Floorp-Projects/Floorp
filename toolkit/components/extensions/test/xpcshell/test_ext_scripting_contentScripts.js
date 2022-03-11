"use strict";

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

// ExtensionContent.jsm needs to know when it's running from xpcshell, to use
// the right timeout for content scripts executed at document_idle.
ExtensionTestUtils.mockAppInfo();

Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);

const makeExtension = ({ manifest: manifestProps, ...otherProps }) => {
  return ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      permissions: ["scripting"],
      ...manifestProps,
    },
    ...otherProps,
  });
};

add_task(async function test_registerContentScripts_runAt() {
  let extension = makeExtension({
    async background() {
      const TEST_CASES = [
        {
          title: "runAt: document_idle",
          params: [
            {
              id: "script-idle",
              js: ["script-idle.js"],
              matches: ["http://*/*/file_sample.html"],
              runAt: "document_idle",
              persistAcrossSessions: false,
            },
          ],
        },
        {
          title: "no runAt specified",
          params: [
            {
              id: "script-idle-default",
              js: ["script-idle-default.js"],
              matches: ["http://*/*/file_sample.html"],
              // `runAt` defaults to `document_idle`.
              persistAcrossSessions: false,
            },
          ],
        },
        {
          title: "runAt: document_end",
          params: [
            {
              id: "script-end",
              js: ["script-end.js"],
              matches: ["http://*/*/file_sample.html"],
              runAt: "document_end",
              persistAcrossSessions: false,
            },
          ],
        },
        {
          title: "runAt: document_start",
          params: [
            {
              id: "script-start",
              js: ["script-start.js"],
              matches: ["http://*/*/file_sample.html"],
              runAt: "document_start",
              persistAcrossSessions: false,
            },
          ],
        },
      ];

      let scripts = await browser.scripting.getRegisteredContentScripts();
      browser.test.assertEq(0, scripts.length, "expected no registered script");

      for (const { title, params } of TEST_CASES) {
        const res = await browser.scripting.registerContentScripts(params);
        browser.test.assertEq(undefined, res, `${title} - expected no result`);
      }

      scripts = await browser.scripting.getRegisteredContentScripts();
      browser.test.assertEq(
        TEST_CASES.length,
        scripts.length,
        `expected ${TEST_CASES.length} registered scripts`
      );
      browser.test.assertEq(
        JSON.stringify([
          {
            id: "script-idle",
            allFrames: false,
            js: ["script-idle.js"],
            matches: ["http://*/*/file_sample.html"],
            runAt: "document_idle",
            persistAcrossSessions: false,
          },
          {
            id: "script-idle-default",
            allFrames: false,
            js: ["script-idle-default.js"],
            matches: ["http://*/*/file_sample.html"],
            runAt: "document_idle",
            persistAcrossSessions: false,
          },
          {
            id: "script-end",
            allFrames: false,
            js: ["script-end.js"],
            matches: ["http://*/*/file_sample.html"],
            runAt: "document_end",
            persistAcrossSessions: false,
          },
          {
            id: "script-start",
            allFrames: false,
            js: ["script-start.js"],
            matches: ["http://*/*/file_sample.html"],
            runAt: "document_start",
            persistAcrossSessions: false,
          },
        ]),
        JSON.stringify(scripts),
        "got expected scripts"
      );

      browser.test.sendMessage("background-ready");
    },
    files: {
      "script-start.js": () => {
        browser.test.assertEq(
          "loading",
          document.readyState,
          "expected state 'loading' at document_start"
        );
        browser.test.sendMessage("script-ran", "script-start.js");
      },
      "script-end.js": () => {
        browser.test.assertTrue(
          ["interactive", "complete"].includes(document.readyState),
          `expected state 'interactive' or 'complete' at document_end, got: ${document.readyState}`
        );
        browser.test.sendMessage("script-ran", "script-end.js");
      },
      "script-idle.js": () => {
        browser.test.assertEq(
          "complete",
          document.readyState,
          "expected state 'complete' at document_idle"
        );
        browser.test.sendMessage("script-ran", "script-idle.js");
      },
      "script-idle-default.js": () => {
        browser.test.assertEq(
          "complete",
          document.readyState,
          "expected state 'complete' at document_idle"
        );
        browser.test.sendMessage("script-ran", "script-idle-default.js");
      },
    },
  });

  let scriptsRan = [];
  let completePromise = new Promise(resolve => {
    extension.onMessage("script-ran", result => {
      scriptsRan.push(result);

      // The value below should be updated when TEST_CASES above is changed.
      if (scriptsRan.length === 4) {
        resolve();
      }
    });
  });

  await extension.startup();
  await extension.awaitMessage("background-ready");

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );

  await completePromise;

  Assert.deepEqual(
    [
      "script-start.js",
      "script-end.js",
      "script-idle.js",
      "script-idle-default.js",
    ],
    scriptsRan,
    "got expected executed scripts"
  );

  await contentPage.close();
  await extension.unload();
});

add_task(async function test_register_and_unregister() {
  let extension = makeExtension({
    async background() {
      const script = {
        id: "a-script",
        js: ["script.js"],
        matches: ["http://*/*/file_sample.html"],
        persistAcrossSessions: false,
      };

      let results = await Promise.allSettled([
        browser.scripting.registerContentScripts([script]),
        browser.scripting.unregisterContentScripts(),
      ]);

      browser.test.assertEq(
        2,
        results.filter(result => result.status === "fulfilled").length,
        "got expected number of fulfilled promises"
      );

      let scripts = await browser.scripting.getRegisteredContentScripts();
      browser.test.assertEq(0, scripts.length, "expected no registered script");

      browser.test.sendMessage("background-done");
    },
    files: {
      "script.js": "",
    },
  });

  await extension.startup();
  await extension.awaitMessage("background-done");

  // Verify that the registered content scripts on the extension are correct.
  let contentScripts = Array.from(
    extension.extension.registeredContentScripts.values()
  );
  Assert.equal(0, contentScripts.length, "expected no registered scripts");

  await extension.unload();
});

add_task(async function test_register_and_unregister_multiple_times() {
  let extension = makeExtension({
    async background() {
      // We use the same script `id` on purpose in this test.
      let results = await Promise.allSettled([
        browser.scripting.registerContentScripts([
          {
            id: "a-script",
            js: ["script-1.js"],
            matches: ["http://*/*/file_sample.html"],
            persistAcrossSessions: false,
          },
        ]),
        browser.scripting.unregisterContentScripts(),
        browser.scripting.registerContentScripts([
          {
            id: "a-script",
            js: ["script-2.js"],
            matches: ["http://*/*/file_sample.html"],
            persistAcrossSessions: false,
          },
        ]),
        browser.scripting.unregisterContentScripts(),
        browser.scripting.registerContentScripts([
          {
            id: "a-script",
            js: ["script-3.js"],
            matches: ["http://*/*/file_sample.html"],
            persistAcrossSessions: false,
          },
        ]),
      ]);

      browser.test.assertEq(
        5,
        results.filter(result => result.status === "fulfilled").length,
        "got expected number of fulfilled promises"
      );

      let scripts = await browser.scripting.getRegisteredContentScripts();
      browser.test.assertEq(1, scripts.length, "expected 1 registered script");

      browser.test.sendMessage("background-done");
    },
    files: {
      "script-1.js": "",
      "script-2.js": "",
      "script-3.js": "",
    },
  });

  await extension.startup();
  await extension.awaitMessage("background-done");

  // Verify that the registered content scripts on the extension are correct.
  let contentScripts = Array.from(
    extension.extension.registeredContentScripts.values()
  );
  Assert.equal(1, contentScripts.length, "expected 1 registered script");
  Assert.ok(
    contentScripts[0].jsPaths[0].endsWith("script-3.js"),
    "got expected js file"
  );

  await extension.unload();
});
