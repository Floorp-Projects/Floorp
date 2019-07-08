/**
 * Test LoginManagerParent._onGeneratedPasswordFilled()
 */

"use strict";

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
const { LoginManagerParent: LMP } = ChromeUtils.import(
  "resource://gre/modules/LoginManagerParent.jsm"
);
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

add_task(async function setup() {
  // Get a profile for storage.
  do_get_profile();

  // Force the feature to be enabled.
  Services.prefs.setBoolPref("signon.generation.available", true);
  Services.prefs.setBoolPref("signon.generation.enabled", true);
});

add_task(async function test_onGeneratedPasswordFilled() {
  ok(LMP._onGeneratedPasswordFilled, "LMP._onGeneratedPasswordFilled exists");
  equal(
    LMP._generatedPasswordsByPrincipalOrigin.size,
    0,
    "Empty cache to start"
  );

  equal(LMP.getGeneratedPassword(99), null, "Null with no BrowsingContext");

  ok(LMP._browsingContextGlobal, "Check _browsingContextGlobal exists");
  ok(
    !LMP._browsingContextGlobal.get(99),
    "BrowsingContext 99 shouldn't exist yet"
  );
  info("Stubbing BrowsingContext.get(99)");
  sinon
    .stub(LMP._browsingContextGlobal, "get")
    .withArgs(99)
    .callsFake(() => {
      return {
        currentWindowGlobal: {
          documentPrincipal: Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(
            "https://www.example.com^userContextId=6"
          ),
        },
      };
    });
  ok(
    LMP._browsingContextGlobal.get(99),
    "Checking BrowsingContext.get(99) stub"
  );

  let password1 = LMP.getGeneratedPassword(99);
  notEqual(password1, null, "Check password was returned");
  equal(password1.length, 15, "Check password length");
  equal(LMP._generatedPasswordsByPrincipalOrigin.size, 1, "1 added to cache");
  equal(
    LMP._generatedPasswordsByPrincipalOrigin.get(
      "https://www.example.com^userContextId=6"
    ).value,
    password1,
    "Cache key and value"
  );

  let storageChangedPromised = TestUtils.topicObserved(
    "passwordmgr-storage-changed",
    (_, data) => data == "addLogin"
  );

  LMP._onGeneratedPasswordFilled({
    browsingContextId: 99,
    formActionOrigin: "https://www.mozilla.org",
  });

  let [login] = await storageChangedPromised;
  let expected = new LoginInfo(
    "https://www.example.com",
    "https://www.mozilla.org",
    null,
    "",
    password1
  );

  ok(login.equals(expected), "Check added login");
  Services.logins.removeAllLogins();

  info("Disable login saving for the site");
  Services.logins.setLoginSavingEnabled("https://www.example.com", false);
  await LMP._onGeneratedPasswordFilled({
    browsingContextId: 99,
    formActionOrigin: "https://www.mozilla.org",
  });
  equal(
    Services.logins.getAllLogins().length,
    0,
    "Should have no saved logins since saving is disabled"
  );
  Services.logins.setLoginSavingEnabled("https://www.example.com", true);
});
