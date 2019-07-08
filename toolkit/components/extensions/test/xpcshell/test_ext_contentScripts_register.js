"use strict";

const { createAppInfo } = AddonTestUtils;

AddonTestUtils.init(this);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "49");

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

function check_applied_styles() {
  const urlElStyle = getComputedStyle(
    document.querySelector("#registered-extension-url-style")
  );
  const blobElStyle = getComputedStyle(
    document.querySelector("#registered-extension-text-style")
  );

  browser.test.sendMessage("registered-styles-results", {
    registeredExtensionUrlStyleBG: urlElStyle["background-color"],
    registeredExtensionBlobStyleBG: blobElStyle["background-color"],
  });
}

add_task(async function test_contentscripts_register_css() {
  async function background() {
    let cssCode = `
      #registered-extension-text-style {
        background-color: blue;
      }
    `;

    const matches = ["http://localhost/*/file_sample_registered_styles.html"];

    browser.test.assertThrows(
      () => {
        browser.contentScripts.register({
          matches,
          unknownParam: "unexpected property",
        });
      },
      /Unexpected property "unknownParam"/,
      "contentScripts.register throws on unexpected properties"
    );

    let fileScript = await browser.contentScripts.register({
      css: [{ file: "registered_ext_style.css" }],
      matches,
      runAt: "document_start",
    });

    let textScript = await browser.contentScripts.register({
      css: [{ code: cssCode }],
      matches,
      runAt: "document_start",
    });

    browser.test.onMessage.addListener(async msg => {
      switch (msg) {
        case "unregister-text":
          await textScript.unregister().catch(err => {
            browser.test.fail(
              `Unexpected exception while unregistering text style: ${err}`
            );
          });

          await browser.test.assertRejects(
            textScript.unregister(),
            /Content script already unregistered/,
            "Got the expected rejection on calling script.unregister() multiple times"
          );

          browser.test.sendMessage("unregister-text:done");
          break;
        case "unregister-file":
          await fileScript.unregister().catch(err => {
            browser.test.fail(
              `Unexpected exception while unregistering url style: ${err}`
            );
          });

          await browser.test.assertRejects(
            fileScript.unregister(),
            /Content script already unregistered/,
            "Got the expected rejection on calling script.unregister() multiple times"
          );

          browser.test.sendMessage("unregister-file:done");
          break;
        default:
          browser.test.fail(`Unexpected test message received: ${msg}`);
      }
    });

    browser.test.sendMessage("background_ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: [
        "http://localhost/*/file_sample_registered_styles.html",
        "<all_urls>",
      ],
      content_scripts: [
        {
          matches: ["http://localhost/*/file_sample_registered_styles.html"],
          run_at: "document_idle",
          js: ["check_applied_styles.js"],
        },
      ],
    },
    background,

    files: {
      "check_applied_styles.js": check_applied_styles,
      "registered_ext_style.css": `
        #registered-extension-url-style {
          background-color: red;
        }
      `,
    },
  });

  await extension.startup();

  await extension.awaitMessage("background_ready");

  // Ensure that a content page running in a content process and which has been
  // started after the content scripts has been registered, it still receives
  // and registers the expected content scripts.
  let contentPage = await ExtensionTestUtils.loadContentPage(`about:blank`);

  await contentPage.loadURL(`${BASE_URL}/file_sample_registered_styles.html`);

  const registeredStylesResults = await extension.awaitMessage(
    "registered-styles-results"
  );

  equal(
    registeredStylesResults.registeredExtensionUrlStyleBG,
    "rgb(255, 0, 0)",
    "The expected style has been applied from the registered extension url style"
  );
  equal(
    registeredStylesResults.registeredExtensionBlobStyleBG,
    "rgb(0, 0, 255)",
    "The expected style has been applied from the registered extension blob style"
  );

  extension.sendMessage("unregister-file");
  await extension.awaitMessage("unregister-file:done");

  await contentPage.loadURL(`${BASE_URL}/file_sample_registered_styles.html`);

  const unregisteredURLStylesResults = await extension.awaitMessage(
    "registered-styles-results"
  );

  equal(
    unregisteredURLStylesResults.registeredExtensionUrlStyleBG,
    "rgba(0, 0, 0, 0)",
    "The expected style has been applied once extension url style has been unregistered"
  );
  equal(
    unregisteredURLStylesResults.registeredExtensionBlobStyleBG,
    "rgb(0, 0, 255)",
    "The expected style has been applied from the registered extension blob style"
  );

  extension.sendMessage("unregister-text");
  await extension.awaitMessage("unregister-text:done");

  await contentPage.loadURL(`${BASE_URL}/file_sample_registered_styles.html`);

  const unregisteredBlobStylesResults = await extension.awaitMessage(
    "registered-styles-results"
  );

  equal(
    unregisteredBlobStylesResults.registeredExtensionUrlStyleBG,
    "rgba(0, 0, 0, 0)",
    "The expected style has been applied once extension url style has been unregistered"
  );
  equal(
    unregisteredBlobStylesResults.registeredExtensionBlobStyleBG,
    "rgba(0, 0, 0, 0)",
    "The expected style has been applied once extension blob style has been unregistered"
  );

  await contentPage.close();
  await extension.unload();
});

