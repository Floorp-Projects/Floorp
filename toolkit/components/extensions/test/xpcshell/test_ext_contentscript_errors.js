"use strict";

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;
const TEST_URL_1 = `${BASE_URL}/file_sample.html`;
const TEST_URL_2 = `${BASE_URL}/file_content_script_errors.html`;

// ExtensionContent.jsm needs to know when it's running from xpcshell,
// to use the right timeout for content scripts executed at document_idle.
ExtensionTestUtils.mockAppInfo();

add_task(async function test_cached_contentscript_on_document_start() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        // Use distinct content scripts as some will throw and would prevent executing the next script
        {
          matches: ["http://*/*/file_content_script_errors.html"],
          js: ["script1.js"],
          run_at: "document_start",
        },
        {
          matches: ["http://*/*/file_content_script_errors.html"],
          js: ["script2.js"],
          run_at: "document_start",
        },
        {
          matches: ["http://*/*/file_content_script_errors.html"],
          js: ["script3.js"],
          run_at: "document_start",
        },
        {
          matches: ["http://*/*/file_content_script_errors.html"],
          js: ["script4.js"],
          run_at: "document_start",
        },
        {
          matches: ["http://*/*/file_content_script_errors.html"],
          js: ["script5.js"],
          run_at: "document_start",
        },
      ],
    },

    files: {
      "script1.js": `
        throw new Error("Object exception");
      `,
      "script2.js": `
        throw "String exception";
      `,
      "script3.js": `
        undefinedSymbol();
      `,
      "script4.js": `
        )
      `,
      "script5.js": `
        Promise.reject("rejected promise");

        (async () => {
          /* make the async, really async */
          await new Promise(r => setTimeout(r, 0));
          throw new Error("async function exception");
        })();

        setTimeout(() => {
          asyncUndefinedSymbol();
        });

        /* Use a delay in order to resume test execution after these async errors */
        setTimeout(() => {
          browser.test.sendMessage("content-script-loaded");
        }, 500);
      `,
    },
  });

  await extension.startup();

  // Load a first page in order to be able to register a console listener in the content process.
  // This has to be done in the same domain of the second page to stay in the same process.
  let contentPage = await ExtensionTestUtils.loadContentPage(TEST_URL_1);

  // Listen to the errors logged in the content process.
  ContentTask.spawn(contentPage.browser, {}, () => {
    this.collectedErrors = [];

    this.consoleErrorListener = error => {
      error.QueryInterface(Ci.nsIScriptError);
      // Ignore errors from ExtensionContent.jsm
      if (error.innerWindowID) {
        this.collectedErrors.push({
          innerWindowID: error.innerWindowID,
          message: error.errorMessage,
        });
      }
    };

    Services.console.registerListener(this.consoleErrorListener);
  });

  // Reload the page and check that the cached content script is still able to
  // run on document_start.
  await contentPage.loadURL(TEST_URL_2);

  await extension.awaitMessage("content-script-loaded");

  const errors = await ContentTask.spawn(contentPage.browser, {}, () => {
    Services.console.unregisterListener(this.consoleErrorListener);
    return this.collectedErrors;
  });
  equal(errors.length, 7);
  for (const { innerWindowID, message } of errors) {
    equal(
      innerWindowID,
      contentPage.browser.innerWindowID,
      `Message ${message} has the innerWindowID set`
    );
  }

  await extension.unload();

  await contentPage.close();
});
