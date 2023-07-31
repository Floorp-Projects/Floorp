"use strict";

const PROCESS_COUNT_PREF = "dom.ipc.processCount";

const { createAppInfo } = AddonTestUtils;

AddonTestUtils.init(this);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "49");

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

add_task(async function setup_test_environment() {
  if (ExtensionTestUtils.remoteContentScripts) {
    // Start with one content process so that we can increase the number
    // later and test the behavior of a fresh content process.
    Services.prefs.setIntPref(PROCESS_COUNT_PREF, 1);
  }

  // Grant the optional permissions requested, without prompting.
  Services.prefs.setBoolPref(
    "extensions.webextOptionalPermissionPrompts",
    false
  );
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("extensions.webextOptionalPermissionPrompts");
  });
});

// Test that there is no userScripts API namespace when the manifest doesn't include a user_scripts
// property.
add_task(async function test_userScripts_manifest_property_required() {
  function background() {
    browser.test.assertEq(
      undefined,
      browser.userScripts,
      "userScripts API namespace should be undefined in the extension page"
    );
    browser.test.sendMessage("background-page:done");
  }

  async function contentScript() {
    browser.test.assertEq(
      undefined,
      browser.userScripts,
      "userScripts API namespace should be undefined in the content script"
    );
    browser.test.sendMessage("content-script:done");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["http://*/*/file_sample.html"],
      content_scripts: [
        {
          matches: ["http://*/*/file_sample.html"],
          js: ["content_script.js"],
          run_at: "document_start",
        },
      ],
    },
    files: {
      "content_script.js": contentScript,
    },
  });

  await extension.startup();
  await extension.awaitMessage("background-page:done");

  let url = `${BASE_URL}/file_sample.html`;
  let contentPage = await ExtensionTestUtils.loadContentPage(url);

  await extension.awaitMessage("content-script:done");

  await extension.unload();
  await contentPage.close();
});

// Test that userScripts can only matches origins that are subsumed by the extension permissions,
// and that more origins can be allowed by requesting an optional permission.
add_task(async function test_userScripts_matches_denied() {
  async function background() {
    async function registerUserScriptWithMatches(matches) {
      const scripts = await browser.userScripts.register({
        js: [{ code: "" }],
        matches,
      });
      await scripts.unregister();
    }

    // These matches are supposed to be denied until the extension has been granted the
    // <all_urls> origin permission.
    const testMatches = [
      "<all_urls>",
      "file://*/*",
      "https://localhost/*",
      "http://example.com/*",
    ];

    browser.test.onMessage.addListener(async msg => {
      if (msg === "test-denied-matches") {
        for (let testMatch of testMatches) {
          await browser.test.assertRejects(
            registerUserScriptWithMatches([testMatch]),
            /Permission denied to register a user script for/,
            "Got the expected rejection when the extension permission does not subsume the userScript matches"
          );
        }
      } else if (msg === "grant-all-urls") {
        await browser.permissions.request({ origins: ["<all_urls>"] });
      } else if (msg === "test-allowed-matches") {
        for (let testMatch of testMatches) {
          try {
            await registerUserScriptWithMatches([testMatch]);
          } catch (err) {
            browser.test.fail(
              `Unexpected rejection ${err} on matching ${JSON.stringify(
                testMatch
              )}`
            );
          }
        }
      } else {
        browser.test.fail(`Received an unexpected ${msg} test message`);
      }

      browser.test.sendMessage(`${msg}:done`);
    });

    browser.test.sendMessage("background-ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["http://localhost/*"],
      optional_permissions: ["<all_urls>"],
      user_scripts: {},
    },
    background,
  });

  await extension.startup();

  await extension.awaitMessage("background-ready");

  // Test that the matches not subsumed by the extension permissions are being denied.
  extension.sendMessage("test-denied-matches");
  await extension.awaitMessage("test-denied-matches:done");

  // Grant the optional <all_urls> permission.
  await withHandlingUserInput(extension, async () => {
    extension.sendMessage("grant-all-urls");
    await extension.awaitMessage("grant-all-urls:done");
  });

  // Test that all the matches are now subsumed by the extension permissions.
  extension.sendMessage("test-allowed-matches");
  await extension.awaitMessage("test-allowed-matches:done");

  await extension.unload();
});

