"use strict";

const {
  createAppInfo,
} = AddonTestUtils;

AddonTestUtils.init(this);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "49");

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

add_task(async function setup_test_environment() {
  // Grant the optional permissions requested.
  function permissionObserver(subject, topic, data) {
    if (topic == "webextension-optional-permission-prompt") {
      let {resolve} = subject.wrappedJSObject;
      resolve(true);
    }
  }
  Services.obs.addObserver(permissionObserver, "webextension-optional-permission-prompt");
  registerCleanupFunction(() => {
    Services.obs.removeObserver(permissionObserver, "webextension-optional-permission-prompt");
  });

  // Turn on the userScripts API using the related pref.
  Services.prefs.setBoolPref("extensions.webextensions.userScripts.enabled", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("extensions.webextensions.userScripts.enabled");
  });
});

// Test that there is no userScripts API namespace when the manifest doesn't include a user_scripts
// property.
add_task(async function test_userScripts_manifest_property_required() {
  function background() {
    browser.test.assertEq(undefined, browser.userScripts,
                          "userScripts API namespace should be undefined in the extension page");
    browser.test.sendMessage("background-page:done");
  }

  async function contentScript() {
    browser.test.assertEq(undefined, browser.userScripts,
                          "userScripts API namespace should be undefined in the content script");
    browser.test.sendMessage("content-script:done");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["http://*/*/file_sample.html"],
      content_scripts: [
        {
          matches:  ["http://*/*/file_sample.html"],
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
        js: [{code: ""}],
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
            "Got the expected rejection when the extension permission does not subsume the userScript matches");
        }
      } else if (msg === "grant-all-urls") {
        await browser.permissions.request({origins: ["<all_urls>"]});
      } else if (msg === "test-allowed-matches") {
        for (let testMatch of testMatches) {
          try {
            await registerUserScriptWithMatches([testMatch]);
          } catch (err) {
            browser.test.fail(`Unexpected rejection ${err} on matching ${JSON.stringify(testMatch)}`);
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
// - can be registered/unregistered from an extension page
// - have no WebExtensions APIs available
// - are able to access the target window and document

// Temporarily disabled due to bug 1498364
false && add_task(async function test_userScripts_no_webext_apis() {
  async function background() {
    const matches = ["http://localhost/*/file_sample.html"];

    const sharedCode = {code: "console.log(\"js code shared by multiple userScripts\");"};

    let script = await browser.userScripts.register({
      js: [sharedCode, {
        code: `
          window.addEventListener("load", () => {
            const webextAPINamespaces = this.browser ? Object.keys(this.browser) : undefined;
            document.body.innerHTML = "userScript loaded - " + JSON.stringify(webextAPINamespaces);
          }, {once: true});
        `,
      }],
      runAt: "document_start",
      matches,
      scriptMetadata: {
        name: "test-user-script",
        arrayProperty: ["el1"],
        objectProperty: {nestedProp: "nestedValue"},
        nullProperty: null,
      },
    });

    // Unregister and then register the same js code again, to verify that the last registered
    // userScript doesn't get assigned a revoked blob url (otherwise Extensioncontent.jsm
    // ScriptCache raises an error because it fails to compile the revoked blob url and the user
    // script will never be loaded).
    script.unregister();
    script = await browser.userScripts.register({
      js: [sharedCode, {
        code: `
          window.addEventListener("load", () => {
            const webextAPINamespaces = this.browser ? Object.keys(this.browser) : undefined;
            document.body.innerHTML = "userScript loaded - " + JSON.stringify(webextAPINamespaces);
          }, {once: true});
        `,
      }],
      runAt: "document_start",
      matches,
      scriptMetadata: {
        name: "test-user-script",
        arrayProperty: ["el1"],
        objectProperty: {nestedProp: "nestedValue"},
        nullProperty: null,
      },
    });

    const scriptToRemove = await browser.userScripts.register({
      js: [sharedCode, {
        code: `
          window.addEventListener("load", () => {
            document.body.innerHTML = "unexpected unregistered userScript loaded";
          }, {once: true});
        `,
      }],
      runAt: "document_start",
      matches,
      scriptMetadata: {
        name: "user-script-to-remove",
      },
    });

    browser.test.assertTrue("unregister" in script,
                            "Got an unregister method on the userScript API object");

    // Remove the last registered user script.
    await scriptToRemove.unregister();

    browser.test.sendMessage("background-ready");
  }

  let extensionData = {
    manifest: {
      permissions: [
        "http://localhost/*/file_sample.html",
      ],
      user_scripts: {},
    },
    background,
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);

  await extension.startup();

  await extension.awaitMessage("background-ready");

  // Test in an existing process (where the registered userScripts has been received from the
  // Extension:RegisterContentScript message sent to all the processes).
  info("Test content script loaded in a process created before any registered userScript");
  let url = `${BASE_URL}/file_sample.html#remote-false`;
  let contentPage = await ExtensionTestUtils.loadContentPage(url, {remote: false});
  let result = await contentPage.spawn(undefined, async () => {
    return {
      textContent: this.content.document.body.textContent,
      url: this.content.location.href,
      readyState: this.content.document.readyState,
    };
  });
  Assert.deepEqual(result, {
    textContent: "userScript loaded - undefined",
    url,
    readyState: "complete",
  }, "The userScript executed on the expected url and no access to the WebExtensions APIs");
  await contentPage.close();

  // Test in a new process (where the registered userScripts has to be retrieved from the extension
  // representation from the shared memory data).
  // NOTE: this part is currently skipped on Android, where e10s content is not yet supported and
  // the xpcshell test crash when we create contentPage2 with `remote = true`.
  if (ExtensionTestUtils.remoteContentScripts) {
    info("Test content script loaded in a process created after the userScript has been registered");
    let url2 = `${BASE_URL}/file_sample.html#remote-true`;
    let contentPage2 = await ExtensionTestUtils.loadContentPage(url2, {remote: true});
    let result2 = await contentPage2.spawn(undefined, async () => {
      return {
        textContent: this.content.document.body.textContent,
        url: this.content.location.href,
        readyState: this.content.document.readyState,
      };
    });
    Assert.deepEqual(result2, {
      textContent: "userScript loaded - undefined",
      url: url2,
      readyState: "complete",
    }, "The userScript executed on the expected url and no access to the WebExtensions APIs");
    await contentPage2.close();
  }

  await extension.unload();
});

add_task(async function test_userScripts_exported_APIs() {
  async function background() {
    const matches = ["http://localhost/*/file_sample.html"];

    await browser.runtime.onMessage.addListener(async (msg, sender) => {
      return {bgPageReply: true};
    });

    async function userScript() {
      // Explicitly retrieve the custom exported API methods
      // to prevent eslint to raise a no-undef validation
      // error for them.
      const {
        US_sync_api,
        US_async_api_with_callback,
        US_send_api_results,
      } = this;
      this.userScriptGlobalVar = "global-sandbox-value";

      const syncAPIResult = US_sync_api("param1", "param2");
      const cb = (cbParam) => {
        return `callback param: ${JSON.stringify(cbParam)}`;
      };
      const cb2 = cb;
      const asyncAPIResult = await US_async_api_with_callback("param3", cb, cb2);

      let expectedError;

      // This is expect to raise an exception due to the window parameter which can't
      // be cloned.
      try {
        US_sync_api(window);
      } catch (err) {
        expectedError = err.message;
      }

      US_send_api_results({syncAPIResult, asyncAPIResult, expectedError});
    }

    await browser.userScripts.register({
      js: [{
        code: `(${userScript})();`,
      }],
      runAt: "document_end",
      matches,
      scriptMetadata: {
        name: "test-user-script-exported-apis",
        arrayProperty: ["el1"],
        objectProperty: {nestedProp: "nestedValue"},
        nullProperty: null,
      },
    });

    browser.test.sendMessage("background-ready");
  }

  function apiScript() {
    // Redefine Promise and Error globals to verify that it doesn't break the WebExtensions internals
    // that are going to use them.
    this.Promise = {};
    this.Error = {};

    browser.userScripts.setScriptAPIs({
      US_sync_api([param1, param2], scriptMetadata, scriptGlobal) {
        browser.test.assertEq("test-user-script-exported-apis", scriptMetadata.name,
                              "Got the expected value for a string scriptMetadata property");
        browser.test.assertEq(null, scriptMetadata.nullProperty,
                              "Got the expected value for a null scriptMetadata property");
        browser.test.assertTrue(scriptMetadata.arrayProperty &&
                                scriptMetadata.arrayProperty.length === 1 &&
                                scriptMetadata.arrayProperty[0] === "el1",
                                "Got the expected value for an array scriptMetadata property");
        browser.test.assertTrue(scriptMetadata.objectProperty &&
                                scriptMetadata.objectProperty.nestedProp === "nestedValue",
                                "Got the expected value for an object scriptMetadata property");

        browser.test.assertEq("param1", param1, "Got the expected parameter value");
        browser.test.assertEq("param2", param2, "Got the expected parameter value");

        browser.test.sendMessage("US_sync_api", {param1, param2});

        return "returned_value";
      },
      async US_async_api_with_callback([param, cb, cb2], scriptMetadata, scriptGlobal) {
        browser.test.assertEq("function", typeof cb, "Got a callback function parameter");
        browser.test.assertTrue(cb === cb2, "Got the same cloned function for the same function parameter");

        browser.runtime.sendMessage({param}).then(bgPageRes => {
          // eslint-disable-next-line no-undef
          const cbResult = cb(cloneInto(bgPageRes, scriptGlobal));
          browser.test.sendMessage("US_async_api_with_callback", cbResult);
        });

        return "resolved_value";
      },
      async US_send_api_results([results], scriptMetadata, scriptGlobal) {
        browser.test.sendMessage("US_send_api_results", results);
      },
    });
  }

  let extensionData = {
    manifest: {
      permissions: [
        "http://localhost/*/file_sample.html",
      ],
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
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);

  // Ensure that a content page running in a content process and which has been
  // already loaded when the content scripts has been registered, it has received
  // and registered the expected content scripts.
  let contentPage = await ExtensionTestUtils.loadContentPage(`about:blank`);

  await extension.startup();

  await extension.awaitMessage("background-ready");

  await contentPage.loadURL(`${BASE_URL}/file_sample.html`);

  info("Wait the userScript to call the exported US_sync_api method");
  await extension.awaitMessage("US_sync_api");

  info("Wait the userScript to call the exported US_async_api_with_callback method");
  const userScriptCallbackResult = await extension.awaitMessage("US_async_api_with_callback");
  equal(userScriptCallbackResult, `callback param: {"bgPageReply":true}`,
        "Got the expected results when the userScript callback has been called");

  info("Wait the userScript to call the exported US_send_api_results method");
  const userScriptsAPIResults = await extension.awaitMessage("US_send_api_results");
  Assert.deepEqual(userScriptsAPIResults, {
    syncAPIResult: "returned_value",
    asyncAPIResult: "resolved_value",
    expectedError: "Only serializable parameters are supported",
  }, "Got the expected userScript API results");

  await extension.unload();

  await contentPage.close();
});

// This test verify that a cached script is still able to catch the document
// while it is still loading (when we do not block the document parsing as
// we do for a non cached script).
add_task(async function test_cached_userScript_on_document_start() {
  function apiScript() {
    browser.userScripts.setScriptAPIs({
      sendTestMessage([name, params]) {
        return browser.test.sendMessage(name, params);
      },
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
      js: [{
        code: `(${userScript})();`,
      }],
      runAt: "document_start",
      matches: [
        "http://localhost/*/file_sample.html",
      ],
    });

    browser.test.sendMessage("user-script-registered");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: [
        "http://localhost/*/file_sample.html",
      ],
      user_scripts: {
        api_script: "api-script.js",
      },
    },
    background,
    files: {
      "api-script.js": apiScript,
    },
  });

  await extension.startup();
  await extension.awaitMessage("user-script-registered");

  let url = `${BASE_URL}/file_sample.html`;
  let contentPage = await ExtensionTestUtils.loadContentPage(url);

  let msg = await extension.awaitMessage("user-script-loaded");
  Assert.deepEqual(msg, {
    url,
    documentReadyState: "loading",
  }, "Got the expected url and document.readyState from a non cached user script");

  // Reload the page and check that the cached content script is still able to
  // run on document_start.
  await contentPage.loadURL(url);

  let msgFromCached = await extension.awaitMessage("user-script-loaded");
  Assert.deepEqual(msgFromCached, {
    url,
    documentReadyState: "loading",
  }, "Got the expected url and document.readyState from a cached user script");

  await contentPage.close();
  await extension.unload();
});

add_task(async function test_userScripts_pref_disabled() {
  async function run_userScript_on_pref_disabled_test() {
    async function background() {
      let promise = (async () => {
        await browser.userScripts.register({
          js: [
            {code: "throw new Error('This userScripts should not be registered')"},
          ],
          runAt: "document_start",
          matches: ["<all_urls>"],
        });
      })();

      await browser.test.assertRejects(
        promise,
        /userScripts APIs are currently experimental/,
        "Got the expected error from userScripts.register when the userScripts API is disabled");

      browser.test.sendMessage("background-page:done");
    }

    async function contentScript() {
      let promise = (async () => {
        browser.userScripts.setScriptAPIs({
          GM_apiMethod() {},
        });
      })();
      await browser.test.assertRejects(
        promise,
        /userScripts APIs are currently experimental/,
        "Got the expected error from userScripts.setScriptAPIs when the userScripts API is disabled");

      browser.test.sendMessage("content-script:done");
    }

    let extension = ExtensionTestUtils.loadExtension({
      background,
      manifest: {
        permissions: ["http://*/*/file_sample.html"],
        user_scripts: {},
        content_scripts: [
          {
            matches:  ["http://*/*/file_sample.html"],
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

  await runWithPrefs([["extensions.webextensions.userScripts.enabled", false]],
                     run_userScript_on_pref_disabled_test);
});
