/**
 * Test LoginManagerParent.getGeneratedPassword()
 */

"use strict";

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
const { LoginManagerParent: LMP } = ChromeUtils.import(
  "resource://gre/modules/LoginManagerParent.jsm"
);

add_task(async function test_getGeneratedPassword() {
  // Force the feature to be enabled.
  Services.prefs.setBoolPref("signon.generation.available", true);
  Services.prefs.setBoolPref("signon.generation.enabled", true);

  ok(LMP.getGeneratedPassword, "LMP.getGeneratedPassword exists");
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
  let password2 = LMP.getGeneratedPassword(99);
  equal(
    password1,
    password2,
    "Same password should be returned for the same origin"
  );

  info("Changing the documentPrincipal to simulate a navigation in the frame");
  LMP._browsingContextGlobal.get.restore();
  sinon
    .stub(LMP._browsingContextGlobal, "get")
    .withArgs(99)
    .callsFake(() => {
      return {
        currentWindowGlobal: {
          documentPrincipal: Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(
            "https://www.mozilla.org^userContextId=2"
          ),
        },
      };
    });
  let password3 = LMP.getGeneratedPassword(99);
  notEqual(
    password2,
    password3,
    "Different password for a different origin for the same BC"
  );
  equal(password3.length, 15, "Check password3 length");

  info("Now checks cases where null should be returned");

  Services.prefs.setBoolPref("signon.rememberSignons", false);
  equal(LMP.getGeneratedPassword(99), null, "Prevented when pwmgr disabled");
  Services.prefs.setBoolPref("signon.rememberSignons", true);

  Services.prefs.setBoolPref("signon.generation.available", false);
  equal(LMP.getGeneratedPassword(99), null, "Prevented when unavailable");
  Services.prefs.setBoolPref("signon.generation.available", true);

  Services.prefs.setBoolPref("signon.generation.enabled", false);
  equal(LMP.getGeneratedPassword(99), null, "Prevented when disabled");
  Services.prefs.setBoolPref("signon.generation.enabled", true);

  equal(
    LMP.getGeneratedPassword(123),
    null,
    "Prevented when browsingContext is missing"
  );
});
