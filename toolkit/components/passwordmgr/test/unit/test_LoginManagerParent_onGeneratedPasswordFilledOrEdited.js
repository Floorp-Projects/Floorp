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
const loginTemplate = Object.freeze({
  origin: "https://www.example.com",
  formActionOrigin: "https://www.mozilla.org",
});

function stubPrompter() {
  let fakePromptToSavePassword = sinon.stub();
  let fakePromptToChangePassword = sinon.stub();
  sinon.stub(LMP, "_getPrompter").callsFake(() => {
    return {
      promptToSavePassword: fakePromptToSavePassword,
      promptToChangePassword: fakePromptToChangePassword,
    };
  });
  LMP._getPrompter().promptToSavePassword();
  LMP._getPrompter().promptToChangePassword();
  ok(LMP._getPrompter.calledTwice, "Checking _getPrompter stub");
  ok(
    fakePromptToSavePassword.calledOnce,
    "Checking fakePromptToSavePassword stub"
  );
  ok(
    fakePromptToChangePassword.calledOnce,
    "Checking fakePromptToChangePassword stub"
  );
  function resetPrompterHistory() {
    LMP._getPrompter.resetHistory();
    fakePromptToSavePassword.resetHistory();
    fakePromptToChangePassword.resetHistory();
  }
  function restorePrompter() {
    LMP._getPrompter.restore();
  }
  resetPrompterHistory();
  return {
    fakePromptToSavePassword,
    fakePromptToChangePassword,
    resetPrompterHistory,
    restorePrompter,
  };
}

function stubGeneratedPasswordForBrowsingContextId(id) {
  ok(LMP._browsingContextGlobal, "Check _browsingContextGlobal exists");
  ok(
    !LMP._browsingContextGlobal.get(id),
    `BrowsingContext ${id} shouldn't exist yet`
  );
  info(`Stubbing BrowsingContext.get(${id})`);
  let stub = sinon
    .stub(LMP._browsingContextGlobal, "get")
    .withArgs(id)
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
    LMP._browsingContextGlobal.get(id),
    `Checking BrowsingContext.get(${id}) stub`
  );

  let generatedPassword = LMP.getGeneratedPassword(id);
  notEqual(generatedPassword, null, "Check password was returned");
  equal(
    generatedPassword.length,
    LoginTestUtils.generation.LENGTH,
    "Check password length"
  );
  equal(LMP._generatedPasswordsByPrincipalOrigin.size, 1, "1 added to cache");
  equal(
    LMP._generatedPasswordsByPrincipalOrigin.get(
      "https://www.example.com^userContextId=6"
    ).value,
    generatedPassword,
    "Cache key and value"
  );
  LMP._browsingContextGlobal.get.resetHistory();

  return {
    stub,
    generatedPassword,
  };
}

function startTestConditions(contextId) {
  ok(
    LMP._onGeneratedPasswordFilledOrEdited,
    "LMP._onGeneratedPasswordFilledOrEdited exists"
  );
  equal(
    LMP.getGeneratedPassword(contextId),
    null,
    "Null with no BrowsingContext"
  );
  equal(
    LMP._generatedPasswordsByPrincipalOrigin.size,
    0,
    "Empty cache to start"
  );
  equal(
    Services.logins.getAllLogins().length,
    0,
    "Should have no saved logins at the start of the test"
  );
}

/*
 * Compare login details excluding usernameField and passwordField
 */
function assertLoginProperties(actualLogin, expected) {
  equal(actualLogin.origin, expected.origin, "Compare origin");
  equal(
    actualLogin.formActionOrigin,
    expected.formActionOrigin,
    "Compare formActionOrigin"
  );
  equal(actualLogin.httpRealm, expected.httpRealm, "Compare httpRealm");
  equal(actualLogin.username, expected.username, "Compare username");
  equal(actualLogin.password, expected.password, "Compare password");
}

add_task(async function setup() {
  // Get a profile for storage.
  do_get_profile();

  // Force the feature to be enabled.
  Services.prefs.setBoolPref("signon.generation.available", true);
  Services.prefs.setBoolPref("signon.generation.enabled", true);
});

add_task(async function test_onGeneratedPasswordFilledOrEdited() {
  startTestConditions(99);
  let { generatedPassword } = stubGeneratedPasswordForBrowsingContextId(99);
  let { fakePromptToChangePassword, restorePrompter } = stubPrompter();

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
    password: generatedPassword,
    username: "someusername",
  });

  let [login] = await storageChangedPromised;
  let expected = new LoginInfo(
    "https://www.example.com",
    "https://www.mozilla.org",
    null,
    "", // verify we don't include the username when auto-saving a login
    generatedPassword
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

  LMP._browsingContextGlobal.get.restore();
  restorePrompter();
  LMP._generatedPasswordsByPrincipalOrigin.clear();
  Services.logins.removeAllLogins();
});

