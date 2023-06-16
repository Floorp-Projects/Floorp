"use strict";

// This test verifies that the internals for associating requests with tabId
// are only active when a session rule with a tabId rule exists.
//
// There are tests for the logic of tabId matching in the match_tabIds task in
// toolkit/components/extensions/test/xpcshell/test_ext_dnr_testMatchOutcome.js
//
// And there are tests that verify matching with real network requests in
// toolkit/components/extensions/test/mochitest/test_ext_dnr_tabIds.html

const server = createHttpServer({ hosts: ["from", "any", "in", "ex"] });
server.registerPathHandler("/", (req, res) => {
  res.setHeader("Access-Control-Allow-Origin", "*");
});

let gTabLookupSpy;

add_setup(async () => {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.enabled", true);

  // Install a spy on WebRequest.getTabIdForChannelWrapper.
  const { WebRequest } = ChromeUtils.importESModule(
    "resource://gre/modules/WebRequest.sys.mjs"
  );
  const { sinon } = ChromeUtils.importESModule(
    "resource://testing-common/Sinon.sys.mjs"
  );
  gTabLookupSpy = sinon.spy(WebRequest, "getTabIdForChannelWrapper");

  await ExtensionTestUtils.startAddonManager();
});

function numberOfTabLookupsSinceLastCheck() {
  let result = gTabLookupSpy.callCount;
  gTabLookupSpy.resetHistory();
  return result;
}

