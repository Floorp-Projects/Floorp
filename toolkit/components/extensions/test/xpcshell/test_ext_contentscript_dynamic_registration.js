"use strict";

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

// ExtensionContent.jsm needs to know when it's running from xpcshell, to use
// the right timeout for content scripts executed at document_idle.
ExtensionTestUtils.mockAppInfo();

// Do not use preallocated processes.
Services.prefs.setBoolPref("dom.ipc.processPrelaunch.enabled", false);

const makeExtension = ({ background, manifest }) => {
  return ExtensionTestUtils.loadExtension({
    manifest: {
      ...manifest,
      permissions:
        manifest.manifest_version === 3 ? ["scripting"] : ["http://*/*/*.html"],
    },
    background,
    files: {
      "script.js": () => {
        browser.test.sendMessage(
          `script-ran: ${location.pathname.split("/").pop()}`
        );
      },
      "inject_browser.js": () => {
        browser.userScripts.onBeforeScript.addListener(script => {
          // Inject `browser.test.sendMessage()` so that it can be used in the
          // `script.js` defined above when using "user scripts".
          script.defineGlobals({
            browser: {
              test: {
                sendMessage(msg) {
                  browser.test.sendMessage(msg);
                },
              },
            },
          });
        });
      },
    },
  });
};

const verifyRegistrationWithNewProcess = async extension => {
  // We override the `broadcast()` method to reliably verify Bug 1756495: when
  // a new process is spawned while we register a content script, the script
  // should be correctly registered and executed in this new process. Below,
  // when we receive the `Extension:RegisterContentScripts`, we open a new tab
  // (which is the "new process") and then we invoke the original "broadcast
  // logic". The expected result is that the content script registered by the
  // extension will run.
  const originalBroadcast = Extension.prototype.broadcast;

  let broadcastCalledCount = 0;
  let secondContentPage;

  extension.extension.broadcast = async function broadcast(msg, data) {
    if (msg !== "Extension:RegisterContentScripts") {
      return originalBroadcast.call(this, msg, data);
    }

    broadcastCalledCount++;
    Assert.equal(
      1,
      broadcastCalledCount,
      "broadcast override should be called once"
    );

    await originalBroadcast.call(this, msg, data);

    Assert.equal(extension.id, data.id, "got expected extension ID");
    Assert.equal(1, data.scripts.length, "expected 1 script to register");
    Assert.ok(
      data.scripts[0].options.jsPaths[0].endsWith("script.js"),
      "got expected js file"
    );

    // Collect the existing process IDs.
    let existingProcessIDs = ChromeUtils.getAllDOMProcesses().map(
      p => p.childID
    );

    secondContentPage = await ExtensionTestUtils.loadContentPage(
      `${BASE_URL}/dummy_page.html`
    );

    const {
      childID: expectedPID,
    } = secondContentPage.browsingContext.currentWindowGlobal.domProcess;

    // We expect to have a new process created for `secondContentPage`.
    Assert.ok(
      !existingProcessIDs.includes(expectedPID),
      `expected PID ${expectedPID} to not be in [${existingProcessIDs.join(
        ", "
      )}])`
    );
  };

  await extension.startup();
  await extension.awaitMessage("background-done");

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );

  await Promise.all([
    extension.awaitMessage("script-ran: file_sample.html"),
    extension.awaitMessage("script-ran: dummy_page.html"),
  ]);

  await contentPage.close();
  await secondContentPage.close();
  await extension.unload();
};

add_task(
  {
    pref_set: [["extensions.manifestV3.enabled", true]],
  },
  async function test_scripting_registerContentScripts() {
    let extension = makeExtension({
      manifest: {
        manifest_version: 3,
      },
      async background() {
        const script = {
          id: "a-script",
          js: ["script.js"],
          matches: ["http://*/*/*.html"],
        };

        await browser.scripting.registerContentScripts([script]);

        browser.test.sendMessage("background-done");
      },
    });

    await verifyRegistrationWithNewProcess(extension);
  }
);

add_task(async function test_contentScripts_register() {
  let extension = makeExtension({
    manifest: {
      manifest_version: 2,
    },
    async background() {
      await browser.contentScripts.register({
        js: [{ file: "script.js" }],
        matches: ["http://*/*/*.html"],
      });

      browser.test.sendMessage("background-done");
    },
  });

  await verifyRegistrationWithNewProcess(extension);
});

add_task(async function test_userScripts_register() {
  let extension = makeExtension({
    manifest: {
      manifest_version: 2,
      user_scripts: {
        api_script: "inject_browser.js",
      },
    },
    async background() {
      await browser.userScripts.register({
        js: [{ file: "script.js" }],
        matches: ["http://*/*/*.html"],
      });

      browser.test.sendMessage("background-done");
    },
  });

  await verifyRegistrationWithNewProcess(extension);
});
