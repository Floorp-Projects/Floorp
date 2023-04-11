/**
 * Test cases for LoginFormState.isProbablyASignUpForm (uses SignUpFormRuleSet fathom model)
 * 1. An obvious sign up form that meets many of the positively weighted rules and leads to score > threshold (default signon.signupDetection.confidenceThreshold)
 * 2. An obvious non sign up form (such as a login form) that meets most of the negatively weighted rules
 * 3. Fathom not running on <input> HTML elements
 */

"use strict";
const TEST_URL = `https://example.com${DIRECTORY_PATH}form_signup_detection.html`;

add_task(async () => {
  info("Test case: Obvious non signup form is detected as non sign up form");
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

        const signUpForm = doc.getElementById("obvious-signup-form");
        isSignUpForm = docState.isProbablyASignUpForm(signUpForm);
        Assert.equal(isSignUpForm, true);

        const loginForm = doc.getElementById("obvious-login-form");
        isSignUpForm = docState.isProbablyASignUpForm(loginForm);
        Assert.equal(isSignUpForm, false);

        const inputField = doc.getElementById("obvious-signup-username");
        isSignUpForm = docState.isProbablyASignUpForm(inputField);
        Assert.equal(isSignUpForm, false);
      });
    }
  );
});
