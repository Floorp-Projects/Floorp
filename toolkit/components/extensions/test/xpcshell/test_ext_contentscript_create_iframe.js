"use strict";

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

add_task(async function test_contentscript_create_iframe() {
  function background() {
    browser.runtime.onMessage.addListener((msg, sender) => {
      let { name, availableAPIs, manifest, testGetManifest } = msg;
      let hasExtTabsAPI = availableAPIs.indexOf("tabs") > 0;
      let hasExtWindowsAPI = availableAPIs.indexOf("windows") > 0;

      browser.test.assertFalse(
        hasExtTabsAPI,
        "the created iframe should not be able to use privileged APIs (tabs)"
      );
      browser.test.assertFalse(
        hasExtWindowsAPI,
        "the created iframe should not be able to use privileged APIs (windows)"
      );

      let {
        applications: {
          gecko: { id: expectedManifestGeckoId },
        },
      } = chrome.runtime.getManifest();
      let {
        applications: {
          gecko: { id: actualManifestGeckoId },
        },
      } = manifest;

      browser.test.assertEq(
        actualManifestGeckoId,
        expectedManifestGeckoId,
        "the add-on manifest should be accessible from the created iframe"
      );

      let {
        applications: {
          gecko: { id: testGetManifestGeckoId },
        },
      } = testGetManifest;

      browser.test.assertEq(
        testGetManifestGeckoId,
        expectedManifestGeckoId,
        "GET_MANIFEST() returns manifest data before extension unload"
      );

      browser.test.sendMessage(name);
    });
  }

  function contentScriptIframe() {
    window.GET_MANIFEST = browser.runtime.getManifest.bind(null);

    window.testGetManifestException = () => {
      try {
        window.GET_MANIFEST();
      } catch (exception) {
        return String(exception);
      }
    };

    let testGetManifest = window.GET_MANIFEST();

    let manifest = browser.runtime.getManifest();
    let availableAPIs = Object.keys(browser).filter(key => browser[key]);

    browser.runtime.sendMessage({
      name: "content-script-iframe-loaded",
      availableAPIs,
      manifest,
      testGetManifest,
    });
  }

  const ID = "contentscript@tests.mozilla.org";
  let extensionData = {
    manifest: {
      applications: { gecko: { id: ID } },
      content_scripts: [
        {
          matches: ["http://example.com/data/file_sample.html"],
          js: ["content_script.js"],
          run_at: "document_idle",
        },
      ],
      web_accessible_resources: ["content_script_iframe.html"],
    },

    background,

    files: {
      "content_script.js"() {
        let iframe = document.createElement("iframe");
        iframe.src = browser.runtime.getURL("content_script_iframe.html");
        document.body.appendChild(iframe);
      },
      "content_script_iframe.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
            <script type="text/javascript" src="content_script_iframe.js"></script>
          </head>
        </html>`,
      "content_script_iframe.js": contentScriptIframe,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/data/file_sample.html"
  );

  await extension.awaitMessage("content-script-iframe-loaded");

  info("testing APIs availability once the extension is unloaded...");

  await contentPage.spawn(null, () => {
    this.iframeWindow = this.content[0];

    Assert.ok(this.iframeWindow, "content script enabled iframe found");
    Assert.ok(
      /content_script_iframe\.html$/.test(this.iframeWindow.location),
      "the found iframe has the expected URL"
    );
  });

  await extension.unload();

  info(
    "test content script APIs not accessible from the frame once the extension is unloaded"
  );

  await contentPage.spawn(null, () => {
    let win = Cu.waiveXrays(this.iframeWindow);
    ok(
      !Cu.isDeadWrapper(win.browser),
      "the API object should not be a dead object"
    );

    let manifest;
    let manifestException;
    try {
      manifest = win.browser.runtime.getManifest();
    } catch (e) {
      manifestException = e;
    }

    Assert.ok(!manifest, "manifest should be undefined");

    Assert.equal(
      manifestException.constructor.name,
      "TypeError",
      "expected exception received"
    );

    Assert.ok(
      manifestException.message.endsWith("win.browser.runtime is undefined"),
      "expected exception received"
    );

    let getManifestException = win.testGetManifestException();

    Assert.equal(
      getManifestException,
      "TypeError: can't access dead object",
      "expected exception received"
    );
  });

  await contentPage.close();
});
