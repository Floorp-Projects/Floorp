"use strict";

const server = createHttpServer({ hosts: ["example.com"] });
server.registerPathHandler("/from", () => {
  Assert.ok(false, "Test should have redirected /from elsewhere");
});

add_task(async function test_navigate_main_frame() {
  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      await browser.declarativeNetRequest.updateSessionRules({
        addRules: [
          {
            id: 1,
            condition: { urlFilter: "from", resourceTypes: ["main_frame"] },
            action: { type: "redirect", redirect: { extensionPath: "/t.htm" } },
          },
        ],
      });
      browser.test.sendMessage("redirect_setup");
    },
    manifest: {
      manifest_version: 3,
      permissions: ["declarativeNetRequest"],
      host_permissions: ["*://example.com/*"],
      granted_host_permissions: true,
      web_accessible_resources: [
        { resources: ["t.htm"], matches: ["*://example.com/*"] },
      ],
    },
    temporarilyInstalled: true, // <-- for granted_host_permissions
    files: {
      "t.htm": `<script src="to.js"></script>`,
      "to.js": () => {
        browser.test.sendMessage("redirect_target_loaded", location.href);
      },
    },
  });
  await extension.startup();
  await extension.awaitMessage("redirect_setup");
  const expectedFinalUrl = `moz-extension://${extension.uuid}/t.htm`;
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/from",
    { redirectUrl: expectedFinalUrl }
  );
  equal(
    await extension.awaitMessage("redirect_target_loaded"),
    expectedFinalUrl,
    "Page at expected URL"
  );

  // Regression test for bug 1940339 / bug 1826867: we should see the
  // moz-extension:-URL here, and not the underlying jar:file:-URL.
  if (Services.appinfo.sessionHistoryInParent) {
    let sh = contentPage.browsingContext.sessionHistory;
    let she = sh.getEntryAtIndex(sh.index);
    equal(she.URI.spec, expectedFinalUrl, "SessionHistoryEntry url is correct");
    // Extra sanity check: when extensions trigger redirects, the history is
    // not populated with pre-redirect URLs.
    equal(sh.index, 0, "No pre-redirect history entries");
  } else {
    await contentPage.spawn([expectedFinalUrl], expectedFinalUrl => {
      let sh = this.docShell.QueryInterface(Ci.nsIWebNavigation).sessionHistory;
      let she = sh.legacySHistory.getEntryAtIndex(sh.index);
      Assert.equal(
        she.URI.spec,
        expectedFinalUrl,
        "SessionHistoryEntry url is correct (with SHIP disabled)"
      );
      // Extra sanity check: when extensions trigger redirects, the history is
      // not populated with pre-redirect URLs.
      Assert.equal(sh.index, 0, "No pre-redirect history entries");
    });
  }

  await contentPage.close();
  await extension.unload();
});
