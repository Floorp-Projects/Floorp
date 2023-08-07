/* Any copyright is dedicated to the Public Domain.
   http://creativ.orgmons.org/publicdomain/zero/1.0/ */

/**
 * Testing every label of the labeled counter pwmgr.form_autofill_result (Glean metric)
 */

"use strict";

const { AUTOFILL_RESULT } = ChromeUtils.importESModule(
  "resource://gre/modules/LoginManagerChild.sys.mjs"
);
const gAutofillLabels = Object.values(AUTOFILL_RESULT);

const gLogin = LoginTestUtils.testData.formLogin({
  origin: "https://example.org",
  formActionOrigin: "https://example.org/",
  username: "username1",
  password: "password1",
});
const gMultipleLogin = LoginTestUtils.testData.formLogin({
  origin: "https://example.org",
  formActionOrigin: "https://example.org/",
  username: "username2",
  password: "password2",
});
const gLoginInsecureAction = LoginTestUtils.testData.formLogin({
  origin: "https://example.org",
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  formActionOrigin: "http://example.org/",
  username: "username3",
  password: "password3",
});

const TEST_CASES = [
  {
    description: "Autofill result - filled",
    autofill_result: "filled",
    test_url: `https://example.org${DIRECTORY_PATH}form_basic_login.html`,
  },
  {
    description: "Autofill result - no_password_field",
    autofill_result: "no_password_field",
    test_url: `https://example.org${DIRECTORY_PATH}form_multiple_passwords.html`,
  },
  {
    description: "Autofill result - password_disabled_readonly",
    autofill_result: "password_disabled_readonly",
    test_url: `https://example.org${DIRECTORY_PATH}form_disabled_readonly_passwordField.html`,
    form_processed_count: 2,
    metric_count: 2,
  },
  {
    description: "Autofill results no_logins_fit",
    autofill_result: "no_logins_fit",
    test_url: `https://example.org${DIRECTORY_PATH}form_basic_login_fields_with_max_length.html`,
    metric_count: 2,
  },
  {
    description: "Autofill results - no_saved_logins",
    autofill_result: "no_saved_logins",
    test_url: `https://example.com${DIRECTORY_PATH}form_basic_login.html`,
  },
  {
    description: "Autofill results - existing_password",
    autofill_result: "existing_password",
    test_url: `https://example.org${DIRECTORY_PATH}form_basic_prefilled_password.html`,
  },
  {
    description: "Autofill results - existing_username",
    autofill_result: "existing_username",
    test_url: `https://example.org${DIRECTORY_PATH}form_basic_prefilled_username.html`,
  },
  {
    description: "Autofill results - multiple_logins",
    autofill_result: "multiple_logins",
    test_url: `https://example.org${DIRECTORY_PATH}form_basic_login.html`,
    extra_login: gMultipleLogin,
  },
  {
    description: "Autofill results - no_autofill_forms",
    autofill_result: "no_autofill_forms",
    test_url: `https://example.org${DIRECTORY_PATH}form_basic_login.html`,
    prefs: [["signon.autofillForms", false]],
  },
  {
    description: "Autofill results - autocomplete_off",
    autofill_result: "autocomplete_off",
    test_url: `https://example.org${DIRECTORY_PATH}form_basic_password_autocomplete_off.html`,
    prefs: [["signon.autofillForms.autocompleteOff", false]],
  },
  {
    description: "Autofill results - insecure",
    autofill_result: "insecure",
    test_url: `https://example.org${DIRECTORY_PATH}form_cross_origin_insecure_action.html`,
    extra_login: gLoginInsecureAction,
  },
  {
    description: "Autofill results - password_autocomplete_new_password",
    autofill_result: "password_autocomplete_new_password",
    test_url: `https://example.org${DIRECTORY_PATH}form_basic_password_autocomplete_new_password.html`,
  },
  {
    description: "Autofill results - type_no_longer_password",
    autofill_result: "type_no_longer_password",
    test_url: `https://example.org${DIRECTORY_PATH}form_unmasked_password_after_pageload.html`,
  },
  {
    description: "Autofill results - form_in_crossorigin_subframe",
    autofill_result: "form_in_crossorigin_subframe",
    test_url: `https://example.com${DIRECTORY_PATH}form_crossframe_no_outer_login_form.html`,
  },
  {
    description: "Autofill result - filled_username_only_form",
    autofill_result: "filled_username_only_form",
    test_url: `https://example.org${DIRECTORY_PATH}form_username_only.html`,
  },
];

add_setup(async () => {
  await Services.logins.addLoginAsync(gLogin);
});

async function verifyAutofillResults(testCase) {
  info(`Test case: ${testCase.description}`);

  if (testCase.prefs) {
    await SpecialPowers.pushPrefEnv({
      set: testCase.prefs,
    });
  }
  if (testCase.extra_login) {
    await Services.logins.addLoginAsync(testCase.extra_login);
  }

  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  let formProcessed = listenForTestNotification(
    "FormProcessed",
    testCase.form_processed_count ?? 1
  );

  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    testCase.test_url
  );

  await formProcessed;

  await Services.fog.testFlushAllChildren();

  gBrowser.removeTab(tab);

  gAutofillLabels.forEach(label => {
    if (label != testCase.autofill_result) {
      Assert.equal(
        undefined,
        Glean.pwmgr.formAutofillResult[label].testGetValue(),
        `The counter for the label ${label} was not incremented.`
      );
    } else {
      Assert.equal(
        testCase.metric_count ?? 1,
        Glean.pwmgr.formAutofillResult[testCase.autofill_result].testGetValue(),
        `The counter for the label ${label} was incremented by ${
          testCase.metric_count ?? 1
        }.`
      );
    }
  });

  if (testCase.extra_login) {
    await Services.logins.removeLogin(testCase.extra_login);
  }

  if (testCase.prefs) {
    await SpecialPowers.popPrefEnv();
  }
}

add_task(async function test_autofill_results() {
  for (let tc of TEST_CASES) {
    await verifyAutofillResults(tc);
  }
});
