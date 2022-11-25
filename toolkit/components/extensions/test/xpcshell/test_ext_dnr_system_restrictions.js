"use strict";

const server = createHttpServer({ hosts: ["example.com", "restricted"] });
server.registerPathHandler("/", (req, res) => {
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.write("response from server");
});

add_setup(() => {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.enabled", true);
  // The restrictedDomains pref should be set early, because the pref is read
  // only once (on first use) by WebExtensionPolicy::IsRestrictedURI.
  Services.prefs.setCharPref(
    "extensions.webextensions.restrictedDomains",
    "restricted"
  );
});

async function startDNRExtension() {
  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      await browser.declarativeNetRequest.updateSessionRules({
        addRules: [{ id: 1, condition: {}, action: { type: "block" } }],
      });
      browser.test.sendMessage("dnr_registered");
    },
    manifest: {
      manifest_version: 3,
      permissions: ["declarativeNetRequest"],
    },
  });
  await extension.startup();
  await extension.awaitMessage("dnr_registered");
  return extension;
}

add_task(async function dnr_ignores_system_requests() {
  let extension = await startDNRExtension();
  Assert.equal(
    await (await fetch("http://example.com/")).text(),
    "response from server",
    "DNR should not block requests from system principal"
  );
  await extension.unload();
});

add_task(async function dnr_ignores_requests_to_restrictedDomains() {
  let extension = await startDNRExtension();
  Assert.equal(
    await ExtensionTestUtils.fetch("http://example.com/", "http://restricted/"),
    "response from server",
    "DNR should not block destination in restrictedDomains"
  );
  await extension.unload();
});

add_task(async function dnr_ignores_initiator_from_restrictedDomains() {
  let extension = await startDNRExtension();
  Assert.equal(
    await ExtensionTestUtils.fetch("http://restricted/", "http://example.com/"),
    "response from server",
    "DNR should not block requests initiated from a page in restrictedDomains"
  );
  await extension.unload();
});
