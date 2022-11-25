"use strict";

add_setup(() => {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.enabled", true);
});

const server = createHttpServer({
  hosts: ["example.com", "example.net", "example.org"],
});
server.registerPathHandler("/never_reached", (req, res) => {
  Assert.ok(false, "Server should never have been reached");
});
server.registerPathHandler("/allowed", (req, res) => {
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.setHeader("Access-Control-Max-Age", "0");
  res.write("allowed");
});
server.registerPathHandler("/", (req, res) => {
  res.write("Dummy page");
});

add_task(async function allowAllRequests_allows_request() {
  async function background() {
    await browser.declarativeNetRequest.updateSessionRules({
      addRules: [
        // allowAllRequests should take precedence over block.
        {
          id: 1,
          condition: { resourceTypes: ["main_frame", "xmlhttprequest"] },
          action: { type: "block" },
        },
        {
          id: 2,
          condition: { resourceTypes: ["main_frame"] },
          action: { type: "allowAllRequests" },
        },
        {
          id: 3,
          priority: 2,
          // Note: when not specified, main_frame is excluded by default. So
          // when a main_frame request is triggered, only rules 1 and 2 match.
          condition: { requestDomains: ["example.com"] },
          action: { type: "block" },
        },
      ],
    });
    browser.test.sendMessage("dnr_registered");
  }
  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      manifest_version: 3,
      permissions: ["declarativeNetRequest"],
    },
  });
  await extension.startup();
  await extension.awaitMessage("dnr_registered");

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/"
  );
  Assert.equal(
    await contentPage.spawn(null, () => content.document.URL),
    "http://example.com/",
    "main_frame request should have been allowed by allowAllRequests"
  );

  async function checkCanFetch(url) {
    return contentPage.spawn(url, async url => {
      try {
        await (await content.fetch(url)).text();
        return true;
      } catch (e) {
        return false; // NetworkError: blocked
      }
    });
  }

  Assert.equal(
    await checkCanFetch("http://example.com/never_reached"),
    false,
    "should be blocked by DNR rule 3"
  );
  Assert.equal(
    await checkCanFetch("http://example.net/"),
    // TODO bug 1797403: Fix expectation once allowAllRequests is implemented:
    // true,
    // "should not be blocked by block rule due to allowAllRequests rule"
    false,
    "is blocked because persistency of allowAllRequests is not yet implemented"
  );

  await contentPage.close();
  await extension.unload();
});
