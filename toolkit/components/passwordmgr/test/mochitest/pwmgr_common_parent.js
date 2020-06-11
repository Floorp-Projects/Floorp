/**
 * Loaded as a frame script to do privileged things in mochitest-plain tests.
 * See pwmgr_common.js for the content process companion.
 */

"use strict";

// assert is available to chrome scripts loaded via SpecialPowers.loadChromeScript.
/* global assert */
/* eslint-env mozilla/frame-script */

var { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
var { LoginHelper } = ChromeUtils.import(
  "resource://gre/modules/LoginHelper.jsm"
);
var { LoginManagerParent } = ChromeUtils.import(
  "resource://gre/modules/LoginManagerParent.jsm"
);
const { LoginTestUtils } = ChromeUtils.import(
  "resource://testing-common/LoginTestUtils.jsm"
);
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

/**
 * Init with a common login
 * If selfFilling is true or non-undefined, fires an event at the page so that
 * the test can start checking filled-in values. Tests that check observer
 * notifications might be confused by this.
 */
function commonInit(selfFilling, testDependsOnDeprecatedLogin) {
  var pwmgr = Services.logins;
  assert.ok(pwmgr != null, "Access LoginManager");

  // Check that initial state has no logins
  var logins = pwmgr.getAllLogins();
  assert.equal(logins.length, 0, "Not expecting logins to be present");
  var disabledHosts = pwmgr.getAllDisabledHosts();
  if (disabledHosts.length) {
    assert.ok(false, "Warning: wasn't expecting disabled hosts to be present.");
    for (var host of disabledHosts) {
      pwmgr.setLoginSavingEnabled(host, true);
    }
  }

  if (testDependsOnDeprecatedLogin) {
    // Add a login that's used in multiple tests
    var login = Cc["@mozilla.org/login-manager/loginInfo;1"].createInstance(
      Ci.nsILoginInfo
    );
    login.init(
      "http://mochi.test:8888",
      "http://mochi.test:8888",
      null,
      "testuser",
      "testpass",
      "uname",
      "pword"
    );
    pwmgr.addLogin(login);
  }

  // Last sanity check
  logins = pwmgr.getAllLogins();
  assert.equal(
    logins.length,
    testDependsOnDeprecatedLogin ? 1 : 0,
    "Checking for successful init login"
  );
  disabledHosts = pwmgr.getAllDisabledHosts();
  assert.equal(disabledHosts.length, 0, "Checking for no disabled hosts");

  if (selfFilling) {
    return;
  }

  // Notify the content side that initialization is done and tests can start.
  sendAsyncMessage("registerRunTests");
}

function dumpLogins() {
  let logins = Services.logins.getAllLogins();
  assert.ok(true, "----- dumpLogins: have " + logins.length + " logins. -----");
  for (var i = 0; i < logins.length; i++) {
    dumpLogin("login #" + i + " --- ", logins[i]);
  }
}

function dumpLogin(label, login) {
  var loginText = "";
  loginText += "origin: ";
  loginText += login.origin;
  loginText += " / formActionOrigin: ";
  loginText += login.formActionOrigin;
  loginText += " / realm: ";
  loginText += login.httpRealm;
  loginText += " / user: ";
  loginText += login.username;
  loginText += " / pass: ";
  loginText += login.password;
  loginText += " / ufield: ";
  loginText += login.usernameField;
  loginText += " / pfield: ";
  loginText += login.passwordField;
  assert.ok(true, label + loginText);
}

function onStorageChanged(subject, topic, data) {
  sendAsyncMessage("storageChanged", {
    topic,
    data,
  });
}
Services.obs.addObserver(onStorageChanged, "passwordmgr-storage-changed");

function onPrompt(subject, topic, data) {
  sendAsyncMessage("promptShown", {
    topic,
    data,
  });
}
Services.obs.addObserver(onPrompt, "passwordmgr-prompt-change");
Services.obs.addObserver(onPrompt, "passwordmgr-prompt-save");

addMessageListener("cleanup", () => {
  Services.obs.removeObserver(onStorageChanged, "passwordmgr-storage-changed");
  Services.obs.removeObserver(onPrompt, "passwordmgr-prompt-change");
  Services.obs.removeObserver(onPrompt, "passwordmgr-prompt-save");
});

// Begin message listeners

addMessageListener(
  "setupParent",
  ({ selfFilling = false, testDependsOnDeprecatedLogin = false } = {}) => {
    commonInit(selfFilling, testDependsOnDeprecatedLogin);
    sendAsyncMessage("doneSetup");
  }
);

addMessageListener("loadRecipes", async function(recipes) {
  var recipeParent = await LoginManagerParent.recipeParentPromise;
  await recipeParent.load(recipes);
  sendAsyncMessage("loadedRecipes", recipes);
});

addMessageListener("resetRecipes", async function() {
  let recipeParent = await LoginManagerParent.recipeParentPromise;
  await recipeParent.reset();
  sendAsyncMessage("recipesReset");
});

addMessageListener("getTelemetryEvents", options => {
  options = Object.assign(
    {
      filterProps: {},
      clear: false,
    },
    options
  );
  let snapshots = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    options.clear
  );
  let events = options.process in snapshots ? snapshots[options.process] : [];

  // event is array of values like: [22476,"pwmgr","autocomplete_field","generatedpassword"]
  let keys = ["id", "category", "method", "object", "value"];
  events = events.filter(entry => {
    for (let idx = 0; idx < keys.length; idx++) {
      let key = keys[idx];
      if (
        key in options.filterProps &&
        options.filterProps[key] !== entry[idx]
      ) {
        return false;
      }
    }
    return true;
  });
  sendAsyncMessage("getTelemetryEvents", events);
});

addMessageListener("proxyLoginManager", msg => {
  // Recreate nsILoginInfo objects from vanilla JS objects.
  let recreatedArgs = msg.args.map((arg, index) => {
    if (msg.loginInfoIndices.includes(index)) {
      return LoginHelper.vanillaObjectToLogin(arg);
    }

    return arg;
  });

  let rv = Services.logins[msg.methodName](...recreatedArgs);
  if (rv instanceof Ci.nsILoginInfo) {
    rv = LoginHelper.loginToVanillaObject(rv);
  } else if (
    Array.isArray(rv) &&
    !!rv.length &&
    rv[0] instanceof Ci.nsILoginInfo
  ) {
    rv = rv.map(login => LoginHelper.loginToVanillaObject(login));
  }
  return rv;
});

addMessageListener("isLoggedIn", () => {
  // This can't use the LoginManager proxy in pwmgr_common.js since it's not a method.
  return Services.logins.isLoggedIn;
});

addMessageListener("setMasterPassword", ({ enable }) => {
  if (enable) {
    LoginTestUtils.masterPassword.enable();
  } else {
    LoginTestUtils.masterPassword.disable();
  }
});

LoginManagerParent.setListenerForTests((msg, { origin, data }) => {
  if (msg == "FormSubmit") {
    sendAsyncMessage("formSubmissionProcessed", { origin, data });
  } else if (msg == "PasswordEditedOrGenerated") {
    sendAsyncMessage("passwordEditedOrGenerated", { origin, data });
  }
});

addMessageListener("cleanup", () => {
  LoginManagerParent.setListenerForTests(null);
});
