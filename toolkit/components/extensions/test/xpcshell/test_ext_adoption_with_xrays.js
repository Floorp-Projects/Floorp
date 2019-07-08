"use strict";

// ExtensionContent.jsm needs to know when it's running from xpcshell,
// to use the right timeout for content scripts executed at document_idle.
ExtensionTestUtils.mockAppInfo();
const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

add_task(async function test_contentscript_xrays() {
  async function contentScript() {
    let node = window.document.createElement("div");
    node.expando = 5;

    browser.test.assertEq(
      node.expando,
      5,
      "We should be able to see our expando"
    );
    browser.test.assertEq(
      node.wrappedJSObject.expando,
      undefined,
      "Underlying object should not have our expando"
    );

    window.frames[0].document.adoptNode(node);
    browser.test.assertEq(
      node.expando,
      5,
      "Adoption should not change expandos"
    );
    browser.test.assertEq(
      node.wrappedJSObject.expando,
      undefined,
      "Adoption should really not change expandos"
    );

    // Repeat but now with an object that has a reference from the
    // window it's being cloned into.
    node = window.document.createElement("div");
    node.expando = 6;

    browser.test.assertEq(
      node.expando,
      6,
      "We should be able to see our expando (2)"
    );
    browser.test.assertEq(
      node.wrappedJSObject.expando,
      undefined,
      "Underlying object should not have our expando (2)"
    );

    window.frames[0].wrappedJSObject.incoming = node.wrappedJSObject;

    window.frames[0].document.adoptNode(node);
    browser.test.assertEq(
      node.expando,
      6,
      "Adoption should not change expandos (2)"
    );
    browser.test.assertEq(
      node.wrappedJSObject.expando,
      undefined,
      "Adoption should really not change expandos (2)"
    );

    // Repeat once more, now with an expando that refers to the object itself.
    node = window.document.createElement("div");
    node.expando = node;

    browser.test.assertEq(
      node.expando,
      node,
      "We should be able to see our self-referential expando (3)"
    );
    browser.test.assertEq(
      node.wrappedJSObject.expando,
      undefined,
      "Underlying object should not have our self-referential expando (3)"
    );

    window.frames[0].document.adoptNode(node);
    browser.test.assertEq(
      node.expando,
      node,
      "Adoption should not change self-referential expando (3)"
    );
    browser.test.assertEq(
      node.wrappedJSObject.expando,
      undefined,
      "Adoption should really not change self-referential expando (3)"
    );

    // And test what happens if we now set document.domain and cause
    // wrapper remapping.
    let doc = window.frames[0].document;
    // eslint-disable-next-line no-self-assign
    doc.domain = doc.domain;

    browser.test.notifyPass("contentScriptAdoptionWithXrays");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://*/*/file_toplevel.html"],
          js: ["content_script.js"],
        },
      ],
    },

    files: {
      "content_script.js": contentScript,
    },
  });

  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_toplevel.html`
  );

  await extension.awaitFinish("contentScriptAdoptionWithXrays");

  await contentPage.close();
  await extension.unload();
});
