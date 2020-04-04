/**
 * Test LoginManagerParent._onPasswordEditedOrGenerated()
 */

"use strict";

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
const { LoginManagerParent } = ChromeUtils.import(
  "resource://gre/modules/LoginManagerParent.jsm"
);
const { LoginManagerPrompter } = ChromeUtils.import(
  "resource://gre/modules/LoginManagerPrompter.jsm"
);
const { PopupNotifications } = ChromeUtils.import(
  "resource://gre/modules/PopupNotifications.jsm"
);

const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);
const loginTemplate = Object.freeze({
  origin: "https://www.example.com",
  formActionOrigin: "https://www.mozilla.org",
});

let LMP = new LoginManagerParent();

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
  ok(
    LoginManagerParent._browsingContextGlobal,
    "Check _browsingContextGlobal exists"
  );
  ok(
    !LoginManagerParent._browsingContextGlobal.get(id),
    `BrowsingContext ${id} shouldn't exist yet`
  );
  info(`Stubbing BrowsingContext.get(${id})`);
  let stub = sinon
    .stub(LoginManagerParent._browsingContextGlobal, "get")
    .withArgs(id)
    .callsFake(() => {
      return {
        currentWindowGlobal: {
          documentPrincipal: Services.scriptSecurityManager.createContentPrincipalFromOrigin(
            "https://www.example.com^userContextId=6"
          ),
        },
        get embedderElement() {
          info("returning embedderElement");
          let browser = MockDocument.createTestDocument(
            "chrome://browser/content/browser.xhtml",
            `<box xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul">
              <browser></browser>
             </box>`,
            "application/xml",
            true
          ).querySelector("browser");
          MockDocument.mockBrowsingContextProperty(browser, this);
          return browser;
        },
        get top() {
          return this;
        },
      };
    });
  ok(
    LoginManagerParent._browsingContextGlobal.get(id),
    `Checking BrowsingContext.get(${id}) stub`
  );

  let generatedPassword = LMP.getGeneratedPassword(id);
  notEqual(generatedPassword, null, "Check password was returned");
  equal(
    generatedPassword.length,
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
    generatedPassword,
    "Cache key and value"
  );
  LoginManagerParent._browsingContextGlobal.get.resetHistory();

  return {
    stub,
    generatedPassword,
  };
}

function checkEditTelemetryRecorded(expectedCount, msg) {
  info("Check that expected telemetry event was recorded");
  const snapshot = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    false
  );
  let resultsCount = 0;
  if ("parent" in snapshot) {
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
    resultsCount = results.length;
  }
  equal(
    resultsCount,
    expectedCount,
    "Check count of pwmgr.filled_field_edited for generatedpassword: " + msg
  );
}

