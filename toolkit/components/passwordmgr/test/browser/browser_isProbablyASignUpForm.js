"use strict";

const TEST_URL = `https://example.com${DIRECTORY_PATH}form_signup_detection.html`;

add_task(async () => {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_URL,
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        const doc = content.document;
        const { FormScenarios } = ChromeUtils.importESModule(
          "resource://gre/modules/FormScenarios.sys.mjs"
        );

        function assertSignUpForm(
          message,
          formId,
          inputId,
          expectedToBeSignUp
        ) {
          const isSignUpForm = !!FormScenarios.detect({
            form: doc.getElementById(formId),
            input: doc.getElementById(inputId),
          }).signUpForm;
          Assert.equal(isSignUpForm, expectedToBeSignUp, message);
        }

        assertSignUpForm(
          "Obvious signup form is detected as sign up form",
          "obvious-signup-form",
          "obvious-signup-email",
          true
        );
        assertSignUpForm(
          "Obvious non signup form is detected as non sign up form",
          "obvious-login-form",
          "obvious-login-username",
          false
        );
        assertSignUpForm(
          "An <input> HTML element is detected as non sign up form",
          "",
          "standalone-username",
          false
        );
      });
    }
  );
});
