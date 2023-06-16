"use strict";

Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);

const server = createHttpServer({ hosts: ["example.com", "example.net"] });
server.registerDirectory("/data/", do_get_file("data"));

const { ExtensionPermissions } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionPermissions.sys.mjs"
);

function grantOptional({ extension: ext }, origins) {
  return ExtensionPermissions.add(ext.id, { origins, permissions: [] }, ext);
}

function revokeOptional({ extension: ext }, origins) {
  return ExtensionPermissions.remove(ext.id, { origins, permissions: [] }, ext);
}

// Test granted optional permissions work with XHR/fetch in new processes.
add_task(
  {
    pref_set: [["dom.ipc.keepProcessesAlive.extension", 0]],
  },
  async function test_fetch_origin_permissions_change() {
    let extension = ExtensionTestUtils.loadExtension({
      manifest: {
        host_permissions: ["http://example.com/*"],
        optional_permissions: ["http://example.net/*"],
      },
      files: {
        "page.js"() {
          fetch("http://example.net/data/file_sample.html")
            .then(req => req.text())
            .then(text => browser.test.sendMessage("done", { text }))
            .catch(e => browser.test.sendMessage("done", { error: e.message }));
        },
        "page.html": `<!DOCTYPE html><meta charset="utf-8"><script src="page.js"></script>`,
      },
    });

    await extension.startup();

    let osPid;
    {
      // Grant permissions before extension process exists.
      await grantOptional(extension, ["http://example.net/*"]);

      let page = await ExtensionTestUtils.loadContentPage(
        extension.extension.baseURI.resolve("page.html")
      );

      let { text } = await extension.awaitMessage("done");
      ok(text.includes("Sample text"), "Can read from granted optional host.");

      osPid = page.browsingContext.currentWindowGlobal.osPid;
      await page.close();
    }

    // Release the extension process so that next part starts a new one.
    Services.ppmm.releaseCachedProcesses();

    {
      // Revoke permissions and confirm fetch fails.
      await revokeOptional(extension, ["http://example.net/*"]);

      let page = await ExtensionTestUtils.loadContentPage(
        extension.extension.baseURI.resolve("page.html")
      );

      let { error } = await extension.awaitMessage("done");
      ok(error.includes("NetworkError"), `Expected error: ${error}`);

      if (WebExtensionPolicy.useRemoteWebExtensions) {
        notEqual(
          osPid,
          page.browsingContext.currentWindowGlobal.osPid,
          "Second part of the test used a new process."
        );
      }

      await page.close();
    }

    await extension.unload();
  }
);
