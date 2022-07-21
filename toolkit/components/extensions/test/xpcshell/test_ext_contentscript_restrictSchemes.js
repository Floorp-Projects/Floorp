"use strict";

function makeExtension({ id, isPrivileged, withScriptingAPI = false }) {
  let permissions = [];
  let content_scripts = [];
  let background = () => {
    browser.test.sendMessage("background-ready");
  };

  if (isPrivileged) {
    permissions.push("mozillaAddons");
  }

  if (withScriptingAPI) {
    permissions.push("scripting");
    // When we don't use a content script registered via the manifest, we
    // should add the origin as a permission.
    permissions.push("resource://foo/file_sample.html");

    // Redefine background script to dynamically register the content script.
    if (isPrivileged) {
      background = async () => {
        await browser.scripting.registerContentScripts([
          {
            id: "content_script",
            js: ["content_script.js"],
            matches: ["resource://foo/file_sample.html"],
            persistAcrossSessions: false,
            runAt: "document_start",
          },
        ]);

        let scripts = await browser.scripting.getRegisteredContentScripts();
        browser.test.assertEq(1, scripts.length, "expected 1 script");

        browser.test.sendMessage("background-ready");
      };
    } else {
      background = async () => {
        await browser.test.assertRejects(
          browser.scripting.registerContentScripts([
            {
              id: "content_script",
              js: ["content_script.js"],
              matches: ["resource://foo/file_sample.html"],
              persistAcrossSessions: false,
              runAt: "document_start",
            },
          ]),
          /Invalid url pattern: resource:/,
          "got expected error"
        );

        browser.test.sendMessage("background-ready");
      };
    }
  } else {
    content_scripts.push({
      js: ["content_script.js"],
      matches: ["resource://foo/file_sample.html"],
      run_at: "document_start",
    });
  }

  return ExtensionTestUtils.loadExtension({
    isPrivileged,

    manifest: {
      manifest_version: 2,
      browser_specific_settings: { gecko: { id } },
      content_scripts,
      permissions,
    },

    background,

    files: {
      "content_script.js"() {
        browser.test.assertEq(
          "resource://foo/file_sample.html",
          document.documentURI,
          `Loaded content script into the correct document (extension: ${browser.runtime.id})`
        );
        browser.test.sendMessage(`content-script-${browser.runtime.id}`);
      },
    },
  });
}

const verifyRestrictSchemes = async ({ withScriptingAPI }) => {
  let resProto = Services.io
    .getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler);
  resProto.setSubstitutionWithFlags(
    "foo",
    Services.io.newFileURI(do_get_file("data")),
    resProto.ALLOW_CONTENT_ACCESS
  );

  let unprivileged = makeExtension({
    id: "unprivileged@tests.mozilla.org",
    isPrivileged: false,
    withScriptingAPI,
  });
  let privileged = makeExtension({
    id: "privileged@tests.mozilla.org",
    isPrivileged: true,
    withScriptingAPI,
  });

  await unprivileged.startup();
  await unprivileged.awaitMessage("background-ready");

  await privileged.startup();
  await privileged.awaitMessage("background-ready");

  unprivileged.onMessage(
    "content-script-unprivileged@tests.mozilla.org",
    () => {
      ok(
        false,
        "Unprivileged extension executed content script on resource URL"
      );
    }
  );

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `resource://foo/file_sample.html`
  );

  await privileged.awaitMessage("content-script-privileged@tests.mozilla.org");

  await contentPage.close();

  await privileged.unload();
  await unprivileged.unload();
};

// Bug 1780507: this only works with MV2 currently because MV3's optional
// permission mechanism lacks `restrictSchemes` flags.
add_task(async function test_contentscript_restrictSchemes_mv2() {
  await verifyRestrictSchemes({ withScriptingAPI: false });
});

// Bug 1780507: this only works with MV2 currently because MV3's optional
// permission mechanism lacks `restrictSchemes` flags.
add_task(async function test_contentscript_restrictSchemes_scripting_mv2() {
  await verifyRestrictSchemes({ withScriptingAPI: true });
});
