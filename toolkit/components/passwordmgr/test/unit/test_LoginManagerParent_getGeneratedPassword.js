/**
 * Test LoginManagerParent.getGeneratedPassword()
 */

"use strict";

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
const { LoginManagerParent } = ChromeUtils.import(
  "resource://gre/modules/LoginManagerParent.jsm"
);

add_task(async function test_getGeneratedPassword() {
  // Force the feature to be enabled.
  Services.prefs.setBoolPref("signon.generation.available", true);
  Services.prefs.setBoolPref("signon.generation.enabled", true);

  // Setup the improved rules collection since the improved password rules
  // pref is on by default. Otherwise any interaciton with LMP.getGeneratedPassword()
  // will take a long time to complete
  if (LoginHelper.improvedPasswordRulesEnabled) {
    await LoginTestUtils.remoteSettings.setupImprovedPasswordRules();
  }

  let LMP = new LoginManagerParent();
  LMP.useBrowsingContext(99);

  ok(LMP.getGeneratedPassword, "LMP.getGeneratedPassword exists");
  equal(
    LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().size,
    0,
    "Empty cache to start"
  );

  equal(await LMP.getGeneratedPassword(), null, "Null with no BrowsingContext");

  ok(
    LoginManagerParent._browsingContextGlobal,
    "Check _browsingContextGlobal exists"
  );
  ok(
    !LoginManagerParent._browsingContextGlobal.get(99),
    "BrowsingContext 99 shouldn't exist yet"
  );
  info("Stubbing BrowsingContext.get(99)");
  sinon
    .stub(LoginManagerParent._browsingContextGlobal, "get")
    .withArgs(99)
    .callsFake(() => {
      return {
        currentWindowGlobal: {
          documentPrincipal: Services.scriptSecurityManager.createContentPrincipalFromOrigin(
            "https://www.example.com^userContextId=6"
          ),
          documentURI: Services.io.newURI("https://www.example.com"),
        },
      };
    });
  ok(
    LoginManagerParent._browsingContextGlobal.get(99),
    "Checking BrowsingContext.get(99) stub"
  );
  let password1 = await LMP.getGeneratedPassword();
  notEqual(password1, null, "Check password was returned");
  equal(
    password1.length,
    LoginTestUtils.generation.LENGTH,
    "Check password length"
  );
  equal(
    LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().size,
    1,
    "1 added to cache"
  );
  equal(
    LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().get(
      "https://www.example.com^userContextId=6"
    ).value,
    password1,
    "Cache key and value"
  );
  let password2 = await LMP.getGeneratedPassword();
  equal(
    password1,
    password2,
    "Same password should be returned for the same origin"
  );

  info("Changing the documentPrincipal to simulate a navigation in the frame");
  LoginManagerParent._browsingContextGlobal.get.restore();
  sinon
    .stub(LoginManagerParent._browsingContextGlobal, "get")
    .withArgs(99)
    .callsFake(() => {
      return {
        currentWindowGlobal: {
          documentPrincipal: Services.scriptSecurityManager.createContentPrincipalFromOrigin(
            "https://www.mozilla.org^userContextId=2"
          ),
          documentURI: Services.io.newURI("https://www.example.com"),
        },
      };
    });
  let password3 = await LMP.getGeneratedPassword();
  notEqual(
    password2,
    password3,
    "Different password for a different origin for the same BC"
  );
  equal(
    password3.length,
    LoginTestUtils.generation.LENGTH,
    "Check password3 length"
  );

  info("Now checks cases where null should be returned");

  Services.prefs.setBoolPref("signon.rememberSignons", false);
  equal(
    await LMP.getGeneratedPassword(),
    null,
    "Prevented when pwmgr disabled"
  );
  Services.prefs.setBoolPref("signon.rememberSignons", true);

  Services.prefs.setBoolPref("signon.generation.available", false);
  equal(await LMP.getGeneratedPassword(), null, "Prevented when unavailable");
  Services.prefs.setBoolPref("signon.generation.available", true);

  Services.prefs.setBoolPref("signon.generation.enabled", false);
  equal(await LMP.getGeneratedPassword(), null, "Prevented when disabled");
  Services.prefs.setBoolPref("signon.generation.enabled", true);

  LMP.useBrowsingContext(123);
  equal(
    await LMP.getGeneratedPassword(),
    null,
    "Prevented when browsingContext is missing"
  );
});