// Test that userScripts sandboxes:
// - can be registered/unregistered from an extension page (and they are registered on both new and
//   existing processes).
// - have no WebExtensions APIs available
// - are able to access the target window and document
add_task(async function test_userScripts_no_webext_apis() {
  async function background() {
    const matches = ["http://localhost/*/file_sample.html*"];

    const sharedCode = {
      code: 'console.log("js code shared by multiple userScripts");',
    };

    const userScriptOptions = {
      js: [
        sharedCode,
        {
          code: `
          window.addEventListener("load", () => {
            const webextAPINamespaces = this.browser ? Object.keys(this.browser) : undefined;
            document.body.innerHTML = "userScript loaded - " + JSON.stringify(webextAPINamespaces);
          }, {once: true});
        `,
        },
      ],
      runAt: "document_start",
      matches,
      scriptMetadata: {
        name: "test-user-script",
        arrayProperty: ["el1"],
        objectProperty: { nestedProp: "nestedValue" },
        nullProperty: null,
      },
    };

    let script = await browser.userScripts.register(userScriptOptions);

    // Unregister and then register the same js code again, to verify that the last registered
    // userScript doesn't get assigned a revoked blob url (otherwise Extensioncontent.jsm
    // ScriptCache raises an error because it fails to compile the revoked blob url and the user
    // script will never be loaded).
    script.unregister();
    script = await browser.userScripts.register(userScriptOptions);

    browser.test.onMessage.addListener(async msg => {
      if (msg !== "register-new-script") {
        return;
      }

      await script.unregister();
      await browser.userScripts.register({
        ...userScriptOptions,
        scriptMetadata: { name: "test-new-script" },
        js: [
          sharedCode,
          {
            code: `
          window.addEventListener("load", () => {
            const webextAPINamespaces = this.browser ? Object.keys(this.browser) : undefined;
            document.body.innerHTML = "new userScript loaded - " + JSON.stringify(webextAPINamespaces);
          }, {once: true});
        `,
          },
        ],
      });

      browser.test.sendMessage("script-registered");
    });

    const scriptToRemove = await browser.userScripts.register({
      js: [
        sharedCode,
        {
          code: `
          window.addEventListener("load", () => {
            document.body.innerHTML = "unexpected unregistered userScript loaded";
          }, {once: true});
        `,
        },
      ],
      runAt: "document_start",
      matches,
      scriptMetadata: {
        name: "user-script-to-remove",
      },
    });

    browser.test.assertTrue(
      "unregister" in script,
      "Got an unregister method on the userScript API object"
    );

    // Remove the last registered user script.
    await scriptToRemove.unregister();

    browser.test.sendMessage("background-ready");
  }

  let extensionData = {
    manifest: {
      permissions: ["http://localhost/*/file_sample.html"],
      user_scripts: {},
    },
    background,
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);

  await extension.startup();

  await extension.awaitMessage("background-ready");

  let url = `${BASE_URL}/file_sample.html?testpage=1`;
  let contentPage = await ExtensionTestUtils.loadContentPage(
    url,
    ExtensionTestUtils.remoteContentScripts ? { remote: true } : undefined
  );
  let result = await contentPage.spawn([], async () => {
    return {
      textContent: this.content.document.body.textContent,
      url: this.content.location.href,
      readyState: this.content.document.readyState,
    };
  });
  Assert.deepEqual(
    result,
    {
      textContent: "userScript loaded - undefined",
      url,
      readyState: "complete",
    },
    "The userScript executed on the expected url and no access to the WebExtensions APIs"
  );

  // If the tests is running with "remote content process" mode, test that the userScript
  // are being correctly registered in newly created processes (received as part of the sharedData).
  if (ExtensionTestUtils.remoteContentScripts) {
    info(
      "Test content script are correctly created on a newly created process"
    );

    await extension.sendMessage("register-new-script");
    await extension.awaitMessage("script-registered");

    // Update the process count preference, so that we can test that the newly registered user script
    // is propagated as expected into the newly created process.
    Services.prefs.setIntPref(PROCESS_COUNT_PREF, 2);

    const url2 = `${BASE_URL}/file_sample.html?testpage=2`;
    let contentPage2 = await ExtensionTestUtils.loadContentPage(url2, {
      remote: true,
    });
    let result2 = await contentPage2.spawn([], async () => {
      return {
        textContent: this.content.document.body.textContent,
        url: this.content.location.href,
        readyState: this.content.document.readyState,
      };
    });
    Assert.deepEqual(
      result2,
      {
        textContent: "new userScript loaded - undefined",
        url: url2,
        readyState: "complete",
      },
      "The userScript executed on the expected url and no access to the WebExtensions APIs"
    );

    await contentPage2.close();
  }

  await contentPage.close();

  await extension.unload();
});

