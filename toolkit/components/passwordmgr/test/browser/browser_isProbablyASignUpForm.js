"use strict";

const TEST_URL = `https://example.com${DIRECTORY_PATH}form_signup_detection.html`;

add_task(async () => {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_URL,
    },
    async function (browser) {
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
});
