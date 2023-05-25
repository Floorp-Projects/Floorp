"use strict";

const server = createHttpServer({ hosts: ["example.com"] });
server.registerPathHandler("/", (req, res) => {
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.setHeader("Access-Control-Max-Age", "0");
});

add_setup(() => {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.enabled", true);
});

async function startDNRExtension({ privateBrowsingAllowed }) {
  let extension = ExtensionTestUtils.loadExtension({
    incognitoOverride: privateBrowsingAllowed ? "spanning" : undefined,
    async background() {
      await browser.declarativeNetRequest.updateSessionRules({
        addRules: [{ id: 1, condition: {}, action: { type: "block" } }],
      });
      browser.test.sendMessage("dnr_registered");
    },
    manifest: {
      manifest_version: 3,
      permissions: ["declarativeNetRequest"],
      browser_specific_settings: { gecko: { id: "@dnr-ext" } },
    },
  });
  await extension.startup();
  await extension.awaitMessage("dnr_registered");
  return extension;
}

async function testMatchedByDNR(privateBrowsing) {
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/?page",
    { privateBrowsing }
  );
  let wasRequestBlocked = await contentPage.legacySpawn(null, async () => {
    try {
      await content.fetch("http://example.com/?fetch");
      return false;
    } catch (e) {
      // Request blocked by DNR rule from startDNRExtension().
      return true;
    }
  });
  await contentPage.close();
  return wasRequestBlocked;
}

add_task(async function private_browsing_not_allowed_by_default() {
  let extension = await startDNRExtension({ privateBrowsingAllowed: false });
  Assert.equal(
    await testMatchedByDNR(false),
    true,
    "DNR applies to non-private browsing requests by default"
  );
  Assert.equal(
    await testMatchedByDNR(true),
    false,
    "DNR not applied to private browsing requests by default"
  );
  await extension.unload();
});

add_task(async function private_browsing_allowed() {
  let extension = await startDNRExtension({ privateBrowsingAllowed: true });
  Assert.equal(
    await testMatchedByDNR(false),
    true,
    "DNR applies to non-private requests regardless of privateBrowsingAllowed"
  );
  Assert.equal(
    await testMatchedByDNR(true),
    true,
    "DNR applied to private browsing requests when privateBrowsingAllowed"
  );
  await extension.unload();
});

add_task(
  { pref_set: [["extensions.dnr.feedback", true]] },
  async function testMatchOutcome_unaffected_by_privateBrowsing() {
    let extensionWithoutPrivateBrowsingAllowed = await startDNRExtension({});
    let extension = ExtensionTestUtils.loadExtension({
      incognitoOverride: "spanning",
      manifest: {
        manifest_version: 3,
        permissions: ["declarativeNetRequest", "declarativeNetRequestFeedback"],
      },
      files: {
        "page.html": `<!DOCTYPE html><script src="page.js"></script>`,
        "page.js": async () => {
          browser.test.assertTrue(
            browser.extension.inIncognitoContext,
            "Extension page is opened in a private browsing context"
          );
          browser.test.assertDeepEq(
            {
              matchedRules: [
                { ruleId: 1, rulesetId: "_session", extensionId: "@dnr-ext" },
              ],
            },
            // testMatchOutcome does not offer a way to specify the private
            // browsing mode of a request. Confirm that testMatchOutcome always
            // simulates requests in normal private browsing mode, even if the
            // testMatchOutcome method itself is called from an extension page
            // in private browsing mode.
            await browser.declarativeNetRequest.testMatchOutcome(
              { url: "http://example.com/?simulated_request", type: "image" },
              { includeOtherExtensions: true }
            ),
            "testMatchOutcome includes DNR from extensions without pbm access"
          );
          browser.test.sendMessage("done");
        },
      },
    });
    await extension.startup();
    let contentPage = await ExtensionTestUtils.loadContentPage(
      `moz-extension://${extension.uuid}/page.html`,
      { privateBrowsing: true }
    );
    await extension.awaitMessage("done");
    await contentPage.close();
    await extension.unload();
    await extensionWithoutPrivateBrowsingAllowed.unload();
  }
);
