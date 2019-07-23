/**
 * Test LoginManagerParent._onGeneratedPasswordFilledOrEdited()
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

add_task(async function test_onGeneratedPasswordFilledOrEdited() {
  ok(
    LMP._onGeneratedPasswordFilledOrEdited,
    "LMP._onGeneratedPasswordFilledOrEdited exists"
  );
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
          documentPrincipal: Services.scriptSecurityManager.createContentPrincipalFromOrigin(
            "https://www.example.com^userContextId=6"
          ),
        },
        get top() {
          return this;
        },
      };
    });
  ok(
    LMP._browsingContextGlobal.get(99),
    "Checking BrowsingContext.get(99) stub"
  );

  info("Stubbing _getPrompter");
  let fakePromptToSavePassword = sinon.stub();
  let fakePromptToChangePassword = sinon.stub();
  sinon.stub(LMP, "_getPrompter").callsFake(() => {
    return {
      promptToSavePassword: fakePromptToSavePassword,
      promptToChangePassword: fakePromptToChangePassword,
    };
  });
  LMP._getPrompter().promptToSavePassword();
  ok(LMP._getPrompter.calledOnce, "Checking _getPrompter stub");
  ok(
    fakePromptToSavePassword.calledOnce,
    "Checking fakePromptToSavePassword stub"
  );
  LMP._getPrompter.resetHistory();
  fakePromptToSavePassword.resetHistory();

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

  equal(
    Services.logins.getAllLogins().length,
    0,
    "Should have no saved logins at the start of the test"
  );

  LMP._onGeneratedPasswordFilledOrEdited({
    browsingContextId: 99,
    formActionOrigin: "https://www.mozilla.org",
    password: password1,
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
  ok(LMP._getPrompter.calledOnce, "Checking _getPrompter was called");
  ok(
    fakePromptToChangePassword.calledOnce,
    "Checking promptToChangePassword was called"
  );
  ok(
    fakePromptToChangePassword.getCall(0).args[2],
    "promptToChangePassword had a truthy 'dismissed' argument"
  );
  ok(
    fakePromptToChangePassword.getCall(0).args[3],
    "promptToChangePassword had a truthy 'notifySaved' argument"
  );

  LMP._getPrompter.resetHistory();
  fakePromptToChangePassword.resetHistory();

  Services.logins.removeAllLogins();

  info("Disable login saving for the site");
  Services.logins.setLoginSavingEnabled("https://www.example.com", false);
  await LMP._onGeneratedPasswordFilledOrEdited({
    browsingContextId: 99,
    formActionOrigin: "https://www.mozilla.org",
    password: password1,
  });
  equal(
    Services.logins.getAllLogins().length,
    0,
    "Should have no saved logins since saving is disabled"
  );

  ok(LMP._getPrompter.calledOnce, "Checking _getPrompter was called");
  ok(
    fakePromptToSavePassword.calledOnce,
    "Checking promptToSavePassword was called"
  );
  ok(
    fakePromptToSavePassword.getCall(0).args[1],
    "promptToSavePassword had a truthy 'dismissed' argument"
  );
  ok(
    !fakePromptToSavePassword.getCall(0).args[2],
    "promptToSavePassword had a falsey 'notifySaved' argument"
  );

  // Edit the password
  const newPassword = password1 + "🔥";
  await LMP._onGeneratedPasswordFilledOrEdited({
    browsingContextId: 99,
    formActionOrigin: "https://www.mozilla.org",
    password: newPassword,
  });
  ok(
    LMP._generatedPasswordsByPrincipalOrigin.get(
      "https://www.example.com^userContextId=6"
    ).edited,
    "Cached edited state should be true"
  );

  // Simulate a second edit to check that the telemetry event for the first edit
  // is not recorded twice
  const newerPassword = newPassword + "🦊";
  await LMP._onGeneratedPasswordFilledOrEdited({
    browsingContextId: 99,
    formActionOrigin: "https://www.mozilla.org",
    password: newerPassword,
  });
  ok(
    LMP._generatedPasswordsByPrincipalOrigin.get(
      "https://www.example.com^userContextId=6"
    ).edited,
    "Cached edited state should remain true"
  );

  // Check that expected telemetry event was recorded
  const snapshot = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    false
  );
  const telemetryProps = Object.freeze({
    category: "pwmgr",
    method: "filled_field_edited",
    object: "generatedpassword",
  });
  const results = snapshot.parent.filter(([time, category, method, object]) => {
    return (
      category === telemetryProps.category &&
      method === telemetryProps.method &&
      object === telemetryProps.object
    );
  });

  equal(
    results.length,
    1,
    "Found telemetry event for generated password editing"
  );

  // Clean up
  LMP._getPrompter.resetHistory();
  fakePromptToSavePassword.resetHistory();

  Services.logins.setLoginSavingEnabled("https://www.example.com", true);
});
