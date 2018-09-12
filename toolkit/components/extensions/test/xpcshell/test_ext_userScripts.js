"use strict";

const {
  createAppInfo,
} = AddonTestUtils;

AddonTestUtils.init(this);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "49");

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

add_task(async function setup_optional_permission_observer() {
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
      }

      browser.test.sendMessage(`${msg}:done`);
    });

    browser.test.sendMessage("background-ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["http://localhost/*"],
      optional_permissions: ["<all_urls>"],
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
add_task(async function test_userScripts_no_webext_apis() {
  async function background() {
    const matches = ["http://localhost/*/file_sample.html"];

    const script = await browser.userScripts.register({
      js: [{
        code: `
          const webextAPINamespaces = this.browser ? Object.keys(this.browser) : undefined;
          document.body.innerHTML = "userScript loaded - " + JSON.stringify(webextAPINamespaces);
        `,
      }],
      runAt: "document_end",
      matches,
      scriptMetadata: {
        name: "test-user-script",
        arrayToMatch: ["el1"],
        objectToMatch: {nestedProp: "nestedValue"},
      },
    });

    const scriptToRemove = await browser.userScripts.register({
      js: [{
        code: 'document.body.innerHTML = "unexpected unregistered userScript loaded";',
      }],
      runAt: "document_end",
      matches,
      scriptMetadata: {
        name: "user-script-to-remove",
      },
    });

    browser.test.assertTrue("unregister" in script,
                            "Got an unregister method on the userScript API object");

    // Remove the last registered user script.
    await scriptToRemove.unregister();

    await browser.contentScripts.register({
      js: [{
        code: `
          browser.test.sendMessage("page-loaded", {
            textContent: document.body.textContent,
            url: window.location.href,
          }); true;
        `,
      }],
      matches,
    });

    browser.test.sendMessage("background-ready");
  }

  let extensionData = {
    manifest: {
      permissions: [
        "http://localhost/*/file_sample.html",
      ],
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
  const reply = await extension.awaitMessage("page-loaded");
  Assert.deepEqual(reply, {
    textContent: "userScript loaded - undefined",
    url,
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
    // Load an url that matches and check that the userScripts has been loaded.
    const reply2 = await extension.awaitMessage("page-loaded");
    Assert.deepEqual(reply2, {
      textContent: "userScript loaded - undefined",
      url: url2,
    }, "The userScript executed on the expected url and no access to the WebExtensions APIs");
    await contentPage2.close();
  }

  await extension.unload();
});
