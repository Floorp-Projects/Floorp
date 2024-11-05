/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that drag and drop events are sent at the right time.
// Includes tests for dragging between domains, windows and iframes.

"use strict";

const OUTER_BASE_1 = "https://example.org/browser/dom/events/test/";
const OUTER_BASE_2 = "https://example.com/browser/dom/events/test/";

// iframe domains
const INNER_BASE_1 = OUTER_BASE_1;
const INNER_BASE_2 = OUTER_BASE_2;

// Resolve fn for promise we resolve after mockCA.analyzeContentRequest runs.
let resolveDropPromise;

let testName;

let mockCA = {
  isActive: true,
  mightBeActive: true,
  caShouldAllow: undefined,
  numAnalyzeContentRequestCalls: undefined,

  async analyzeContentRequest(_aRequest, _aAutoAcknowledge) {
    info(`[${testName}]| Called analyzeContentRequest`);
    this.numAnalyzeContentRequestCalls += 1;

    // We want analyzeContentRequest to return before dropPromise is resolved
    // because dropPromise tells the test harness that it is time to check that
    // the drop or dragleave event was received, and that is sent immediately
    // after analyzeContentRequest returns (as part of a promise handler chain).
    setTimeout(resolveDropPromise, 0);

    return this.caShouldAllow
      ? { shouldAllowContent: true }
      : { shouldAllowContent: false };
  },
};

add_setup(async function () {
  // This pref must be set before calling the test's setup function.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.contentanalysis.enabled", true]],
  });
  registerCleanupFunction(async function () {
    SpecialPowers.popPrefEnv();
  });

  mockCA = mockContentAnalysisService(mockCA);

  await setup();
});

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/dom/events/test/browser_dragdrop_impl.js",
  this
);

runTest = async function (
  testRootName,
  sourceBrowsingCxt,
  targetBrowsingCxt,
  dndOptions = {}
) {
  if (
    sourceBrowsingCxt.top == targetBrowsingCxt.top &&
    targetBrowsingCxt.currentWindowGlobal.documentPrincipal.subsumes(
      sourceBrowsingCxt.currentWindowGlobal.documentPrincipal
    )
  ) {
    // Content Analysis should not run.
    info(testRootName);
    testName = testRootName;
    mockCA.numAnalyzeContentRequestCalls = 0;
    await runDnd(testRootName, sourceBrowsingCxt, targetBrowsingCxt, {
      ...dndOptions,
    });
    is(
      mockCA.numAnalyzeContentRequestCalls,
      0,
      `[${testName}]| AnalyzeContentRequest was not called`
    );
    return;
  }

  for (let caShouldAllow of [false, true]) {
    let name = `${testRootName}:${caShouldAllow ? "allow_drop" : "deny_drop"}`;
    info(name);
    testName = name;
    mockCA.caShouldAllow = caShouldAllow;
    mockCA.numAnalyzeContentRequestCalls = 0;
    let dropPromise = new Promise(res => {
      resolveDropPromise = res;
    });
    await runDnd(name, sourceBrowsingCxt, targetBrowsingCxt, {
      dropPromise,
      expectDragLeave: !caShouldAllow,
      ...dndOptions,
    });
    is(
      mockCA.numAnalyzeContentRequestCalls,
      1,
      `[${testName}]| Called AnalyzeContentRequest once`
    );
  }
};