function startTestConditions(contextId) {
  LMP.useBrowsingContext(contextId);

  ok(
    LMP._onPasswordEditedOrGenerated,
    "LMP._onPasswordEditedOrGenerated exists"
  );
  equal(LMP.getGeneratedPassword(), null, "Null with no BrowsingContext");
  equal(
    LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().size,
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

add_task(async function test_onPasswordEditedOrGenerated_generatedPassword() {
  startTestConditions(99);
  let { generatedPassword } = stubGeneratedPasswordForBrowsingContextId(99);
  let { fakePromptToChangePassword, restorePrompter } = stubPrompter();
  let rootBrowser = LMP.getRootBrowser();

  let storageChangedPromised = TestUtils.topicObserved(
    "passwordmgr-storage-changed",
    (_, data) => data == "addLogin"
  );

  equal(
    Services.logins.getAllLogins().length,
    0,
    "Should have no saved logins at the start of the test"
  );

  await LMP._onPasswordEditedOrGenerated(rootBrowser, {
    browsingContextId: 99,
    origin: "https://www.example.com",
    formActionOrigin: "https://www.mozilla.org",
    newPasswordField: { value: generatedPassword },
    usernameField: { value: "someusername" },
    triggeredByFillingGenerated: true,
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
    fakePromptToChangePassword.getCall(0).args[3],
    "promptToChangePassword had a truthy 'dismissed' argument"
  );
  ok(
    fakePromptToChangePassword.getCall(0).args[4],
    "promptToChangePassword had a truthy 'notifySaved' argument"
  );

  info("Edit the password");
  const newPassword = generatedPassword + "🔥";
  storageChangedPromised = TestUtils.topicObserved(
    "passwordmgr-storage-changed",
    (_, data) => data == "modifyLogin"
  );
  await LMP._onPasswordEditedOrGenerated(rootBrowser, {
    browsingContextId: 99,
    origin: "https://www.example.com",
    formActionOrigin: "https://www.mozilla.org",
    newPasswordField: { value: newPassword },
    usernameField: { value: "someusername" },
    triggeredByFillingGenerated: true,
  });
  let generatedPW = LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().get(
    "https://www.example.com^userContextId=6"
  );
  ok(generatedPW.edited, "Cached edited boolean should be true");
  equal(generatedPW.value, newPassword, "Cached password should be updated");
  // login metadata should be updated
  let [dataArray] = await storageChangedPromised;
  login = dataArray.queryElementAt(1, Ci.nsILoginInfo);
  expected.password = newPassword;
  ok(login.equals(expected), "Check updated login");
  equal(
    Services.logins.getAllLogins().length,
    1,
    "Should have 1 saved login still"
  );

  info(
    "Simulate a second edit to check that the telemetry event for the first edit is not recorded twice"
  );
  const newerPassword = newPassword + "🦊";
  storageChangedPromised = TestUtils.topicObserved(
    "passwordmgr-storage-changed",
    (_, data) => data == "modifyLogin"
  );
  await LMP._onPasswordEditedOrGenerated(rootBrowser, {
    browsingContextId: 99,
    origin: "https://www.example.com",
    formActionOrigin: "https://www.mozilla.org",
    newPasswordField: { value: newerPassword },
    usernameField: { value: "someusername" },
    triggeredByFillingGenerated: true,
  });
  generatedPW = LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().get(
    "https://www.example.com^userContextId=6"
  );
  ok(generatedPW.edited, "Cached edited state should remain true");
  equal(generatedPW.value, newerPassword, "Cached password should be updated");
  [dataArray] = await storageChangedPromised;
  login = dataArray.queryElementAt(1, Ci.nsILoginInfo);
  expected.password = newerPassword;
  ok(login.equals(expected), "Check updated login");
  equal(
    Services.logins.getAllLogins().length,
    1,
    "Should have 1 saved login still"
  );

  checkEditTelemetryRecorded(1, "with auto-save");

  LoginManagerParent._browsingContextGlobal.get.restore();
  restorePrompter();
  LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().clear();
  Services.logins.removeAllLogins();
  Services.telemetry.clearEvents();
});

add_task(
  async function test_onPasswordEditedOrGenerated_editToEmpty_generatedPassword() {
    startTestConditions(99);
    let { generatedPassword } = stubGeneratedPasswordForBrowsingContextId(99);
    let { fakePromptToChangePassword, restorePrompter } = stubPrompter();
    let rootBrowser = LMP.getRootBrowser();

    let storageChangedPromised = TestUtils.topicObserved(
      "passwordmgr-storage-changed",
      (_, data) => data == "addLogin"
    );

    equal(
      Services.logins.getAllLogins().length,
      0,
      "Should have no saved logins at the start of the test"
    );

    await LMP._onPasswordEditedOrGenerated(rootBrowser, {
      browsingContextId: 99,
      origin: "https://www.example.com",
      formActionOrigin: "https://www.mozilla.org",
      newPasswordField: { value: generatedPassword },
      usernameField: { value: "someusername" },
      triggeredByFillingGenerated: true,
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
      fakePromptToChangePassword.getCall(0).args[3],
      "promptToChangePassword had a truthy 'dismissed' argument"
    );
    ok(
      fakePromptToChangePassword.getCall(0).args[4],
      "promptToChangePassword had a truthy 'notifySaved' argument"
    );

    info("Edit the password to be empty");
    const newPassword = "";
    await LMP._onPasswordEditedOrGenerated(rootBrowser, {
      browsingContextId: 99,
      origin: "https://www.example.com",
      formActionOrigin: "https://www.mozilla.org",
      newPasswordField: { value: newPassword },
      usernameField: { value: "someusername" },
      triggeredByFillingGenerated: true,
    });
    let generatedPW = LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().get(
      "https://www.example.com^userContextId=6"
    );
    ok(!generatedPW.edited, "Cached edited boolean should be false");
    equal(
      generatedPW.value,
      generatedPassword,
      "Cached password shouldn't be updated"
    );

    checkEditTelemetryRecorded(0, "Blanking doesn't count as an edit");

    LoginManagerParent._browsingContextGlobal.get.restore();
    restorePrompter();
    LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().clear();
    Services.logins.removeAllLogins();
    Services.telemetry.clearEvents();
  }
);

add_task(async function test_addUsernameBeforeAutoSaveEdit() {
  startTestConditions(99);
  let { generatedPassword } = stubGeneratedPasswordForBrowsingContextId(99);
  let {
    fakePromptToChangePassword,
    restorePrompter,
    resetPrompterHistory,
  } = stubPrompter();
  let rootBrowser = LMP.getRootBrowser();
  let fakePopupNotifications = {
    getNotification: sinon.stub().returns({ dismissed: true }),
  };
  sinon.stub(LoginHelper, "getBrowserForPrompt").callsFake(() => {
    return {
      ownerGlobal: {
        PopupNotifications: fakePopupNotifications,
      },
    };
  });

  let storageChangedPromised = TestUtils.topicObserved(
    "passwordmgr-storage-changed",
    (_, data) => data == "addLogin"
  );

  equal(
    Services.logins.getAllLogins().length,
    0,
    "Should have no saved logins at the start of the test"
  );

  await LMP._onPasswordEditedOrGenerated(rootBrowser, {
    browsingContextId: 99,
    origin: "https://www.example.com",
    formActionOrigin: "https://www.mozilla.org",
    newPasswordField: { value: generatedPassword },
    usernameField: { value: "someusername" },
    triggeredByFillingGenerated: true,
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
    fakePromptToChangePassword.getCall(0).args[3],
    "promptToChangePassword had a truthy 'dismissed' argument"
  );
  ok(
    fakePromptToChangePassword.getCall(0).args[4],
    "promptToChangePassword had a truthy 'notifySaved' argument"
  );

  info("Checking the getNotification stub");
  ok(
    !fakePopupNotifications.getNotification.called,
    "getNotification didn't get called yet"
  );
  resetPrompterHistory();

  info("Add a username to the auto-saved login in storage");
  let loginWithUsername = login.clone();
  loginWithUsername.username = "added_username";
  LoginManagerPrompter._updateLogin(login, loginWithUsername);

  info("Edit the password");
  const newPassword = generatedPassword + "🔥";
  storageChangedPromised = TestUtils.topicObserved(
    "passwordmgr-storage-changed",
    (_, data) => data == "modifyLogin"
  );
  // will update the doorhanger with changed password
  await LMP._onPasswordEditedOrGenerated(rootBrowser, {
    browsingContextId: 99,
    origin: "https://www.example.com",
    formActionOrigin: "https://www.mozilla.org",
    newPasswordField: { value: newPassword },
    usernameField: { value: "someusername" },
    triggeredByFillingGenerated: true,
  });
  let generatedPW = LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().get(
    "https://www.example.com^userContextId=6"
  );
  ok(generatedPW.edited, "Cached edited boolean should be true");
  equal(generatedPW.value, newPassword, "Cached password should be updated");
  let [dataArray] = await storageChangedPromised;
  login = dataArray.queryElementAt(1, Ci.nsILoginInfo);
  loginWithUsername.password = newPassword;
  // the password should be updated in storage, but not the username (until the user confirms the doorhanger)
  assertLoginProperties(login, loginWithUsername);
  ok(login.matches(loginWithUsername, false), "Check updated login");
  equal(
    Services.logins.getAllLogins().length,
    1,
    "Should have 1 saved login still"
  );

  ok(
    fakePopupNotifications.getNotification.calledOnce,
    "getNotification was called"
  );
  ok(LMP._getPrompter.calledOnce, "Checking _getPrompter was called");
  ok(
    fakePromptToChangePassword.calledOnce,
    "Checking promptToChangePassword was called"
  );
  ok(
    fakePromptToChangePassword.getCall(0).args[3],
    "promptToChangePassword had a truthy 'dismissed' argument"
  );
  // The generated password changed, so we expect notifySaved to be true
  ok(
    fakePromptToChangePassword.getCall(0).args[4],
    "promptToChangePassword should have a falsey 'notifySaved' argument"
  );
  resetPrompterHistory();

  info(
    "Simulate a second edit to check that the telemetry event for the first edit is not recorded twice"
  );
  const newerPassword = newPassword + "🦊";
  storageChangedPromised = TestUtils.topicObserved(
    "passwordmgr-storage-changed",
    (_, data) => data == "modifyLogin"
  );
  info("Calling _onPasswordEditedOrGenerated again");
  await LMP._onPasswordEditedOrGenerated(rootBrowser, {
    browsingContextId: 99,
    origin: "https://www.example.com",
    formActionOrigin: "https://www.mozilla.org",
    newPasswordField: { value: newerPassword },
    usernameField: { value: "someusername" },
    triggeredByFillingGenerated: true,
  });
  generatedPW = LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().get(
    "https://www.example.com^userContextId=6"
  );
  ok(generatedPW.edited, "Cached edited state should remain true");
  equal(generatedPW.value, newerPassword, "Cached password should be updated");
  [dataArray] = await storageChangedPromised;
  login = dataArray.queryElementAt(1, Ci.nsILoginInfo);
  loginWithUsername.password = newerPassword;
  assertLoginProperties(login, loginWithUsername);
  ok(login.matches(loginWithUsername, false), "Check updated login");
  equal(
    Services.logins.getAllLogins().length,
    1,
    "Should have 1 saved login still"
  );

  checkEditTelemetryRecorded(1, "with auto-save");

  ok(
    fakePromptToChangePassword.calledOnce,
    "Checking promptToChangePassword was called"
  );
  equal(
    fakePromptToChangePassword.getCall(0).args[2].password,
    newerPassword,
    "promptToChangePassword had the updated password"
  );
  ok(
    fakePromptToChangePassword.getCall(0).args[3],
    "promptToChangePassword had a truthy 'dismissed' argument"
  );

  LoginManagerParent._browsingContextGlobal.get.restore();
  LoginHelper.getBrowserForPrompt.restore();
  restorePrompter();
  LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().clear();
  Services.logins.removeAllLogins();
  Services.telemetry.clearEvents();
});

add_task(async function test_editUsernameOfFilledSavedLogin() {
  startTestConditions(99);
  stubGeneratedPasswordForBrowsingContextId(99);
  let {
    fakePromptToChangePassword,
    fakePromptToSavePassword,
    restorePrompter,
    resetPrompterHistory,
  } = stubPrompter();
  let rootBrowser = LMP.getRootBrowser();
  let fakePopupNotifications = {
    getNotification: sinon.stub().returns({ dismissed: true }),
  };
  sinon.stub(LoginHelper, "getBrowserForPrompt").callsFake(() => {
    return {
      ownerGlobal: {
        PopupNotifications: fakePopupNotifications,
      },
    };
  });

  let login0Props = Object.assign({}, loginTemplate, {
    username: "someusername",
    password: "qweqweq",
  });
  info("Adding initial login: " + JSON.stringify(login0Props));
  let savedLogin = await LoginTestUtils.addLogin(login0Props);

  info(
    "Saved initial login: " + JSON.stringify(Services.logins.getAllLogins()[0])
  );

  equal(
    Services.logins.getAllLogins().length,
    1,
    "Should have 1 saved login at the start of the test"
  );

  // first prompt to save a new login
  let newUsername = "differentuser";
  let newPassword = login0Props.password + "🔥";
  await LMP._onPasswordEditedOrGenerated(rootBrowser, {
    browsingContextId: 99,
    origin: "https://www.example.com",
    formActionOrigin: "https://www.mozilla.org",
    autoFilledLoginGuid: savedLogin.guid,
    newPasswordField: { value: newPassword },
    usernameField: { value: newUsername },
    triggeredByFillingGenerated: false,
  });

  let expected = new LoginInfo(
    login0Props.origin,
    login0Props.formActionOrigin,
    null,
    newUsername,
    newPassword
  );

  ok(LMP._getPrompter.calledOnce, "Checking _getPrompter was called");
  info("Checking the getNotification stub");
  ok(
    !fakePopupNotifications.getNotification.called,
    "getNotification was not called"
  );
  ok(
    fakePromptToSavePassword.calledOnce,
    "Checking promptToSavePassword was called"
  );
  ok(
    fakePromptToSavePassword.getCall(0).args[2],
    "promptToSavePassword had a truthy 'dismissed' argument"
  );
  ok(
    !fakePromptToSavePassword.getCall(0).args[3],
    "promptToSavePassword had a falsey 'notifySaved' argument"
  );
  assertLoginProperties(fakePromptToSavePassword.getCall(0).args[1], expected);
  resetPrompterHistory();

  // then prompt with matching username/password
  await LMP._onPasswordEditedOrGenerated(rootBrowser, {
    browsingContextId: 99,
    origin: "https://www.example.com",
    formActionOrigin: "https://www.mozilla.org",
    autoFilledLoginGuid: savedLogin.guid,
    newPasswordField: { value: login0Props.password },
    usernameField: { value: login0Props.username },
    triggeredByFillingGenerated: false,
  });

  expected = new LoginInfo(
    login0Props.origin,
    login0Props.formActionOrigin,
    null,
    login0Props.username,
    login0Props.password
  );

  ok(LMP._getPrompter.calledOnce, "Checking _getPrompter was called");
  info("Checking the getNotification stub");
  ok(
    fakePopupNotifications.getNotification.called,
    "getNotification was called"
  );
  ok(
    fakePromptToChangePassword.calledOnce,
    "Checking promptToChangePassword was called"
  );
  ok(
    fakePromptToChangePassword.getCall(0).args[3],
    "promptToChangePassword had a truthy 'dismissed' argument"
  );
  ok(
    !fakePromptToChangePassword.getCall(0).args[4],
    "promptToChangePassword had a falsey 'notifySaved' argument"
  );
  assertLoginProperties(
    fakePromptToChangePassword.getCall(0).args[2],
    expected
  );
  resetPrompterHistory();

  LoginManagerParent._browsingContextGlobal.get.restore();
  LoginHelper.getBrowserForPrompt.restore();
  restorePrompter();
  LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().clear();
  Services.logins.removeAllLogins();
  Services.telemetry.clearEvents();
});

add_task(
  async function test_onPasswordEditedOrGenerated_generatedPassword_withDisabledLogin() {
    startTestConditions(99);
    let { generatedPassword } = stubGeneratedPasswordForBrowsingContextId(99);
    let { restorePrompter } = stubPrompter();
    let rootBrowser = LMP.getRootBrowser();

    info("Disable login saving for the site");
    Services.logins.setLoginSavingEnabled("https://www.example.com", false);
    await LMP._onPasswordEditedOrGenerated(rootBrowser, {
      browsingContextId: 99,
      origin: "https://www.example.com",
      formActionOrigin: "https://www.mozilla.org",
      newPasswordField: { value: generatedPassword },
      triggeredByFillingGenerated: true,
    });
    equal(
      Services.logins.getAllLogins().length,
      0,
      "Should have no saved logins since saving is disabled"
    );
    ok(LMP._getPrompter.notCalled, "Checking _getPrompter wasn't called");

    // Clean up
    LoginManagerParent._browsingContextGlobal.get.restore();
    restorePrompter();
    LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().clear();
    Services.logins.setLoginSavingEnabled("https://www.example.com", true);
    Services.logins.removeAllLogins();
  }
);

add_task(
  async function test_onPasswordEditedOrGenerated_generatedPassword_withSavedEmptyUsername() {
    startTestConditions(99);
    let login0Props = Object.assign({}, loginTemplate, {
      username: "",
      password: "qweqweq",
    });
    info("Adding initial login: " + JSON.stringify(login0Props));
    let expected = await LoginTestUtils.addLogin(login0Props);

    info(
      "Saved initial login: " +
        JSON.stringify(Services.logins.getAllLogins()[0])
    );

    let {
      generatedPassword: password1,
    } = stubGeneratedPasswordForBrowsingContextId(99);
    let { restorePrompter, fakePromptToChangePassword } = stubPrompter();
    let rootBrowser = LMP.getRootBrowser();

    await LMP._onPasswordEditedOrGenerated(rootBrowser, {
      browsingContextId: 99,
      origin: "https://www.example.com",
      formActionOrigin: "https://www.mozilla.org",
      newPasswordField: { value: password1 },
      triggeredByFillingGenerated: true,
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
      fakePromptToChangePassword.getCall(0).args[3],
      "promptToChangePassword had a truthy 'dismissed' argument"
    );
    ok(
      !fakePromptToChangePassword.getCall(0).args[4],
      "promptToChangePassword had a falsey 'notifySaved' argument"
    );

    info("Edit the password");
    const newPassword = password1 + "🔥";
    await LMP._onPasswordEditedOrGenerated(rootBrowser, {
      browsingContextId: 99,
      origin: "https://www.example.com",
      formActionOrigin: "https://www.mozilla.org",
      newPasswordField: { value: newPassword },
      usernameField: { value: "someusername" },
      triggeredByFillingGenerated: true,
    });
    let generatedPW = LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().get(
      "https://www.example.com^userContextId=6"
    );
    ok(generatedPW.edited, "Cached edited boolean should be true");
    equal(generatedPW.storageGUID, null, "Should have no storageGUID");
    equal(generatedPW.value, newPassword, "Cached password should be updated");
    assertLoginProperties(Services.logins.getAllLogins()[0], login0Props);
    ok(Services.logins.getAllLogins()[0].equals(expected), "Ensure no changes");
    equal(
      Services.logins.getAllLogins().length,
      1,
      "Should have 1 saved login still"
    );

    checkEditTelemetryRecorded(1, "Updating cache, not storage (no auto-save)");

    LoginManagerParent._browsingContextGlobal.get.restore();
    restorePrompter();
    LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().clear();
    Services.logins.removeAllLogins();
    Services.telemetry.clearEvents();
  }
);

add_task(
  async function test_onPasswordEditedOrGenerated_generatedPassword_withSavedEmptyUsernameAndUsernameValue() {
    // Save as the above task but with a non-empty username field value.
    startTestConditions(99);
    let login0Props = Object.assign({}, loginTemplate, {
      username: "",
      password: "qweqweq",
    });
    info("Adding initial login: " + JSON.stringify(login0Props));
    let expected = await LoginTestUtils.addLogin(login0Props);

    info(
      "Saved initial login: " +
        JSON.stringify(Services.logins.getAllLogins()[0])
    );

    let {
      generatedPassword: password1,
    } = stubGeneratedPasswordForBrowsingContextId(99);
    let {
      restorePrompter,
      fakePromptToChangePassword,
      fakePromptToSavePassword,
    } = stubPrompter();
    let rootBrowser = LMP.getRootBrowser();

    await LMP._onPasswordEditedOrGenerated(rootBrowser, {
      browsingContextId: 99,
      origin: "https://www.example.com",
      formActionOrigin: "https://www.mozilla.org",
      newPasswordField: { value: password1 },
      usernameField: { value: "non-empty-username" },
      triggeredByFillingGenerated: true,
    });
    equal(
      Services.logins.getAllLogins().length,
      1,
      "Should just have the previously-saved login with empty username"
    );
    assertLoginProperties(Services.logins.getAllLogins()[0], login0Props);

    ok(LMP._getPrompter.calledOnce, "Checking _getPrompter was called");
    ok(
      fakePromptToChangePassword.notCalled,
      "Checking promptToChangePassword wasn't called"
    );
    ok(
      fakePromptToSavePassword.calledOnce,
      "Checking promptToSavePassword was called"
    );
    ok(
      fakePromptToSavePassword.getCall(0).args[2],
      "promptToSavePassword had a truthy 'dismissed' argument"
    );
    ok(
      !fakePromptToSavePassword.getCall(0).args[3],
      "promptToSavePassword had a falsey 'notifySaved' argument"
    );

    info("Edit the password");
    const newPassword = password1 + "🔥";
    await LMP._onPasswordEditedOrGenerated(rootBrowser, {
      browsingContextId: 99,
      origin: "https://www.example.com",
      formActionOrigin: "https://www.mozilla.org",
      newPasswordField: { value: newPassword },
      usernameField: { value: "non-empty-username" },
      triggeredByFillingGenerated: true,
    });
    ok(
      fakePromptToChangePassword.notCalled,
      "Checking promptToChangePassword wasn't called"
    );
    ok(
      fakePromptToSavePassword.calledTwice,
      "Checking promptToSavePassword was called again"
    );
    ok(
      fakePromptToSavePassword.getCall(1).args[2],
      "promptToSavePassword had a truthy 'dismissed' argument"
    );
    ok(
      !fakePromptToSavePassword.getCall(1).args[3],
      "promptToSavePassword had a falsey 'notifySaved' argument"
    );

    let generatedPW = LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().get(
      "https://www.example.com^userContextId=6"
    );
    ok(generatedPW.edited, "Cached edited boolean should be true");
    equal(generatedPW.storageGUID, null, "Should have no storageGUID");
    equal(generatedPW.value, newPassword, "Cached password should be updated");
    assertLoginProperties(Services.logins.getAllLogins()[0], login0Props);
    ok(Services.logins.getAllLogins()[0].equals(expected), "Ensure no changes");
    equal(
      Services.logins.getAllLogins().length,
      1,
      "Should have 1 saved login still"
    );

    checkEditTelemetryRecorded(
      1,
      "Updating cache, not storage (no auto-save) with username in field"
    );

    LoginManagerParent._browsingContextGlobal.get.restore();
    restorePrompter();
    LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().clear();
    Services.logins.removeAllLogins();
    Services.telemetry.clearEvents();
  }
);

add_task(
  async function test_onPasswordEditedOrGenerated_generatedPassword_withEmptyUsernameDifferentFormActionOrigin() {
    startTestConditions(99);
    let login0Props = Object.assign({}, loginTemplate, {
      username: "",
      password: "qweqweq",
    });
    await LoginTestUtils.addLogin(login0Props);

    let {
      generatedPassword: password1,
    } = stubGeneratedPasswordForBrowsingContextId(99);
    let { restorePrompter, fakePromptToChangePassword } = stubPrompter();
    let rootBrowser = LMP.getRootBrowser();

    await LMP._onPasswordEditedOrGenerated(rootBrowser, {
      browsingContextId: 99,
      origin: "https://www.example.com",
      formActionOrigin: "https://www.elsewhere.com",
      newPasswordField: { value: password1 },
      triggeredByFillingGenerated: true,
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
      fakePromptToChangePassword.getCall(0).args[2],
      "promptToChangePassword had a truthy 'dismissed' argument"
    );
    ok(
      fakePromptToChangePassword.getCall(0).args[3],
      "promptToChangePassword had a truthy 'notifySaved' argument"
    );

    LoginManagerParent._browsingContextGlobal.get.restore();
    restorePrompter();
    LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().clear();
    Services.logins.removeAllLogins();
  }
);

add_task(
  async function test_onPasswordEditedOrGenerated_generatedPassword_withSavedUsername() {
    startTestConditions(99);
    let login0Props = Object.assign({}, loginTemplate, {
      username: "previoususer",
      password: "qweqweq",
    });
    await LoginTestUtils.addLogin(login0Props);

    let {
      generatedPassword: password1,
    } = stubGeneratedPasswordForBrowsingContextId(99);
    let { restorePrompter, fakePromptToChangePassword } = stubPrompter();
    let rootBrowser = LMP.getRootBrowser();

    await LMP._onPasswordEditedOrGenerated(rootBrowser, {
      browsingContextId: 99,
      origin: "https://www.example.com",
      formActionOrigin: "https://www.mozilla.org",
      newPasswordField: { value: password1 },
      triggeredByFillingGenerated: true,
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
      fakePromptToChangePassword.getCall(0).args[2],
      "promptToChangePassword had a truthy 'dismissed' argument"
    );
    ok(
      fakePromptToChangePassword.getCall(0).args[3],
      "promptToChangePassword had a truthy 'notifySaved' argument"
    );

    LoginManagerParent._browsingContextGlobal.get.restore();
    restorePrompter();
    LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().clear();
    Services.logins.removeAllLogins();
  }
);
