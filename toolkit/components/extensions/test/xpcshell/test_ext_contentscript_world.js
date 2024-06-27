"use strict";

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

// ExtensionContent.sys.mjs needs to know when it's running from xpcshell, to use
// the right timeout for content scripts executed at document_idle.
ExtensionTestUtils.mockAppInfo();

add_task(async function manifest_content_scripts_invalid_world() {
  ExtensionTestUtils.failOnSchemaWarnings(false);
  let normalized = await ExtensionTestUtils.normalizeManifest({
    content_scripts: [
      {
        js: ["dummy.js"],
        matches: ["*://*/*"],
        // MAIN and ISOLATED are the only supported values. The userScripts API
        // may define userScripts.ExecutionWorld.USER_SCRIPT, but that should
        // not be permitted as a value in manifest.json.
        world: "USER_SCRIPT",
      },
    ],
  });
  ExtensionTestUtils.failOnSchemaWarnings(true);

  equal(
    normalized.error,
    'Error processing content_scripts.0.world: Invalid enumeration value "USER_SCRIPT"',
    "Should refuse to load extension with an unsupported world value"
  );
  Assert.deepEqual(normalized.errors, [], "Should not have any other warnings");
});

add_task(async function manifest_content_scripts_world_MAIN() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          js: ["main.js"],
          matches: ["*://*/data/file_simple_inline_script.html"],
          run_at: "document_end",
          world: "MAIN",
        },
        {
          js: ["isolated.js"],
          matches: ["*://*/data/file_simple_inline_script.html"],
          world: "ISOLATED",
        },
      ],
    },
    files: {
      "main.js": () => {
        globalThis.varFromExt = "varFromExt";
        globalThis.varInPageReadByExt = window.varInPage;
        globalThis.thisIsGlobalThis = this === globalThis;
        globalThis.thisIsWindow = this === window;
        globalThis.browserIsUndefined = typeof browser == "undefined";
      },
      "isolated.js": () => {
        // Sanity check that the isolated world is distinct from main world.
        browser.test.assertEq(
          undefined,
          window.varInPage,
          "varInPage not directly visible in contentscript in ISOLATED world"
        );
        browser.test.assertEq(
          "varInPage",
          window.wrappedJSObject.varInPage,
          "After waiving xrays content script can see varInPage from page"
        );

        // Verify that main.js has executed.
        browser.test.assertEq(
          "varFromExt",
          window.wrappedJSObject.varFromExt,
          "MAIN world script has executed"
        );
        browser.test.assertEq(
          "varInPage",
          window.wrappedJSObject.varInPageReadByExt,
          "MAIN world script ran after the page's own scripts by default"
        );

        // Verify some more state in the main script.
        browser.test.assertTrue(
          window.wrappedJSObject.thisIsGlobalThis,
          "this === globalThis in MAIN script"
        );
        browser.test.assertTrue(
          window.wrappedJSObject.thisIsWindow,
          "this === window in MAIN script"
        );
        browser.test.assertTrue(
          window.wrappedJSObject.browserIsUndefined,
          "extension APIs are not defined (browser is undefined)"
        );
        browser.test.sendMessage("done");
      },
    },
  });
  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/data/file_simple_inline_script.html"
  );
  await extension.awaitMessage("done");
  await contentPage.close();
  await extension.unload();
});

add_task(async function manifest_content_scripts_world_MAIN_runAt_order() {
  const files = {};
  const main_world_content_scripts = [];
  for (const run_at of ["document_idle", "document_end", "document_start"]) {
    let filename = `main_world_${run_at}.js`;
    files[filename] = `
      globalThis.main_world_content_scripts_observed_run_at ??= [];
      main_world_content_scripts_observed_run_at.push("${run_at}");

      globalThis.ext_has_seen_varInPage ??= {};
      globalThis.ext_has_seen_varInPage["${run_at}"] = globalThis.varInPage;
    `;
    main_world_content_scripts.push({
      js: [filename],
      run_at,
      matches: ["*://*/data/file_simple_inline_script.html"],
      world: "MAIN",
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        ...main_world_content_scripts,
        {
          js: ["isolated_report_results.js"],
          matches: ["*://*/data/file_simple_inline_script.html"],
          world: "ISOLATED",
        },
      ],
    },
    files: {
      ...files,
      "isolated_report_results.js": () => {
        browser.test.assertDeepEq(
          window.wrappedJSObject.main_world_content_scripts_observed_run_at,
          ["document_start", "document_end", "document_idle"],
          "main world script ran in the order given by run_at"
        );

        browser.test.assertDeepEq(
          window.wrappedJSObject.ext_has_seen_varInPage,
          {
            document_start: undefined,
            document_end: "varInPage",
            document_idle: "varInPage",
          },
          "varInPage seen at the right time"
        );
        browser.test.sendMessage("done");
      },
    },
  });
  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/data/file_simple_inline_script.html"
  );
  await extension.awaitMessage("done");
  await contentPage.close();
  await extension.unload();
});

add_task(async function manifest_content_scripts_anonymous_filename() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          js: ["file_leak_me.js"],
          matches: ["*://*/data/file_simple_inline_script.html"],
          run_at: "document_end",
          world: "MAIN",
        },
        {
          js: ["file_leak_me.js"],
          matches: ["*://*/data/file_simple_inline_script.html"],
          world: "ISOLATED",
        },
      ],
    },
    files: {
      "file_leak_me.js": function fileExtractFilename() {
        let error = new Error();
        let result = {
          fileName: error.fileName,
          // We only care about file names, so strip line and column numbers.
          stack: error.stack.replaceAll(/:\d+:\d+/g, ""),
        };
        if (window.varInPage) {
          window.mainResult = result;
        } else {
          // The content script runs in an execution environment isolated from
          // non-extension code, so it is safe to expose the actual script URL.
          const contentScriptUrl = browser.runtime.getURL("file_leak_me.js");
          browser.test.assertDeepEq(
            {
              fileName: contentScriptUrl,
              stack: `fileExtractFilename@${contentScriptUrl}\n@${contentScriptUrl}\n`,
            },
            result,
            "world:ISOLATED content script may see the extension URL"
          );

          // Code running in the MAIN world is shared with the web page. To
          // minimize information leakage, the script sources are anonymous.
          browser.test.assertDeepEq(
            {
              fileName: "<anonymous code>",
              stack:
                "fileExtractFilename@<anonymous code>\n@<anonymous code>\n",
            },
            window.wrappedJSObject.mainResult,
            "world:MAIN content script should not leak the extension URL"
          );

          browser.test.sendMessage("done");
        }
      },
    },
  });
  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/data/file_simple_inline_script.html"
  );
  await extension.awaitMessage("done");
  await contentPage.close();
  await extension.unload();
});
