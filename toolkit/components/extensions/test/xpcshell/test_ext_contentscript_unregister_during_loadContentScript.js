/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

add_task(async function content_script_unregistered_during_loadContentScript() {
  let content_scripts = [];

  for (let i = 0; i < 10; i++) {
    content_scripts.push({
      matches: ["<all_urls>"],
      js: ["dummy.js"],
      run_at: "document_start",
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts,
    },
    files: {
      "dummy.js": function() {
        browser.test.sendMessage("content-script-executed");
      },
    },
  });

  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );
  info("Wait for all the content scripts to be executed");
  await Promise.all(
    content_scripts.map(() => extension.awaitMessage("content-script-executed"))
  );

  const promiseDone = contentPage.spawn([extension.id], extensionId => {
    const { ExtensionProcessScript } = ChromeUtils.import(
      "resource://gre/modules/ExtensionProcessScript.jsm"
    );

    return new Promise(resolve => {
      // This recreates a scenario similar to Bug 1593240 and ensures that the
      // related fix doesn't regress. Replacing loadContentScript with a
      // function that unregisters all the content scripts make us sure that
      // mutating the policy contentScripts doesn't trigger a crash due to
      // the invalidation of the contentScripts iterator being used by the
      // caller (ExtensionPolicyService::CheckContentScripts).
      const { loadContentScript } = ExtensionProcessScript;
      ExtensionProcessScript.loadContentScript = async (...args) => {
        const policy = WebExtensionPolicy.getByID(extensionId);
        let initial = policy.contentScripts.length;
        let i = initial;
        while (i) {
          policy.unregisterContentScript(policy.contentScripts[--i]);
        }
        Services.tm.dispatchToMainThread(() =>
          resolve({
            initial,
            final: policy.contentScripts.length,
          })
        );
        // Call the real loadContentScript method.
        return loadContentScript(...args);
      };
    });
  });

  info("Reload the webpage");
  await contentPage.loadURL(`${BASE_URL}/file_sample.html`);
  info("Wait for all the content scripts to be executed again");
  await Promise.all(
    content_scripts.map(() => extension.awaitMessage("content-script-executed"))
  );
  info("No crash triggered as expected");

  Assert.deepEqual(
    await promiseDone,
    { initial: content_scripts.length, final: 0 },
    "All content scripts unregistered as expected"
  );

  await contentPage.close();
  await extension.unload();
});