// This test checks that WebRequest.getTabIdForChannelWrapper is only called
// when there are any registered tabId/excludedTabIds rules. Moreover, it
// verifies that after unloading (reloading) the extension, that the method is
// still not called unnecessarily.
add_task(async function getTabIdForChannelWrapper_only_called_when_needed() {
  async function background() {
    const RULE_ANY_TAB_ID = {
      id: 1,
      condition: { requestDomains: ["from"] },
      action: { type: "redirect", redirect: { url: "http://any/" } },
    };
    const RULE_INCLUDE_TAB_ID = {
      id: 2,
      condition: { requestDomains: ["from"], tabIds: [-1] },
      action: { type: "redirect", redirect: { url: "http://in/" } },
      priority: 2,
    };
    const RULE_EXCLUDE_TAB_ID = {
      id: 3,
      condition: { requestDomains: ["from"], excludedTabIds: [-1] },
      action: { type: "redirect", redirect: { url: "http://ex/" } },
      priority: 2,
    };
    async function promiseOneMessage(messageName) {
      return new Promise(resolve => {
        browser.test.onMessage.addListener(function listener(msg, result) {
          if (messageName === msg) {
            browser.test.onMessage.removeListener(listener);
            resolve(result);
          }
        });
      });
    }
    async function numberOfTabLookupsSinceLastCheck() {
      let promise = promiseOneMessage("tabLookups");
      browser.test.sendMessage("getTabLookups");
      return promise;
    }
    async function testFetchUrl(url, expectedUrl, expectedCount, description) {
      let res = await fetch(url);
      browser.test.assertEq(expectedUrl, res.url, `Final URL for ${url}`);
      browser.test.assertEq(
        expectedCount,
        await numberOfTabLookupsSinceLastCheck(),
        `Expected number of tab lookups - ${url} - ${description}`
      );
    }

    const startupCountPromise = promiseOneMessage("extensionStartupCount");
    browser.test.sendMessage("extensionStarted");
    const startupCount = await startupCountPromise;
    if (startupCount !== 0) {
      browser.test.assertEq(1, startupCount, "Extension restarted once");

      // Note: declarativeNetRequest.updateSessionRules is intentionally not
      // called here, because we want to verify that upon unloading the
      // extension, that the tabId lookup logic was properly cleaned up,
      // i.e. that NetworkIntegration.maybeUpdateTabIdChecker() was called.

      await testFetchUrl(
        "http://from/?after-restart-supposedly-no-include-tab",
        "http://from/?after-restart-supposedly-no-include-tab",
        0,
        "No lookup because session rules should have disappeared at reload"
      );

      browser.test.assertDeepEq(
        [],
        await browser.declarativeNetRequest.getSessionRules(),
        "The session rules have indeed been cleared upon reload."
      );

      browser.test.sendMessage("test_completed_after_reload");
      return;
    }

    browser.test.assertEq(
      0,
      await numberOfTabLookupsSinceLastCheck(),
      "Initially, no tab lookups"
    );

    await testFetchUrl(
      "http://from/?no_dnr_rules",
      "http://from/?no_dnr_rules",
      0,
      "No tab lookups without any registered DNR rules"
    );

    await browser.declarativeNetRequest.updateSessionRules({
      addRules: [RULE_ANY_TAB_ID],
    });
    // Active rules now: RULE_ANY_TAB_ID

    await testFetchUrl(
      "http://from/?only_dnr_rule_matches_any_tab",
      "http://any/",
      0,
      "No tab lookups when only rule has no tabIds/excludedTabIds conditions"
    );

    await browser.declarativeNetRequest.updateSessionRules({
      addRules: [RULE_EXCLUDE_TAB_ID],
    });
    // Active rules now: RULE_ANY_TAB_ID, RULE_EXCLUDE_TAB_ID

    await testFetchUrl(
      "http://from/?dnr_rule_matches_any,dnr_rule_excludes_-1",
      // should be "any" instead of "ex" because excludedTabIds: [-1] should
      // exclude the background.
      "http://any/",
      2, // initial request + redirect request.
      "Expected tabId lookup when a tabId rule is registered"
    );

    await browser.declarativeNetRequest.updateSessionRules({
      removeRuleIds: [RULE_ANY_TAB_ID.id],
    });
    // Active rules now: RULE_EXCLUDE_TAB_ID

    await testFetchUrl(
      "http://from/?only_dnr_rule_excludes_-1",
      // Not redirected to "ex" because excludedTabIds: [-1] does not match the
      // background that has tabId -1.
      "http://from/?only_dnr_rule_excludes_-1",
      1,
      "Expected lookup after unregistering unrelated rule, keeping tabId rule"
    );

    await browser.declarativeNetRequest.updateSessionRules({
      addRules: [RULE_INCLUDE_TAB_ID],
    });
    // Active rules now: RULE_EXCLUDE_TAB_ID, RULE_INCLUDE_TAB_ID
    await testFetchUrl(
      "http://from/?two_dnr_rule_include_and_exclude_-1",
      "http://in/",
      2, // initial request + redirect request.
      "Expecting lookup because of 2 DNR rules with tabId and excludedTabIds"
    );

    await browser.declarativeNetRequest.updateSessionRules({
      removeRuleIds: [RULE_EXCLUDE_TAB_ID.id],
    });
    // Active rules now: RULE_INCLUDE_TAB_ID

    await testFetchUrl(
      "http://from/?only_dnr_rule_includes_-1",
      "http://in/",
      2, // initial request + redirect request.
      "Expecting lookup because of remaining tabId DNR rule"
    );

    await browser.declarativeNetRequest.updateSessionRules({
      removeRuleIds: [RULE_INCLUDE_TAB_ID.id],
    });
    // Active rules now: none

    await testFetchUrl(
      "http://from/?no_rules_again",
      "http://from/?no_rules_again",
      0,
      "Expected no lookups after unregistering the last remaining rule"
    );

    await browser.declarativeNetRequest.updateSessionRules({
      addRules: [RULE_INCLUDE_TAB_ID],
    });
    // Active rules now: RULE_INCLUDE_TAB_ID

    await testFetchUrl(
      "http://from/?again_with-include-1",
      "http://in/",
      2, // initial request + redirect request.
      "Expecting lookup again because of include rule"
    );

    // Ending test with remaining rule: RULE_INCLUDE_TAB_ID
    // Reload extension.
    browser.test.sendMessage("reload_extension");
  }
  let extension = ExtensionTestUtils.loadExtension({
    background,
    useAddonManager: "temporary", // for reload and granted_host_permissions.
    allowInsecureRequests: true,
    manifest: {
      manifest_version: 3,
      host_permissions: ["*://from/*"],
      granted_host_permissions: true,
      permissions: ["declarativeNetRequest"],
    },
  });
  extension.onMessage("getTabLookups", () => {
    extension.sendMessage("tabLookups", numberOfTabLookupsSinceLastCheck());
  });
  let startupCount = 0;
  extension.onMessage("extensionStarted", () => {
    extension.sendMessage("extensionStartupCount", startupCount++);
  });
  await extension.startup();
  await extension.awaitMessage("reload_extension");
  await extension.addon.reload();
  await extension.awaitMessage("test_completed_after_reload");
  Assert.equal(
    0,
    numberOfTabLookupsSinceLastCheck(),
    "No new tab lookups since completion of extension tests"
  );
  await extension.unload();
});