// This test verify that a cached script is still able to catch the document
// while it is still loading (when we do not block the document parsing as
// we do for a non cached script).
add_task(async function test_cached_userScript_on_document_start() {
  function apiScript() {
    browser.userScripts.onBeforeScript.addListener(script => {
      script.defineGlobals({
        sendTestMessage(name, params) {
          return browser.test.sendMessage(name, params);
        },
      });
    });
  }

  async function background() {
    function userScript() {
      this.sendTestMessage("user-script-loaded", {
        url: window.location.href,
        documentReadyState: document.readyState,
      });
    }

    await browser.userScripts.register({
      js: [
        {
          code: `(${userScript})();`,
        },
      ],
      runAt: "document_start",
      matches: ["http://localhost/*/file_sample.html"],
    });

    browser.test.sendMessage("user-script-registered");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["http://localhost/*/file_sample.html"],
      user_scripts: {
        api_script: "api-script.js",
        // The following is an unexpected manifest property, that we expect to be ignored and
        // to not prevent the test extension from being installed and run as expected.
        unexpected_manifest_key: "test-unexpected-key",
      },
    },
    background,
    files: {
      "api-script.js": apiScript,
    },
  });

  ExtensionTestUtils.failOnSchemaWarnings(false);
  await extension.startup();
  ExtensionTestUtils.failOnSchemaWarnings(true);
  await extension.awaitMessage("user-script-registered");

  let url = `${BASE_URL}/file_sample.html`;
  let contentPage = await ExtensionTestUtils.loadContentPage(url);

  let msg = await extension.awaitMessage("user-script-loaded");
  Assert.deepEqual(
    msg,
    {
      url,
      documentReadyState: "loading",
    },
    "Got the expected url and document.readyState from a non cached user script"
  );

  // Reload the page and check that the cached content script is still able to
  // run on document_start.
  await contentPage.loadURL(url);

  let msgFromCached = await extension.awaitMessage("user-script-loaded");
  Assert.deepEqual(
    msgFromCached,
    {
      url,
      documentReadyState: "loading",
    },
    "Got the expected url and document.readyState from a cached user script"
  );

  await contentPage.close();
  await extension.unload();
});

add_task(async function test_userScripts_pref_disabled() {
  async function run_userScript_on_pref_disabled_test() {
    async function background() {
      let promise = (async () => {
        await browser.userScripts.register({
          js: [
            {
              code: "throw new Error('This userScripts should not be registered')",
            },
          ],
          runAt: "document_start",
          matches: ["<all_urls>"],
        });
      })();

      await browser.test.assertRejects(
        promise,
        /userScripts APIs are currently experimental/,
        "Got the expected error from userScripts.register when the userScripts API is disabled"
      );

      browser.test.sendMessage("background-page:done");
    }

    async function contentScript() {
      let promise = (async () => {
        browser.userScripts.onBeforeScript.addListener(() => {});
      })();
      await browser.test.assertRejects(
        promise,
        /userScripts APIs are currently experimental/,
        "Got the expected error from userScripts.onBeforeScript when the userScripts API is disabled"
      );

      browser.test.sendMessage("content-script:done");
    }

    let extension = ExtensionTestUtils.loadExtension({
      background,
      manifest: {
        permissions: ["http://*/*/file_sample.html"],
        user_scripts: { api_script: "" },
        content_scripts: [
          {
            matches: ["http://*/*/file_sample.html"],
            js: ["content_script.js"],
            run_at: "document_start",
          },
        ],
      },
      files: {
        "content_script.js": contentScript,
      },
    });

    await extension.startup();

    await extension.awaitMessage("background-page:done");

    let url = `${BASE_URL}/file_sample.html`;
    let contentPage = await ExtensionTestUtils.loadContentPage(url);

    await extension.awaitMessage("content-script:done");

    await extension.unload();
    await contentPage.close();
  }

  await runWithPrefs(
    [["extensions.webextensions.userScripts.enabled", false]],
    run_userScript_on_pref_disabled_test
  );
});

// This test verify that userScripts.onBeforeScript API Event is not available without
// a "user_scripts.api_script" property in the manifest.
add_task(async function test_user_script_api_script_required() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://localhost/*/file_sample.html"],
          js: ["content_script.js"],
          run_at: "document_start",
        },
      ],
      user_scripts: {},
    },
    files: {
      "content_script.js": function () {
        browser.test.assertEq(
          undefined,
          browser.userScripts && browser.userScripts.onBeforeScript,
          "Got an undefined onBeforeScript property as expected"
        );
        browser.test.sendMessage("no-onBeforeScript:done");
      },
    },
  });

  await extension.startup();

  let url = `${BASE_URL}/file_sample.html`;
  let contentPage = await ExtensionTestUtils.loadContentPage(url);

  await extension.awaitMessage("no-onBeforeScript:done");

  await extension.unload();
  await contentPage.close();
});

