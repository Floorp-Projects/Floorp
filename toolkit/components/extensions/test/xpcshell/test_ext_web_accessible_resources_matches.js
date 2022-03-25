"use strict";
Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);

const server = createHttpServer({ hosts: ["example.com", "example.org"] });
server.registerDirectory("/data/", do_get_file("data"));

let image = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAAA1BMVEUAA" +
    "ACnej3aAAAAAXRSTlMAQObYZgAAAApJREFUCNdjYAAAAAIAAeIhvDMAAAAASUVORK5CYII="
);
const IMAGE_ARRAYBUFFER = Uint8Array.from(image, byte => byte.charCodeAt(0))
  .buffer;

add_task(async function test_web_accessible_resources() {
  async function contentScript() {
    let canLoad = window.location.href.startsWith("http://example.com");

    let urls = [
      {
        name: "iframe",
        url: browser.runtime.getURL("accessible.html"),
        shouldLoad: canLoad,
      },
      {
        name: "iframe",
        url: browser.runtime.getURL("inaccessible.html"),
        shouldLoad: false,
      },
      {
        name: "img",
        url: browser.runtime.getURL("image.png"),
        shouldLoad: canLoad,
      },
      {
        name: "script",
        url: browser.runtime.getURL("script.js"),
        shouldLoad: canLoad,
      },
    ];

    function test_element_src(name, url) {
      return new Promise(resolve => {
        let elem = document.createElement(name);
        // Set the src via wrappedJSObject so the load is triggered with the
        // content page's principal rather than ours.
        elem.wrappedJSObject.setAttribute("src", url);
        elem.addEventListener(
          "load",
          () => {
            resolve(true);
          },
          { once: true }
        );
        elem.addEventListener(
          "error",
          () => {
            resolve(false);
          },
          { once: true }
        );
        document.body.appendChild(elem);
      });
    }
    for (let test of urls) {
      let loaded = await test_element_src(test.name, test.url);
      browser.test.assertEq(loaded, test.shouldLoad, "resource loaded");
    }
    browser.test.notifyPass("web-accessible-resources");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      content_scripts: [
        {
          matches: ["http://example.com/data/*", "http://example.org/data/*"],
          js: ["content_script.js"],
          run_at: "document_idle",
        },
      ],

      web_accessible_resources: [
        {
          resources: ["/accessible.html", "/image.png", "/script.js"],
          matches: ["http://example.com/data/*"],
        },
      ],
    },

    files: {
      "content_script.js": contentScript,

      "accessible.html": `<html><head>
          <meta charset="utf-8">
        </head></html>`,

      "inaccessible.html": `<html><head>
          <meta charset="utf-8">
        </head></html>`,

      "image.png": IMAGE_ARRAYBUFFER,
      "script.js": () => {
        // empty script
      },
    },
  });

  await extension.startup();

  let page = await ExtensionTestUtils.loadContentPage(
    "http://example.com/data/"
  );

  await extension.awaitFinish("web-accessible-resources");
  await page.close();

  // None of the test resources are loadable in example.org
  page = await ExtensionTestUtils.loadContentPage("http://example.org/data/");

  await extension.awaitFinish("web-accessible-resources");

  await page.close();
  await extension.unload();
});
