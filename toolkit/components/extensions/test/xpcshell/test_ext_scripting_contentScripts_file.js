"use strict";

const FILE_DUMMY_URL = Services.io.newFileURI(
  do_get_file("data/dummy_page.html")
).spec;

// ExtensionContent.jsm needs to know when it's running from xpcshell, to use
// the right timeout for content scripts executed at document_idle.
ExtensionTestUtils.mockAppInfo();

Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);

const makeExtension = ({ manifest: manifestProps, ...otherProps }) => {
  return ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      permissions: ["scripting"],
      host_permissions: ["<all_urls>"],
      granted_host_permissions: true,
      ...manifestProps,
    },
    temporarilyInstalled: true,
    ...otherProps,
  });
};

add_task(async function test_registered_content_script_with_files() {
  let extension = makeExtension({
    async background() {
      const MATCHES = [
        { id: "script-1", matches: ["<all_urls>"] },
        { id: "script-2", matches: ["file:///*"] },
        { id: "script-3", matches: ["file://*/*dummy_page.html"] },
        { id: "fail-if-executed", matches: ["*://*/*"] },
      ];

      await browser.scripting.registerContentScripts(
        MATCHES.map(({ id, matches }) => ({
          id,
          js: [`${id}.js`],
          matches,
          persistAcrossSessions: false,
        }))
      );

      browser.test.sendMessage("background-ready");
    },
    files: {
      "script-1.js": () => {
        browser.test.sendMessage("script-1-ran");
      },
      "script-2.js": () => {
        browser.test.sendMessage("script-2-ran");
      },
      "script-3.js": () => {
        browser.test.sendMessage("script-3-ran");
      },
      "fail-if-executed.js": () => {
        browser.test.fail("this script should not be executed");
      },
    },
  });

  await extension.startup();
  await extension.awaitMessage("background-ready");

  let contentPage = await ExtensionTestUtils.loadContentPage(FILE_DUMMY_URL);

  await Promise.all([
    extension.awaitMessage("script-1-ran"),
    extension.awaitMessage("script-2-ran"),
    extension.awaitMessage("script-3-ran"),
  ]);

  await contentPage.close();
  await extension.unload();
});