add_task(async function test_scriptMetaData() {
  function getTestCases(isUserScriptsRegister) {
    return [
      // When scriptMetadata is not set (or undefined), it is treated as if it were null.
      // In the API script, the metadata is then expected to be null.
      isUserScriptsRegister ? undefined : null,

      // Falsey
      null,
      "",
      false,
      0,

      // Truthy
      true,
      1,
      "non-empty string",

      // Objects
      ["some array with value"],
      { "some object": "with value" },
    ];
  }

  async function background() {
    for (let scriptMetadata of getTestCases(true)) {
      await browser.userScripts.register({
        js: [{ file: "userscript.js" }],
        runAt: "document_end",
        matches: ["http://localhost/*/file_sample.html"],
        scriptMetadata,
      });
    }

    browser.test.sendMessage("background-page:done");
  }

  function apiScript() {
    let testCases = getTestCases(false);
    let i = 0;

    browser.userScripts.onBeforeScript.addListener(script => {
      script.defineGlobals({
        checkMetadata() {
          let expectation = testCases[i];
          let metadata = script.metadata;
          if (typeof expectation === "object" && expectation !== null) {
            // Non-primitive values cannot be compared with assertEq,
            // so serialize both and just verify that they are equal.
            expectation = JSON.stringify(expectation);
            metadata = JSON.stringify(script.metadata);
          }

          browser.test.assertEq(
            expectation,
            metadata,
            `Expected metadata at call ${i}`
          );
          if (++i === testCases.length) {
            browser.test.sendMessage("apiscript:done");
          }
        },
      });
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `${getTestCases};(${background})()`,
    manifest: {
      permissions: ["http://*/*/file_sample.html"],
      user_scripts: {
        api_script: "apiscript.js",
      },
    },
    files: {
      "apiscript.js": `${getTestCases};(${apiScript})()`,
      "userscript.js": "checkMetadata();",
    },
  });

  await extension.startup();

  await extension.awaitMessage("background-page:done");

  const pageUrl = `${BASE_URL}/file_sample.html`;
  info(`Load content page: ${pageUrl}`);
  const page = await ExtensionTestUtils.loadContentPage(pageUrl);

  await extension.awaitMessage("apiscript:done");

  await page.close();

  await extension.unload();
});

add_task(async function test_userScriptOptions_js_property_required() {
  function background() {
    const userScriptOptions = {
      runAt: "document_start",
      matches: ["http://*/*/file_sample.html"],
    };

    browser.test.assertThrows(
      () => browser.userScripts.register(userScriptOptions),
      /Type error for parameter userScriptOptions \(Property \"js\" is required\)/,
      "Got the expected error from userScripts.register when js property is missing"
    );

    browser.test.sendMessage("done");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["http://*/*/file_sample.html"],
      user_scripts: {},
    },
  });

  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
});

add_task(async function test_userScripts_are_unregistered_on_unload() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["http://*/*/file_sample.html"],
      user_scripts: {
        api_script: "api_script.js",
      },
    },
    files: {
      "userscript.js": "",
      "extpage.html": `<!DOCTYPE html><script src="extpage.js"></script>`,
      "extpage.js": async function extPage() {
        await browser.userScripts.register({
          js: [{ file: "userscript.js" }],
          matches: ["http://localhost/*/file_sample.html"],
        });

        browser.test.sendMessage("user-script-registered");
      },
    },
  });

  await extension.startup();

  equal(
    // In order to read the `registeredContentScripts` map, we need to access
    // the extension embedded in the `ExtensionWrapper` first.
    extension.extension.registeredContentScripts.size,
    0,
    "no user scripts registered yet"
  );

  const url = `moz-extension://${extension.uuid}/extpage.html`;
  info(`loading extension page: ${url}`);
  const page = await ExtensionTestUtils.loadContentPage(url);

  info("waiting for the user script to be registered");
  await extension.awaitMessage("user-script-registered");

  equal(
    extension.extension.registeredContentScripts.size,
    1,
    "got registered user scripts in the extension content scripts map"
  );

  await page.close();

  equal(
    extension.extension.registeredContentScripts.size,
    0,
    "user scripts unregistered from the extension content scripts map"
  );

  await extension.unload();
});
