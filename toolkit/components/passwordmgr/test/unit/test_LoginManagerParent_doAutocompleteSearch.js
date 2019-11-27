/**
 * Test LoginManagerParent.doAutocompleteSearch()
 */

"use strict";

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
const { LoginManagerParent } = ChromeUtils.import(
  "resource://gre/modules/LoginManagerParent.jsm"
);

add_task(async function test_doAutocompleteSearch_generated_noLogins() {
  Services.prefs.setBoolPref("signon.generation.available", true); // TODO: test both with false
  Services.prefs.setBoolPref("signon.generation.enabled", true);

  let LMP = new LoginManagerParent();
  LMP.useBrowsingContext(123);

  ok(LMP.doAutocompleteSearch, "doAutocompleteSearch exists");

  // Default to the happy path
  let arg1 = {
    autocompleteInfo: {
      section: "",
      addressType: "",
      contactType: "",
      fieldName: "new-password",
      canAutomaticallyPersist: false,
    },
    formOrigin: "https://example.com",
    actionOrigin: "https://mozilla.org",
    searchString: "",
    previousResult: null,
    requestId: "foo",
    isSecure: true,
    isPasswordField: true,
  };

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

  let result1 = await LMP.doAutocompleteSearch(arg1);
  equal(result1.logins.length, 0, "no logins");
  ok(result1.generatedPassword, "has a generated password");
  equal(result1.generatedPassword.length, 15, "generated password length");

  info("repeat the search and ensure the same password was used");
  let result2 = await LMP.doAutocompleteSearch(arg1);
  equal(result2.logins.length, 0, "no logins");
  equal(
    result2.generatedPassword,
    result1.generatedPassword,
    "same generated password"
  );

  info("Check cases where a password shouldn't be generated");

  let result3 = await LMP.doAutocompleteSearch({
    ...arg1,
    ...{ isPasswordField: false },
  });
  equal(
    result3.generatedPassword,
    null,
    "no generated password when not a pw. field"
  );

  let arg1_2 = { ...arg1 };
  arg1_2.autocompleteInfo.fieldName = "";
  let result4 = await LMP.doAutocompleteSearch(arg1_2);
  equal(
    result4.generatedPassword,
    null,
    "no generated password when not autocomplete=new-password"
  );

  let result5 = await LMP.doAutocompleteSearch({
    ...arg1,
    ...{ browsingContextId: 999 },
  });
  equal(
    result5.generatedPassword,
    null,
    "no generated password with a missing browsingContextId"
  );
});