add_task(async function test_contentscripts_unregister_on_context_unload() {
  async function background() {
    const frame = document.createElement("iframe");
    frame.setAttribute("src", "/background-frame.html");

    document.body.appendChild(frame);

    browser.test.onMessage.addListener(msg => {
      switch (msg) {
        case "unload-frame":
          frame.remove();
          browser.test.sendMessage("unload-frame:done");
          break;
        default:
          browser.test.fail(`Unexpected test message received: ${msg}`);
      }
    });

    browser.test.sendMessage("background_ready");
  }

  async function background_frame() {
    await browser.contentScripts.register({
      css: [{ file: "registered_ext_style.css" }],
      matches: ["http://localhost/*/file_sample_registered_styles.html"],
      runAt: "document_start",
    });

    browser.test.sendMessage("background_frame_ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["http://localhost/*/file_sample_registered_styles.html"],
      content_scripts: [
        {
          matches: ["http://localhost/*/file_sample_registered_styles.html"],
          run_at: "document_idle",
          js: ["check_applied_styles.js"],
        },
      ],
    },
    background,

    files: {
      "background-frame.html": `<!DOCTYPE html>
        <html>
          <head>
            <script src="background-frame.js"></script>
          </head>
          <body>
          </body>
        </html>
      `,
      "background-frame.js": background_frame,
      "check_applied_styles.js": check_applied_styles,
      "registered_ext_style.css": `
        #registered-extension-url-style {
          background-color: red;
        }
      `,
    },
  });

  await extension.startup();

  await extension.awaitMessage("background_ready");

  // Wait the background frame to have been loaded and its script
  // executed.
  await extension.awaitMessage("background_frame_ready");

  // Ensure that a content page running in a content process and which has been
  // started after the content scripts has been registered, it still receives
  // and registers the expected content scripts.
  let contentPage = await ExtensionTestUtils.loadContentPage(`about:blank`);

  await contentPage.loadURL(`${BASE_URL}/file_sample_registered_styles.html`);

  const registeredStylesResults = await extension.awaitMessage(
    "registered-styles-results"
  );

  equal(
    registeredStylesResults.registeredExtensionUrlStyleBG,
    "rgb(255, 0, 0)",
    "The expected style has been applied from the registered extension url style"
  );

  extension.sendMessage("unload-frame");
  await extension.awaitMessage("unload-frame:done");

  await contentPage.loadURL(`${BASE_URL}/file_sample_registered_styles.html`);

  const unregisteredURLStylesResults = await extension.awaitMessage(
    "registered-styles-results"
  );

  equal(
    unregisteredURLStylesResults.registeredExtensionUrlStyleBG,
    "rgba(0, 0, 0, 0)",
    "The expected style has been applied once extension url style has been unregistered"
  );

  await contentPage.close();
  await extension.unload();
});

