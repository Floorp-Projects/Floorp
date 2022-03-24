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
      host_permissions: ["http://localhost/*"],
      granted_host_permissions: true,
      ...manifestProps,
    },
    temporarilyInstalled: true,
    ...otherProps,
  });
};

add_task(async function test_scripting_updateContentScripts() {
  let extension = makeExtension({
    async background() {
      const script = {
        id: "a-script",
        js: ["script-1.js"],
        matches: ["http://*/*/*.html"],
        persistAcrossSessions: false,
      };

      await browser.scripting.registerContentScripts([script]);
      await browser.scripting.updateContentScripts([
        {
          id: script.id,
          js: ["script-2.js"],
        },
      ]);
      let scripts = await browser.scripting.getRegisteredContentScripts();
      browser.test.assertEq(1, scripts.length, "expected 1 registered script");

      browser.test.sendMessage("background-ready");
    },
    files: {
      "script-1.js": () => {
        browser.test.fail("script-1 should not be executed");
      },
      "script-2.js": () => {
        browser.test.sendMessage(
          `script-2 executed in ${location.pathname.split("/").pop()}`
        );
      },
    },
  });

  await extension.startup();
  await extension.awaitMessage("background-ready");

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );

  await extension.awaitMessage("script-2 executed in file_sample.html");

  await extension.unload();
  await contentPage.close();
});

add_task(
  async function test_scripting_updateContentScripts_non_default_values() {
    let extension = makeExtension({
      async background() {
        const script = {
          id: "a-script",
          allFrames: true,
          matches: ["http://*/*/*.html"],
          runAt: "document_start",
          persistAcrossSessions: false,
          css: ["style.js"],
          excludeMatches: ["http://*/*/foobar.html"],
          js: ["script.js"],
        };

        await browser.scripting.registerContentScripts([script]);

        // This should not modify the previously registered script.
        await browser.scripting.updateContentScripts([{ id: script.id }]);

        let scripts = await browser.scripting.getRegisteredContentScripts();
        browser.test.assertEq(
          JSON.stringify([script]),
          JSON.stringify(scripts),
          "expected unmodified registered script"
        );

        browser.test.sendMessage("background-done");
      },
      files: {
        "script.js": "",
        "style.css": "",
      },
    });

    await extension.startup();
    await extension.awaitMessage("background-done");
    await extension.unload();
  }
);
