/**
 * Test LoginManagerParent.doAutocompleteSearch()
 */

"use strict";

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
const { LoginManagerParent } = ChromeUtils.import(
  "resource://gre/modules/LoginManagerParent.jsm"
);

// new-password to the happy path
const NEW_PASSWORD_TEMPLATE_ARG = {
  actionOrigin: "https://mozilla.org",
  searchString: "",
  previousResult: null,
  requestId: "foo",
  hasBeenTypePassword: true,
  isSecure: true,
  isProbablyANewPasswordField: true,
};

add_task(async function setup() {
  Services.prefs.setBoolPref("signon.generation.available", true);
  Services.prefs.setBoolPref("signon.generation.enabled", true);

  sinon
    .stub(LoginManagerParent._browsingContextGlobal, "get")
    .withArgs(123)
    .callsFake(() => {
      return {
        currentWindowGlobal: {
          documentPrincipal: Services.scriptSecurityManager.createContentPrincipalFromOrigin(
            "https://www.example.com^userContextId=1"
          ),
        },
      };
    });
});

add_task(async function test_generated_noLogins() {
  let LMP = new LoginManagerParent();
  LMP.useBrowsingContext(123);

  ok(LMP.doAutocompleteSearch, "doAutocompleteSearch exists");

  let result1 = await LMP.doAutocompleteSearch(
    "https://example.com",
    NEW_PASSWORD_TEMPLATE_ARG
  );
  equal(result1.logins.length, 0, "no logins");
  ok(result1.generatedPassword, "has a generated password");
  equal(result1.generatedPassword.length, 15, "generated password length");
  ok(
    result1.willAutoSaveGeneratedPassword,
    "will auto-save when storage is empty"
  );

  info("repeat the search and ensure the same password was used");
  let result2 = await LMP.doAutocompleteSearch(
    "https://example.com",
    NEW_PASSWORD_TEMPLATE_ARG
  );
  equal(result2.logins.length, 0, "no logins");
  equal(
    result2.generatedPassword,
    result1.generatedPassword,
    "same generated password"
  );
  ok(
    result1.willAutoSaveGeneratedPassword,
    "will auto-save when storage is still empty"
  );

  info("Check cases where a password shouldn't be generated");

  let result3 = await LMP.doAutocompleteSearch("https://example.com", {
    ...NEW_PASSWORD_TEMPLATE_ARG,
    ...{
      hasBeenTypePassword: false,
      isProbablyANewPasswordField: false,
    },
  });
  equal(
    result3.generatedPassword,
    null,
    "no generated password when not a pw. field"
  );

  let result4 = await LMP.doAutocompleteSearch("https://example.com", {
    ...NEW_PASSWORD_TEMPLATE_ARG,
    ...{
      // This is false when there is no autocomplete="new-password" attribute &&
      // LoginAutoComplete._isProbablyANewPasswordField returns false
      isProbablyANewPasswordField: false,
    },
  });
  equal(
    result4.generatedPassword,
    null,
    "no generated password when isProbablyANewPasswordField is false"
  );

  LMP.useBrowsingContext(999);
  let result5 = await LMP.doAutocompleteSearch("https://example.com", {
    ...NEW_PASSWORD_TEMPLATE_ARG,
  });
  equal(
    result5.generatedPassword,
    null,
    "no generated password with a missing browsingContextId"
  );
});

add_task(async function test_generated_emptyUsernameSavedLogin() {
  info("Test with a login that will prevent auto-saving");
  await LoginTestUtils.addLogin({
    username: "",
    password: "my-saved-password",
    origin: "https://example.com",
    formActionOrigin: NEW_PASSWORD_TEMPLATE_ARG.actionOrigin,
  });

  let LMP = new LoginManagerParent();
  LMP.useBrowsingContext(123);

  ok(LMP.doAutocompleteSearch, "doAutocompleteSearch exists");

  let result1 = await LMP.doAutocompleteSearch(
    "https://example.com",
    NEW_PASSWORD_TEMPLATE_ARG
  );
  equal(result1.logins.length, 1, "1 login");
  ok(result1.generatedPassword, "has a generated password");
  equal(result1.generatedPassword.length, 15, "generated password length");
  ok(
    !result1.willAutoSaveGeneratedPassword,
    "won't auto-save when an empty-username match is found"
  );

  LoginTestUtils.clearData();
});