add_task(async function test_contentscripts_register_js() {
  async function background() {
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

    // Raise an exception when the content script cannot be registered
    // because the extension has no permission to access the specified origin.

    await browser.test.assertRejects(
      browser.contentScripts.register({
        matches: ["http://*/*"],
        js: [
          {
            code:
              'browser.test.fail("content script with wrong matches should not run")',
          },
        ],
      }),
      /Permission denied to register a content script for/,
      "The reject contains the expected error message"
    );

    // Register a content script from a JS code string.

    function textScriptCodeStart() {
      browser.runtime.sendMessage([
        "script-run",
        ["loading"],
        document.readyState,
      ]);
    }
    function textScriptCodeEnd() {
      browser.runtime.sendMessage([
        "script-run",
        ["interactive", "complete"],
        document.readyState,
      ]);
    }
    function textScriptCodeIdle() {
      browser.runtime.sendMessage([
        "script-run",
        ["complete"],
        document.readyState,
      ]);
    }

    // Register content scripts from both extension URLs and plain JS code strings.

    const content_scripts = [
      // Plain JS code strings.
      {
        matches: ["http://localhost/*/file_sample.html"],
        js: [{ code: `(${textScriptCodeStart})()` }],
        runAt: "document_start",
      },
      {
        matches: ["http://localhost/*/file_sample.html"],
        js: [{ code: `(${textScriptCodeEnd})()` }],
        runAt: "document_end",
      },
      {
        matches: ["http://localhost/*/file_sample.html"],
        js: [{ code: `(${textScriptCodeIdle})()` }],
        runAt: "document_idle",
      },
      // Extension URLs.
      {
        matches: ["http://localhost/*/file_sample.html"],
        js: [{ file: "content_script_start.js" }],
        runAt: "document_start",
      },
      {
        matches: ["http://localhost/*/file_sample.html"],
        js: [{ file: "content_script_end.js" }],
        runAt: "document_end",
      },
      {
        matches: ["http://localhost/*/file_sample.html"],
        js: [{ file: "content_script_idle.js" }],
        runAt: "document_idle",
      },
      {
        matches: ["http://localhost/*/file_sample.html"],
        js: [{ file: "content_script.js" }],
        // "runAt" is not specified here to ensure that it defaults to document_idle when missing.
      },
    ];

    const expectedAPIs = ["unregister"];

    for (const scriptOptions of content_scripts) {
      const script = await browser.contentScripts.register(scriptOptions);
      const actualAPIs = Object.keys(script);

      browser.test.assertEq(
        JSON.stringify(expectedAPIs),
        JSON.stringify(actualAPIs),
        `Got a script API object for ${scriptOptions.js[0]}`
      );
    }

    browser.test.sendMessage("background-ready");
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
    void manifest.permissions;
    browser.runtime.sendMessage(["chrome-namespace-ok"]);
  }

  let extensionData = {
    manifest: {
      permissions: ["http://localhost/*/file_sample.html"],
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
      resolve();
    });
  });

  let chromeNamespacePromise = extension.awaitMessage("chrome-namespace-ok");

  // Ensure that a content page running in a content process and which has been
  // already loaded when the content scripts has been registered, it has received
  // and registered the expected content scripts.
  let contentPage = await ExtensionTestUtils.loadContentPage(`about:blank`);

  await extension.startup();
  await extension.awaitMessage("background-ready");

  await contentPage.loadURL(`${BASE_URL}/file_sample.html`);

  await Promise.all([completePromise, chromeNamespacePromise]);

  await contentPage.close();

  // Expect two content scripts to run (one registered using an extension URL
  // and one registered from plain JS code).
  equal(loadingCount, 2, "document_start script ran exactly twice");
  equal(interactiveCount, 2, "document_end script ran exactly twice");
  equal(completeCount, 2, "document_idle script ran exactly twice");

  await extension.unload();
});

// Test that the contentScript.register options are correctly translated
// into the expected WebExtensionContentScript properties.
add_task(async function test_contentscripts_register_all_options() {
  async function background() {
    await browser.contentScripts.register({
      js: [{ file: "content_script.js" }],
      css: [{ file: "content_style.css" }],
      matches: ["http://localhost/*"],
      excludeMatches: ["http://localhost/exclude/*"],
      excludeGlobs: ["*_exclude.html"],
      includeGlobs: ["*_include.html"],
      allFrames: true,
      matchAboutBlank: true,
      runAt: "document_start",
    });

    browser.test.sendMessage("background-ready", window.location.origin);
  }

  const extensionData = {
    manifest: {
      permissions: ["http://localhost/*"],
    },
    background,

    files: {
      "content_script.js": "",
      "content_style.css": "",
    },
  };

  const extension = ExtensionTestUtils.loadExtension(extensionData);

  await extension.startup();

  const baseExtURL = await extension.awaitMessage("background-ready");

  const policy = WebExtensionPolicy.getByID(extension.id);

  ok(policy, "Got the WebExtensionPolicy for the test extension");
  equal(
    policy.contentScripts.length,
    1,
    "Got the expected number of registered content scripts"
  );

  const script = policy.contentScripts[0];
  let { allFrames, cssPaths, jsPaths, matchAboutBlank, runAt } = script;

  deepEqual(
    {
      allFrames,
      cssPaths,
      jsPaths,
      matchAboutBlank,
      runAt,
    },
    {
      allFrames: true,
      cssPaths: [`${baseExtURL}/content_style.css`],
      jsPaths: [`${baseExtURL}/content_script.js`],
      matchAboutBlank: true,
      runAt: "document_start",
    },
    "Got the expected content script properties"
  );

  ok(
    script.matchesURI(Services.io.newURI("http://localhost/ok_include.html")),
    "matched and include globs should match"
  );
  ok(
    !script.matchesURI(
      Services.io.newURI("http://localhost/exclude/ok_include.html")
    ),
    "exclude matches should not match"
  );
  ok(
    !script.matchesURI(Services.io.newURI("http://localhost/ok_exclude.html")),
    "exclude globs should not match"
  );

  await extension.unload();
});
