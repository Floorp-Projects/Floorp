"use strict";

Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
Services.prefs.setBoolPref("extensions.originControls.grantByDefault", false);

const server = createHttpServer({ hosts: ["example.com", "example.net"] });
server.registerDirectory("/data/", do_get_file("data"));

const HOSTS = ["http://example.com/*", "http://example.net/*"];

const { ExtensionPermissions } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionPermissions.sys.mjs"
);

function grantOptional({ extension: ext }, origins) {
  return ExtensionPermissions.add(ext.id, { origins, permissions: [] }, ext);
}

function revokeOptional({ extension: ext }, origins) {
  return ExtensionPermissions.remove(ext.id, { origins, permissions: [] }, ext);
}

function makeExtension(id, content_scripts) {
  return ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,

      browser_specific_settings: { gecko: { id } },
      content_scripts,

      permissions: ["scripting"],
      host_permissions: HOSTS,
    },
    files: {
      "cs.js"() {
        browser.test.log(`${browser.runtime.id} script on ${location.host}`);
        browser.test.sendMessage(`${browser.runtime.id}_on_${location.host}`);
      },
    },
    background() {
      browser.test.onMessage.addListener(async (msg, origins) => {
        browser.test.log(`${browser.runtime.id} registering content scripts`);
        await browser.scripting.registerContentScripts([
          {
            id: "cs1",
            persistAcrossSessions: false,
            matches: origins,
            js: ["cs.js"],
          },
        ]);
        browser.test.sendMessage("done");
      });
    },
  });
}

// Test that content scripts in MV3 enforce origin permissions.
// Test granted optional permissions are available in newly spawned processes.
add_task(async function test_contentscript_mv3_permissions() {
  // Alpha lists content scripts in the manifest.
  let alpha = makeExtension("alpha@test", [{ matches: HOSTS, js: ["cs.js"] }]);
  let beta = makeExtension("beta@test");

  await grantOptional(alpha, HOSTS);
  await grantOptional(beta, ["http://example.net/*"]);
  info("Granted initial permissions for both.");

  await alpha.startup();
  await beta.startup();

  // Beta registers same content scripts using the scripting api.
  beta.sendMessage("register", HOSTS);
  await beta.awaitMessage("done");

  // Only Alpha has origin permissions for example.com.
  {
    let page = await ExtensionTestUtils.loadContentPage(
      `http://example.com/data/file_sample.html`
    );
    info("Loaded a page from example.com.");

    await alpha.awaitMessage("alpha@test_on_example.com");
    info("Got a message from alpha@test on example.com.");
    await page.close();
  }

  await revokeOptional(alpha, ["http://example.net/*"]);
  info("Revoked example.net permissions from Alpha.");

  // Now only Beta has origin permissions for example.net.
  {
    let page = await ExtensionTestUtils.loadContentPage(
      `http://example.net/data/file_sample.html`
    );
    info("Loaded a page from example.net.");

    await beta.awaitMessage("beta@test_on_example.net");
    info("Got a message from beta@test on example.net.");
    await page.close();
  }

  info("Done, unloading Alpha and Beta.");
  await beta.unload();
  await alpha.unload();
});
