"use strict";

// ExtensionContent.jsm needs to know when it's running from xpcshell,
// to use the right timeout for content scripts executed at document_idle.
ExtensionTestUtils.mockAppInfo();
const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

add_task(async function test_contentscript_private_field_xrays() {
  async function contentScript() {
    let node = window.document.createElement("div");

    class Base {
      constructor(o) {
        return o;
      }
    }

    class A extends Base {
      #x = 5;
      static gx(o) {
        return o.#x;
      }
      static sx(o, v) {
        o.#x = v;
      }
    }

    browser.test.log(A.toString());

    // Stamp node with A's private field.
    new A(node);

    browser.test.log("stamped");

    browser.test.assertEq(
      A.gx(node),
      5,
      "We should be able to see our expando private field"
    );
    browser.test.log("Read");
    browser.test.assertThrows(
      () => A.gx(node.wrappedJSObject),
      /can't access private field or method/,
      "Underlying object should not have our private field"
    );

    browser.test.log("threw");
    window.frames[0].document.adoptNode(node);
    browser.test.log("adopted");
    browser.test.assertEq(
      A.gx(node),
      5,
      "Adoption should not change expando private field"
    );
    browser.test.log("read");
    browser.test.assertThrows(
      () => A.gx(node.wrappedJSObject),
      /can't access private field or method/,
      "Adoption should really not change expandos private fields"
    );
    browser.test.log("threw2");

    // Repeat but now with an object that has a reference from the
    // window it's being cloned into.
    node = window.document.createElement("div");
    // Stamp node with A's private field.
    new A(node);
    A.sx(node, 6);

    browser.test.assertEq(
      A.gx(node),
      6,
      "We should be able to see our expando (2)"
    );
    browser.test.assertThrows(
      () => A.gx(node.wrappedJSObject),
      /can't access private field or method/,
      "Underlying object should not have exxpando. (2)"
    );

    window.frames[0].wrappedJSObject.incoming = node.wrappedJSObject;
    window.frames[0].document.adoptNode(node);

    browser.test.assertEq(
      A.gx(node),
      6,
      "We should be able to see our expando (3)"
    );
    browser.test.assertThrows(
      () => A.gx(node.wrappedJSObject),
      /can't access private field or method/,
      "Underlying object should not have exxpando. (3)"
    );

    // Repeat once more, now with an expando that refers to the object itself
    node = window.document.createElement("div");
    new A(node);
    A.sx(node, node);

    browser.test.assertEq(
      A.gx(node),
      node,
      "We should be able to see our self-referential expando (4)"
    );
    browser.test.assertThrows(
      () => A.gx(node.wrappedJSObject),
      /can't access private field or method/,
      "Underlying object should not have exxpando. (4)"
    );

    window.frames[0].document.adoptNode(node);

    browser.test.assertEq(
      A.gx(node),
      node,
      "Adoption should not change our self-referential expando (4)"
    );
    browser.test.assertThrows(
      () => A.gx(node.wrappedJSObject),
      /can't access private field or method/,
      "Adoption should not change underlying object. (4)"
    );

    // And test what happens if we now set document.domain and cause
    // wrapper remapping.
    let doc = window.frames[0].document;
    // eslint-disable-next-line no-self-assign
    doc.domain = doc.domain;

    browser.test.notifyPass("privateFieldXRayAdoption");
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

  await extension.awaitFinish("privateFieldXRayAdoption");

  await contentPage.close();
  await extension.unload();
});
