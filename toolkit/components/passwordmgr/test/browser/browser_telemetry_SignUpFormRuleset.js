"use strict";

const SIGNUP_DETECTION_HISTOGRAM = "PWMGR_SIGNUP_FORM_DETECTION_MS";
const TEST_URL = `https://example.com${DIRECTORY_PATH}form_signup_detection.html`;

/**
 *
 * @param {Object} histogramData The histogram data to examine
 * @returns The amount of entries found in the histogram data
 */
function countEntries(histogramData) {
  return histogramData
    ? Object.values(histogramData.values).reduce((a, b) => a + b, 0)
    : null;
}

/**
 * @param {String} id The histogram to examine
 * @param {Number} expected The expected amount of entries for a histogram
 */
async function countEntriesOfChildHistogram(id, expected) {
  let histogram;
  await TestUtils.waitForCondition(() => {
    let histograms = Services.telemetry.getSnapshotForHistograms(
      "main",
      false
    ).content;

    histogram = histograms[id];

    return !!histogram && countEntries(histogram) == expected;
  }, `The histogram ${id} was expected to have ${expected} entries.`);
  Assert.equal(countEntries(histogram), expected);
}

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["signon.signupDetection.enabled", true]],
  });
  Services.telemetry.getHistogramById(SIGNUP_DETECTION_HISTOGRAM).clear();
});

add_task(async () => {
  let formProcessed = listenForTestNotification("FormProcessed", 2);

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  await formProcessed;

  await countEntriesOfChildHistogram(SIGNUP_DETECTION_HISTOGRAM, 2);

  gBrowser.removeTab(tab);
});
