"use strict";

const server = createHttpServer({ hosts: ["example.com", "restricted"] });
server.registerPathHandler("/", (req, res) => {
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.write("response from server");
});
server.registerPathHandler("/style_with_import.css", (req, res) => {
  res.setHeader("Content-Type", "text/css");
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.write("@import url('http://example.com/imported.css');");
});
server.registerPathHandler("/imported.css", (req, res) => {
  res.setHeader("Content-Type", "text/css");
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.write("imported_stylesheet_here { }");
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
        addRules: [
          {
            id: 1,
            condition: { resourceTypes: ["xmlhttprequest", "stylesheet"] },
            action: { type: "block" },
          },
          {
            id: 2,
            condition: { urlFilter: "blockme", resourceTypes: ["main_frame"] },
            action: { type: "block" },
          },
        ],
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

add_task(async function dnr_ignores_navigation_to_restrictedDomains() {
  let extension = await startDNRExtension();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://restricted/?blockme"
  );
  await contentPage.spawn([], () => {
    const { document } = content;
    Assert.equal(document.URL, "http://restricted/?blockme", "Same URL");
    Assert.equal(document.body.textContent, "response from server", "body");
  });
  await contentPage.close();
  await extension.unload();
});

add_task(async function dnr_ignores_css_import_at_restrictedDomains() {
  // CSS @import have triggeringPrincipal set to the URL of the stylesheet,
  // and the loadingPrincipal set to the web page. To verify that access is
  // indeed being restricted as expected, confirm that none of the stylesheet
  // requests are blocked by the DNR extension.
  let extension = await startDNRExtension();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://restricted/"
  );
  await contentPage.spawn([], async () => {
    // Use wrappedJSObject so that all operations below are with the principal
    // of the content instead of the system principal (from this ContentTask).
    const { document } = content.wrappedJSObject;
    const style = document.createElement("link");
    style.rel = "stylesheet";
    // Note: intentionally not at "http://restricted/" because we want to check
    // that subresources from a restricted domain are ignored by DNR..
    style.href = "http://example.com/style_with_import.css";
    style.crossOrigin = "anonymous";
    await new Promise(resolve => {
      info("Waiting for style sheet to load...");
      style.onload = resolve;
      document.head.append(style);
    });
    const importRule = style.sheet.cssRules[0];
    Assert.equal(
      importRule?.cssText,
      `@import url("http://example.com/imported.css");`,
      "Not blocked by DNR: Loaded style_with_import.css"
    );
    // Waiving Xrays here because we cannot read cssRules despite CORS because
    // that is not implemented for child stylesheets (loaded via @import):
    // https://searchfox.org/mozilla-central/rev/55d5c4b9dffe5e59eb6b019c1a930ec9ada47e10/layout/style/Loader.cpp#2052
    const importedStylesheet = Cu.unwaiveXrays(importRule.styleSheet);
    Assert.equal(
      importedStylesheet.cssRules[0]?.cssText,
      "imported_stylesheet_here { }",
      "Not blocked by DNR: Loaded import.css"
    );
  });
  await contentPage.close();
  await extension.unload();
});

add_task(
  { pref_set: [["extensions.dnr.feedback", true]] },
  async function testMatchOutcome_and_restrictedDomains() {
    let extension = ExtensionTestUtils.loadExtension({
      async background() {
        await browser.declarativeNetRequest.updateSessionRules({
          addRules: [{ id: 1, condition: {}, action: { type: "block" } }],
        });
        const type = "other"; // matches the condition of the above rule.

        browser.test.assertDeepEq(
          { matchedRules: [] },
          await browser.declarativeNetRequest.testMatchOutcome({
            url: "http://restricted/",
            type,
          }),
          "testMatchOutcome ignores restricted url"
        );
        browser.test.assertDeepEq(
          { matchedRules: [] },
          await browser.declarativeNetRequest.testMatchOutcome({
            url: "http://example.com/",
            initiator: "http://restricted/",
            type,
          }),
          "testMatchOutcome ignores restricted initiator"
        );
        browser.test.sendMessage("done");
      },
      manifest: {
        manifest_version: 3,
        permissions: ["declarativeNetRequest", "declarativeNetRequestFeedback"],
      },
    });
    await extension.startup();
    await extension.awaitMessage("done");
    await extension.unload();
  }
);

