"use strict";

const SIGNUP_DETECTION_HISTOGRAM = "PWMGR_SIGNUP_FORM_DETECTION_MS";
const TEST_URL = `https://example.com${DIRECTORY_PATH}form_signup_detection.html`;

/**
 *
 * @param {Object} histogramData The histogram data to examine
 * @returns The amount of entries found in the histogram data
 */
function countEntries(histogramData) {
  info(typeof histogramData);
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
    let histograms = Services.telemetry.getSnapshotForHistograms("main", false)
      .content;

    histogram = histograms[id];

    return !!histogram && countEntries(histogram) == expected;
  }, `The histogram ${id} was expected to have ${expected} entries.`);
  Assert.equal(countEntries(histogram), expected);
}

add_setup(async () => {
  Services.telemetry.getSnapshotForHistograms("main", true);
});

add_task(async () => {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_URL,
    },
    async function(browser) {
      await SpecialPowers.spawn(browser, [], async () => {
        const doc = content.document;
        const { LoginManagerChild } = ChromeUtils.importESModule(
          "resource://gre/modules/LoginManagerChild.sys.mjs"
        );
        const loginManagerChild = new LoginManagerChild();
        const docState = loginManagerChild.stateForDocument(doc);
        let isSignUpForm;

        info("Test case: Obvious signup form is detected as sign up form");
        const signUpForm = doc.getElementById("obvious-signup-form");
        isSignUpForm = docState.isProbablyASignUpForm(signUpForm);
        Assert.equal(isSignUpForm, true);

        info(
          "Test case: Obvious non signup form is detected as non sign up form"
        );
        const loginForm = doc.getElementById("obvious-login-form");
        isSignUpForm = docState.isProbablyASignUpForm(loginForm);
        Assert.equal(isSignUpForm, false);

        info(
          "Test case: An <input> HTML element is detected as non sign up form"
        );
        const inputField = doc.getElementById("obvious-signup-username");
        isSignUpForm = docState.isProbablyASignUpForm(inputField);
        Assert.equal(isSignUpForm, false);
      });
    }
  );

  info(
    "Test case: After running isProbablyASignUpForm against two <form> HTML elements, the histogram PWMGR_SIGNUP_FORM_DETECTION_MS should have two entries."
  );
  await countEntriesOfChildHistogram(SIGNUP_DETECTION_HISTOGRAM, 2);
});
