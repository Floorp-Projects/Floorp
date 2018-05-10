"use strict";

function makeExtension(id, isPrivileged) {
  return ExtensionTestUtils.loadExtension({
    isPrivileged,

    manifest: {
      applications: {gecko: {id}},

      permissions: isPrivileged ? ["mozillaAddons"] : [],

      content_scripts: [
        {
          "matches": ["resource://foo/file_sample.html"],
          "js": ["content_script.js"],
          "run_at": "document_start",
        },
      ],
    },

    files: {
      "content_script.js"() {
        browser.test.assertEq("resource://foo/file_sample.html", document.documentURI,
                              `Loaded content script into the correct document (extension: ${browser.runtime.id})`);
        browser.test.sendMessage(`content-script-${browser.runtime.id}`);
      },
    },
  });
}

add_task(async function test_contentscript_restrictSchemes() {
  let resProto = Services.io.getProtocolHandler("resource").QueryInterface(Ci.nsIResProtocolHandler);
  resProto.setSubstitutionWithFlags("foo", Services.io.newFileURI(do_get_file("data")),
                                    resProto.ALLOW_CONTENT_ACCESS);

  let unprivileged = makeExtension("unprivileged@tests.mozilla.org", false);
  let privileged = makeExtension("privileged@tests.mozilla.org", true);

  await unprivileged.startup();
  await privileged.startup();

  unprivileged.onMessage("content-script-unprivileged@tests.mozilla.org", () => {
    ok(false, "Unprivileged extension executed content script on resource URL");
  });

  let contentPage = await ExtensionTestUtils.loadContentPage(`resource://foo/file_sample.html`);

  await privileged.awaitMessage("content-script-privileged@tests.mozilla.org");

  await contentPage.close();

  await privileged.unload();
  await unprivileged.unload();
});