add_task(
  async function test_onGeneratedPasswordFilledOrEdited_withDisabledLogin() {
    startTestConditions(99);
    let { generatedPassword } = stubGeneratedPasswordForBrowsingContextId(99);
    let { restorePrompter, fakePromptToSavePassword } = stubPrompter();

    info("Disable login saving for the site");
    Services.logins.setLoginSavingEnabled("https://www.example.com", false);
    await LMP._onGeneratedPasswordFilledOrEdited({
      browsingContextId: 99,
      formActionOrigin: "https://www.mozilla.org",
      password: generatedPassword,
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
    const newPassword = generatedPassword + "🔥";
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
    const results = snapshot.parent.filter(
      ([time, category, method, object]) => {
        return (
          category === telemetryProps.category &&
          method === telemetryProps.method &&
          object === telemetryProps.object
        );
      }
    );

    equal(
      results.length,
      1,
      "Found telemetry event for generated password editing"
    );

    // Clean up
    LMP._browsingContextGlobal.get.restore();
    restorePrompter();
    LMP._generatedPasswordsByPrincipalOrigin.clear();
    Services.logins.setLoginSavingEnabled("https://www.example.com", true);
    Services.logins.removeAllLogins();
  }
);

add_task(
  async function test_onGeneratedPasswordFilledOrEdited_withSavedEmptyUsername() {
    startTestConditions();
    let login0Props = Object.assign({}, loginTemplate, {
      username: "",
      password: "qweqweq",
    });
    info("Adding initial login: " + JSON.stringify(login0Props));
    await LoginTestUtils.addLogin(login0Props);

    info(
      "Saved initial login: " +
        JSON.stringify(Services.logins.getAllLogins()[0])
    );

    let {
      generatedPassword: password1,
    } = stubGeneratedPasswordForBrowsingContextId(99);
    let { restorePrompter, fakePromptToChangePassword } = stubPrompter();

    await LMP._onGeneratedPasswordFilledOrEdited({
      browsingContextId: 99,
      formActionOrigin: "https://www.mozilla.org",
      password: password1,
    });
    equal(
      Services.logins.getAllLogins().length,
      1,
      "Should just have the previously-saved login with empty username"
    );
    assertLoginProperties(Services.logins.getAllLogins()[0], login0Props);

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
      !fakePromptToChangePassword.getCall(0).args[3],
      "promptToChangePassword had a falsey 'notifySaved' argument"
    );

    LMP._browsingContextGlobal.get.restore();
    restorePrompter();
    LMP._generatedPasswordsByPrincipalOrigin.clear();
    Services.logins.removeAllLogins();
  }
);

add_task(
  async function test_onGeneratedPasswordFilledOrEdited_withEmptyUsernameDifferentFormActionOrigin() {
    startTestConditions();
    let login0Props = Object.assign({}, loginTemplate, {
      username: "",
      password: "qweqweq",
    });
    await LoginTestUtils.addLogin(login0Props);

    let {
      generatedPassword: password1,
    } = stubGeneratedPasswordForBrowsingContextId(99);
    let { restorePrompter, fakePromptToChangePassword } = stubPrompter();

    await LMP._onGeneratedPasswordFilledOrEdited({
      browsingContextId: 99,
      formActionOrigin: "https://www.elsewhere.com",
      password: password1,
    });

    let savedLogins = Services.logins.getAllLogins();
    equal(
      savedLogins.length,
      2,
      "Should have saved the generated-password login"
    );

    assertLoginProperties(savedLogins[0], login0Props);
    assertLoginProperties(
      savedLogins[1],
      Object.assign({}, loginTemplate, {
        formActionOrigin: "https://www.elsewhere.com",
        username: "",
        password: password1,
      })
    );

    ok(LMP._getPrompter.calledOnce, "Checking _getPrompter was called");
    ok(
      fakePromptToChangePassword.calledOnce,
      "Checking promptToChangePassword was called"
    );
    ok(
      fakePromptToChangePassword.getCall(0).args[1],
      "promptToChangePassword had a truthy 'dismissed' argument"
    );
    ok(
      fakePromptToChangePassword.getCall(0).args[2],
      "promptToChangePassword had a truthy 'notifySaved' argument"
    );

    LMP._browsingContextGlobal.get.restore();
    restorePrompter();
    LMP._generatedPasswordsByPrincipalOrigin.clear();
    Services.logins.removeAllLogins();
  }
);

add_task(
  async function test_onGeneratedPasswordFilledOrEdited_withSavedUsername() {
    startTestConditions();
    let login0Props = Object.assign({}, loginTemplate, {
      username: "previoususer",
      password: "qweqweq",
    });
    await LoginTestUtils.addLogin(login0Props);

    let {
      generatedPassword: password1,
    } = stubGeneratedPasswordForBrowsingContextId(99);
    let { restorePrompter, fakePromptToChangePassword } = stubPrompter();

    await LMP._onGeneratedPasswordFilledOrEdited({
      browsingContextId: 99,
      formActionOrigin: "https://www.mozilla.org",
      password: password1,
    });

    let savedLogins = Services.logins.getAllLogins();
    equal(
      savedLogins.length,
      2,
      "Should have saved the generated-password login"
    );
    assertLoginProperties(Services.logins.getAllLogins()[0], login0Props);
    assertLoginProperties(
      savedLogins[1],
      Object.assign({}, loginTemplate, {
        username: "",
        password: password1,
      })
    );

    ok(LMP._getPrompter.calledOnce, "Checking _getPrompter was called");
    ok(
      fakePromptToChangePassword.calledOnce,
      "Checking promptToChangePassword was called"
    );
    ok(
      fakePromptToChangePassword.getCall(0).args[1],
      "promptToChangePassword had a truthy 'dismissed' argument"
    );
    ok(
      fakePromptToChangePassword.getCall(0).args[2],
      "promptToChangePassword had a truthy 'notifySaved' argument"
    );

    LMP._browsingContextGlobal.get.restore();
    restorePrompter();
    LMP._generatedPasswordsByPrincipalOrigin.clear();
    Services.logins.removeAllLogins();
  }
);
