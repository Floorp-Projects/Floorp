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

add_task(async function test_web_accessible_resources_matching() {
  let extension = await ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      web_accessible_resources: [
        {
          resources: ["/accessible.html"],
        },
      ],
    },
  });

  await Assert.rejects(
    extension.startup(),
    /web_accessible_resources requires one of "matches" or "extension_ids"/,
    "web_accessible_resources object format incorrect"
  );

  extension = await ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      web_accessible_resources: [
        {
          resources: ["/accessible.html"],
          matches: ["http://example.com/data/*"],
        },
      ],
    },
  });

  await extension.startup();
  ok(true, "web_accessible_resources with matches loads");
  await extension.unload();

  extension = await ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      web_accessible_resources: [
        {
          resources: ["/accessible.html"],
          extension_ids: ["foo@mochitest"],
        },
      ],
    },
  });

  await extension.startup();
  ok(true, "web_accessible_resources with extensions loads");
  await extension.unload();

  extension = await ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      web_accessible_resources: [
        {
          resources: ["/accessible.html"],
          matches: ["http://example.com/data/*"],
          extension_ids: ["foo@mochitest"],
        },
      ],
    },
  });

  await extension.startup();
  ok(true, "web_accessible_resources with matches and extensions loads");
  await extension.unload();
});

add_task(async function test_web_accessible_resources() {
  async function contentScript() {
    let canLoad = window.location.href.startsWith("http://example.com");
    let urls = [
      {
        name: "iframe",
        path: "accessible.html",
        shouldLoad: canLoad,
      },
      {
        name: "iframe",
        path: "inaccessible.html",
        shouldLoad: false,
      },
      {
        name: "img",
        path: "image.png",
        shouldLoad: true,
      },
      {
        name: "script",
        path: "script.js",
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
      let loaded = await test_element_src(
        test.name,
        browser.runtime.getURL(test.path)
      );
      browser.test.assertEq(
        loaded,
        test.shouldLoad,
        `resource loaded ${test.path} in ${window.location.href}`
      );
    }
    browser.test.sendMessage("complete");
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
      host_permissions: ["http://example.com/*", "http://example.org/*"],
      granted_host_permissions: true,

      web_accessible_resources: [
        {
          resources: ["/accessible.html", "/script.js"],
          matches: ["http://example.com/data/*"],
        },
        {
          resources: ["/image.png"],
          matches: ["<all_urls>"],
        },
      ],
    },
    temporarilyInstalled: true,

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

  await extension.awaitMessage("complete");
  await page.close();

  // None of the test resources are loadable in example.org
  page = await ExtensionTestUtils.loadContentPage("http://example.org/data/");

  await extension.awaitMessage("complete");

  await page.close();
  await extension.unload();
});

async function pageScript() {
  function test_element_src(data) {
    return new Promise(resolve => {
      let elem = document.createElement(data.elem);
      let elemContext =
        data.content_context && elem.wrappedJSObject
          ? elem.wrappedJSObject
          : elem;
      elemContext.setAttribute("src", data.url);
      elem.addEventListener(
        "load",
        () => {
          browser.test.log(`got load event for ${data.url}`);
          resolve(true);
        },
        { once: true }
      );
      elem.addEventListener(
        "error",
        () => {
          browser.test.log(`got error event for ${data.url}`);
          resolve(false);
        },
        { once: true }
      );
      document.body.appendChild(elem);
    });
  }
  browser.test.onMessage.addListener(async msg => {
    browser.test.log(`testing ${JSON.stringify(msg)}`);
    let loaded = await test_element_src(msg);
    browser.test.assertEq(loaded, msg.shouldLoad, `${msg.name} loaded`);
    browser.test.sendMessage("web-accessible-resources");
  });
  browser.test.sendMessage("page-loaded");
}

add_task(async function test_web_accessible_resources_extensions() {
  let other = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: { gecko: { id: "other@mochitest" } },
    },
    files: {
      "page.js": pageScript,

      "page.html": `<html><head>
          <meta charset="utf-8">
          <script src="page.js"></script>
        </head></html>`,
    },
  });

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      applications: { gecko: { id: "this@mochitest" } },
      web_accessible_resources: [
        {
          resources: ["/image.png"],
          extension_ids: ["other@mochitest"],
        },
      ],
    },

    files: {
      "image.png": IMAGE_ARRAYBUFFER,
      "inaccessible.png": IMAGE_ARRAYBUFFER,
      "page.js": pageScript,

      "page.html": `<html><head>
          <meta charset="utf-8">
          <script src="page.js"></script>
        </head></html>`,
    },
  });

  await extension.startup();
  let extensionUrl = `moz-extension://${extension.uuid}/`;

  await other.startup();
  let pageUrl = `moz-extension://${other.uuid}/page.html`;

  let page = await ExtensionTestUtils.loadContentPage(pageUrl);
  await other.awaitMessage("page-loaded");

  other.sendMessage({
    name: "accessible resource",
    elem: "img",
    url: `${extensionUrl}image.png`,
    shouldLoad: true,
  });
  await other.awaitMessage("web-accessible-resources");

  other.sendMessage({
    name: "inaccessible resource",
    elem: "img",
    url: `${extensionUrl}inaccessible.png`,
    shouldLoad: false,
  });
  await other.awaitMessage("web-accessible-resources");

  await page.close();

  // test that the extension may load it's own web accessible resource
  page = await ExtensionTestUtils.loadContentPage(`${extensionUrl}page.html`);
  await extension.awaitMessage("page-loaded");

  extension.sendMessage({
    name: "accessible resource",
    elem: "img",
    url: `${extensionUrl}image.png`,
    shouldLoad: true,
  });
  await extension.awaitMessage("web-accessible-resources");

  await page.close();
  await extension.unload();
  await other.unload();
});

// test that a web page not in matches cannot load the resource
add_task(async function test_web_accessible_resources_inaccessible() {
  let extension = ExtensionTestUtils.loadExtension({
    temporarilyInstalled: true,
    manifest: {
      manifest_version: 3,
      applications: { gecko: { id: "web@mochitest" } },
      content_scripts: [
        {
          matches: ["http://example.com/data/*"],
          js: ["page.js"],
          run_at: "document_idle",
        },
      ],
      web_accessible_resources: [
        {
          resources: ["/image.png"],
          extension_ids: ["some_other_ext@mochitest"],
        },
      ],
      host_permissions: ["*://example.com/*"],
      granted_host_permissions: true,
    },

    files: {
      "image.png": IMAGE_ARRAYBUFFER,
      "page.js": pageScript,

      "page.html": `<html><head>
          <meta charset="utf-8">
          <script src="page.js"></script>
        </head></html>`,
    },
  });

  await extension.startup();
  let extensionUrl = `moz-extension://${extension.uuid}/`;
  let page = await ExtensionTestUtils.loadContentPage(
    "http://example.com/data/"
  );
  await extension.awaitMessage("page-loaded");

  extension.sendMessage({
    name: "cannot access resource",
    elem: "img",
    url: `${extensionUrl}image.png`,
    content_context: true,
    shouldLoad: false,
  });
  await extension.awaitMessage("web-accessible-resources");

  await page.close();
  await extension.unload();
});
