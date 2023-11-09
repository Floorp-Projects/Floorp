/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Wait for the result of checkFn to equal expectedValue and run an assertion
 * once the value matches.
 * @param {function} checkFn - Function which returns value to compare to
 * expectedValue.
 * @param {*} expectedValue - Value to compare against checkFn return value.
 * @param {string} message - Assertion / test message.
 */
async function waitForAndAssertEqual(checkFn, expectedValue, message) {
  let checkFnResult;
  await BrowserTestUtils.waitForCondition(() => {
    checkFnResult = checkFn();
    return checkFnResult == expectedValue;
  }, message);
  Assert.equal(checkFnResult, expectedValue, message);
}

/**
 * Test that query selector histogram data is set / unset.
 * @param {*} options
 * @param {boolean} options.hasTopLevelData - Whether there should be telemetry
 * data for top level windows.
 * @param {boolean} options.hasFrameData = Whether there should be telemetry
 * data for sub-frames.
 */
async function assertQuerySelectorTelemetry({ hasTopLevelData, hasFrameData }) {
  // Ensure we have all data from the content process.
  await Services.fog.testFlushAllChildren();

  let runDurationTopLevel = () =>
    Glean.cookieBannersClick.querySelectorRunDurationPerWindowTopLevel.testGetValue() !=
    null;
  let runDurationFrame = () =>
    Glean.cookieBannersClick.querySelectorRunDurationPerWindowFrame.testGetValue() !=
    null;

  let runCountTopLevel = () =>
    Glean.cookieBannersClick.querySelectorRunCountPerWindowTopLevel.testGetValue() !=
    null;
  let runCountFrame = () =>
    Glean.cookieBannersClick.querySelectorRunCountPerWindowFrame.testGetValue() !=
    null;

  let messagePrefix = hasData => `Should${hasData ? "" : " not"} have`;

  await waitForAndAssertEqual(
    runCountTopLevel,
    hasTopLevelData,
    `${messagePrefix(hasTopLevelData)} top-level run count data.`
  );
  await waitForAndAssertEqual(
    runDurationTopLevel,
    hasTopLevelData,
    `${messagePrefix(hasTopLevelData)} top-level run duration data.`
  );

  await waitForAndAssertEqual(
    runDurationFrame,
    hasFrameData,
    `${messagePrefix(hasFrameData)} sub-frame run duration data.`
  );
  await waitForAndAssertEqual(
    runCountFrame,
    hasFrameData,
    `${messagePrefix(hasFrameData)} sub-frame run count data.`
  );
}

add_setup(clickTestSetup);

/**
 * Tests that, when performing cookie banner clicking, query selector
 * performance telemetry is collected.
 */
add_task(async function test_click_query_selector_telemetry() {
  // Clear telemetry.
  Services.fog.testResetFOG();

  // Enable cookie banner handling.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  insertTestClickRules();

  info("No telemetry recorded initially.");
  await assertQuerySelectorTelemetry({
    hasFrameData: false,
    hasTopLevelData: false,
  });

  // No opt out rule for the example.org, the banner shouldn't be clicked.
  info("Top level cookie banner with no matching rule.");
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_B,
    testURL: TEST_PAGE_B,
    visible: true,
    expected: "NoClick",
  });
  await assertQuerySelectorTelemetry({
    hasFrameData: false,
    hasTopLevelData: true,
  });

  Services.cookieBanners.removeAllExecutedRecords(false);

  info("Top level cookie banner with matching rule.");
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });
  await assertQuerySelectorTelemetry({
    hasFrameData: false,
    hasTopLevelData: true,
  });

  Services.cookieBanners.removeAllExecutedRecords(false);

  info("Iframe cookie banner with matching rule.");
  await openIframeAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });
  await assertQuerySelectorTelemetry({
    hasFrameData: true,
    hasTopLevelData: true,
  });

  Services.cookieBanners.removeAllExecutedRecords(false);

  // Reset test rules and insert only site-specific rules so we can ensure the
  // clicking mechanism is only running for the iframe, not the top level.
  info("Insert test rules without global rules.");
  insertTestClickRules(false);

  // Clear telemetry.
  info("Clear telemetry.");
  Services.fog.testResetFOG();

  info("Iframe cookie banner with matching rule.");
  await openIframeAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });
  await assertQuerySelectorTelemetry({
    hasFrameData: true,
    hasTopLevelData: false,
  });

  // Clear telemetry.
  Services.fog.testResetFOG();
});