add_task(
  // In debug builds, any attempt to load data:-URLs in the parent process
  // results in a crash or at least a logged error, via
  // nsContentSecurityUtils::ValidateScriptFilename.
  //
  // Xpcshell tests use loadFrameScript with data:-URLs, which could trigger the
  // above error / crash, when a page is loaded in the parent process.
  // For example, the following error message (or crash),
  // "InternalError: unsafe filename: data:text/javascript,//"
  // "Hit MOZ_CRASH(Blocking a script load data:text/javascript,// from file (None))"
  // is triggered because of the loadFrameScript call at
  // https://searchfox.org/mozilla-central/rev/11dbac7f64f509b78037465cbb4427ed71f8b565/testing/modules/XPCShellContentUtils.sys.mjs#308
  //
  // This test loads about:logo in the parent, because nsAboutRedirector.cpp
  // registers about:logo without nsIAboutModule::URI_MUST_LOAD_IN_CHILD.
  // When about:logo is loaded, the ContentPage test helper also triggers the
  // above error/crash at:
  // https://searchfox.org/mozilla-central/rev/11dbac7f64f509b78037465cbb4427ed71f8b565/testing/modules/XPCShellContentUtils.sys.mjs#224,242
  //
  // Opt out of the check/crash from ValidateScriptFilename:
  { pref_set: [["security.allow_parent_unrestricted_js_loads", true]] },
  async function non_system_request_with_disallowed_scheme() {
    let extension = await startDNRExtension();
    Assert.equal(
      await (await fetch("http://example.com/")).text(),
      "response from server",
      "DNR should not block requests from system principal"
    );
    // We are loading about:logo for the following reasons:
    // - It is a regular content principal, NOT a system principal.
    // - It is an about:-URL that resolves across all builds (part of toolkit/).
    // - It does not have a CSP (intentional - bug 1587417). That enables us to
    //   send a fetch() request below.
    let contentPage = await ExtensionTestUtils.loadContentPage(
      "about:logo?blockme"
    );
    await contentPage.spawn([], async () => {
      const { document } = content;
      // To make sure that the test does not pass trivially, we verify that it
      // is not the system principal (because dnr_ignores_system_requests
      // already tests that) and not a null principal (because that translates
      // to a void "initiator" in the DNR API, which would pass access checks).
      Assert.ok(
        document.nodePrincipal.isContentPrincipal,
        "about:logo has content principal (not system or NullPrincipal))"
      );
      Assert.equal(document.URL, "about:logo?blockme", "Same URL");
      Assert.equal(
        await (await content.fetch("http://example.com/")).text(),
        "response from server",
        "fetch() at about:logo not blocked by DNR"
      );
    });
    await contentPage.close();
    await extension.unload();
  }
);

add_task(
  { pref_set: [["extensions.dnr.feedback", true]] },
  async function testMatchOutcome_non_system_request_with_disallowed_scheme() {
    let extension = ExtensionTestUtils.loadExtension({
      async background() {
        await browser.declarativeNetRequest.updateSessionRules({
          addRules: [{ id: 1, condition: {}, action: { type: "block" } }],
        });
        const type = "other"; // matches the condition of the above rule.

        browser.test.assertDeepEq(
          { matchedRules: [] },
          await browser.declarativeNetRequest.testMatchOutcome({
            url: "about:logo",
            type,
          }),
          "testMatchOutcome ignores url with disallowed schema"
        );
        browser.test.assertDeepEq(
          { matchedRules: [] },
          await browser.declarativeNetRequest.testMatchOutcome({
            url: "http://example.com/",
            initiator: "about:logo",
            type,
          }),
          "testMatchOutcome ignores initiator with disallowed schema"
        );
        browser.test.sendMessage("done");
      },
      manifest: {
        manifest_version: 3,
        permissions: ["declarativeNetRequest", "declarativeNetRequestFeedback"],
      },
    });
    await extension.startup();
    await extension.awaitMessage("done");
    await extension.unload();
  }
);
