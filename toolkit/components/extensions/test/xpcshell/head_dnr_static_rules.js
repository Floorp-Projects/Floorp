/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* exported assertDNRGetAvailableStaticRuleCount, assertDNRGetDisabledRuleIds,
 *          assertDNRGetEnabledRulesets, assertDNRTestMatchOutcome,
 *          backgroundWithDNRAPICallHandlers, getDNRExtension,
 *          updateStaticRules, updateEnabledRulesets
 */

function backgroundWithDNRAPICallHandlers() {
  browser.test.onMessage.addListener(async (msg, ...args) => {
    let result;
    switch (msg) {
      case "getEnabledRulesets":
        result = await browser.declarativeNetRequest.getEnabledRulesets();
        break;
      case "getAvailableStaticRuleCount":
        result =
          await browser.declarativeNetRequest.getAvailableStaticRuleCount();
        break;
      case "getDisabledRuleIds":
        result = await browser.declarativeNetRequest
          .getDisabledRuleIds(args[0])
          .catch(err => {
            return { rejectedWithErrorMessage: err.message };
          });
        break;
      case "testMatchOutcome":
        result = await browser.declarativeNetRequest
          .testMatchOutcome(...args)
          .catch(err =>
            browser.test.fail(
              `Unexpected rejection from testMatchOutcome call: ${err}`
            )
          );
        break;
      case "updateEnabledRulesets":
        // Run (one or more than one concurrently) updateEnabledRulesets calls
        // and report back the results.
        result = await Promise.all(
          args.map(arg => {
            return browser.declarativeNetRequest
              .updateEnabledRulesets(arg)
              .catch(err => {
                return { rejectedWithErrorMessage: err.message };
              });
          })
        );
        break;
      case "updateStaticRules":
        // Run (one or more than one concurrently) updateEnabledRulesets calls
        // and report back the results.
        result = await Promise.all(
          args.map(arg => {
            return browser.declarativeNetRequest
              .updateStaticRules(arg)
              .catch(err => {
                return { rejectedWithErrorMessage: err.message };
              });
          })
        );
        break;
      default:
        browser.test.fail(`Unexpected test message: ${msg}`);
        return;
    }

    browser.test.sendMessage(`${msg}:done`, result);
  });

  browser.test.sendMessage("bgpage:ready");
}

function getDNRExtension({
  id = "test-dnr-static-rules@test-extension",
  version = "1.0",
  background = backgroundWithDNRAPICallHandlers,
  useAddonManager = "permanent",
  rule_resources,
  declarative_net_request,
  files,
}) {
  // Omit declarative_net_request if rule_resources isn't defined
  // (because declarative_net_request fails the manifest validation
  // if rule_resources is missing).
  const dnr = rule_resources ? { rule_resources } : undefined;

  return {
    background,
    useAddonManager,
    manifest: {
      manifest_version: 3,
      version,
      permissions: ["declarativeNetRequest", "declarativeNetRequestFeedback"],
      // Needed to make sure the upgraded extension will have the same id and
      // same uuid (which is mapped based on the extension id).
      browser_specific_settings: {
        gecko: { id },
      },
      declarative_net_request: declarative_net_request
        ? { ...declarative_net_request, ...(dnr ?? {}) }
        : dnr,
    },
    files,
  };
}

const assertDNRTestMatchOutcome = async (
  { extension, testRequest, expected },
  assertMessage
) => {
  extension.sendMessage("testMatchOutcome", testRequest);
  Assert.deepEqual(
    expected,
    await extension.awaitMessage("testMatchOutcome:done"),
    assertMessage ??
      "Got the expected matched rules from testMatchOutcome API call"
  );
};

const assertDNRGetAvailableStaticRuleCount = async (
  extensionTestWrapper,
  expectedCount,
  assertMessage
) => {
  extensionTestWrapper.sendMessage("getAvailableStaticRuleCount");
  Assert.deepEqual(
    await extensionTestWrapper.awaitMessage("getAvailableStaticRuleCount:done"),
    expectedCount,
    assertMessage ??
      "Got the expected count value from dnr.getAvailableStaticRuleCount API method"
  );
};

const assertDNRGetEnabledRulesets = async (
  extensionTestWrapper,
  expectedRulesetIds
) => {
  extensionTestWrapper.sendMessage("getEnabledRulesets");
  Assert.deepEqual(
    await extensionTestWrapper.awaitMessage("getEnabledRulesets:done"),
    expectedRulesetIds,
    "Got the expected enabled ruleset ids from dnr.getEnabledRulesets API method"
  );
};

const assertDNRGetDisabledRuleIds = async (
  extensionTestWrapper,
  getDisabledRuleIdsArg,
  expectedRuleIds
) => {
  extensionTestWrapper.sendMessage("getDisabledRuleIds", getDisabledRuleIdsArg);
  Assert.deepEqual(
    await extensionTestWrapper.awaitMessage("getDisabledRuleIds:done"),
    expectedRuleIds,
    `Got the expected disabled ruleset ids from dnr.getDisabledRuleIds API method call ${JSON.stringify(
      getDisabledRuleIdsArg
    )}`
  );
};

const updateStaticRules = (extensionTestWrapper, ...callArgs) => {
  extensionTestWrapper.sendMessage("updateStaticRules", ...callArgs);
  return extensionTestWrapper.awaitMessage("updateStaticRules:done");
};

const updateEnabledRulesets = (extensionTestWrapper, ...callArgs) => {
  extensionTestWrapper.sendMessage("updateEnabledRulesets", ...callArgs);
  return extensionTestWrapper.awaitMessage("updateEnabledRulesets:done");
};
