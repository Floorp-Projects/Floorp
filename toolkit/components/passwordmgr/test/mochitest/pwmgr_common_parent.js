/**
 * Loaded as a frame script to do privileged things in mochitest-plain tests.
 * See pwmgr_common.js for the content process companion.
 */

/* eslint-env mozilla/chrome-script */

"use strict";

var { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
var { LoginHelper } = ChromeUtils.importESModule(
  "resource://gre/modules/LoginHelper.sys.mjs"
);
var { LoginManagerParent } = ChromeUtils.importESModule(
  "resource://gre/modules/LoginManagerParent.sys.mjs"
);
const { LoginTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/LoginTestUtils.sys.mjs"
);
if (LoginHelper.relatedRealmsEnabled) {
  let rsPromise =
    LoginTestUtils.remoteSettings.setupWebsitesWithSharedCredentials();
  async () => {
    await rsPromise;
  };
}
if (LoginHelper.improvedPasswordRulesEnabled) {
  let rsPromise = LoginTestUtils.remoteSettings.setupImprovedPasswordRules({
    rules: "",
  });
  async () => {
    await rsPromise;
  };
}

/**
 * Init with a common login.
 */
async function commonInit(testDependsOnDeprecatedLogin) {
  var pwmgr = Services.logins;
  assert.ok(pwmgr != null, "Access LoginManager");

  // Check that initial state has no logins
  var logins = await pwmgr.getAllLogins();
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
    await pwmgr.addLoginAsync(login);
  }

  // Last sanity check
  logins = await pwmgr.getAllLogins();
  assert.equal(
    logins.length,
    testDependsOnDeprecatedLogin ? 1 : 0,
    "Checking for successful init login"
  );
  disabledHosts = pwmgr.getAllDisabledHosts();
  assert.equal(disabledHosts.length, 0, "Checking for no disabled hosts");
}

async function dumpLogins() {
  let logins = await Services.logins.getAllLogins();
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

addMessageListener("storageChanged", async function ({ expectedChangeTypes }) {
  return new Promise((resolve, reject) => {
    function storageChanged(subject, topic, data) {
      let changeType = expectedChangeTypes.shift();
      if (data != changeType) {
        resolve("Unexpected change type " + data + ", expected " + changeType);
      } else if (expectedChangeTypes.length === 0) {
        Services.obs.removeObserver(
          storageChanged,
          "passwordmgr-storage-changed"
        );
        resolve();
      }
    }

    Services.obs.addObserver(storageChanged, "passwordmgr-storage-changed");
  });
});

addMessageListener("promptShown", async function () {
  return new Promise(resolve => {
    function promptShown(subject, topic, data) {
      Services.obs.removeObserver(promptShown, "passwordmgr-prompt-change");
      Services.obs.removeObserver(promptShown, "passwordmgr-prompt-save");
      resolve(topic);
    }

    Services.obs.addObserver(promptShown, "passwordmgr-prompt-change");
    Services.obs.addObserver(promptShown, "passwordmgr-prompt-save");
  });
});

addMessageListener("cleanup", () => {
  Services.logins.removeAllUserFacingLogins();
});

// Begin message listeners

addMessageListener(
  "setupParent",
  async ({ testDependsOnDeprecatedLogin = false } = {}) => {
    return commonInit(testDependsOnDeprecatedLogin);
  }
);

addMessageListener("loadRecipes", async function (recipes) {
  var recipeParent = await LoginManagerParent.recipeParentPromise;
  await recipeParent.load(recipes);
  return recipes;
});

addMessageListener("resetRecipes", async function () {
  let recipeParent = await LoginManagerParent.recipeParentPromise;
  await recipeParent.reset();
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

  return events;
});

addMessageListener("proxyLoginManager", async msg => {
  // Recreate nsILoginInfo objects from vanilla JS objects.
  let recreatedArgs = msg.args.map((arg, index) => {
    if (msg.loginInfoIndices.includes(index)) {
      return LoginHelper.vanillaObjectToLogin(arg);
    }

    return arg;
  });

  let rv = await Services.logins[msg.methodName](...recreatedArgs);
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

addMessageListener("setPrimaryPassword", ({ enable }) => {
  if (enable) {
    LoginTestUtils.primaryPassword.enable();
  } else {
    LoginTestUtils.primaryPassword.disable();
  }
});

LoginManagerParent.setListenerForTests((msg, { origin, data }) => {
  if (msg == "ShowDoorhanger") {
    sendAsyncMessage("formSubmissionProcessed", { origin, data });
  } else if (msg == "PasswordEditedOrGenerated") {
    sendAsyncMessage("passwordEditedOrGenerated", { origin, data });
  } else if (msg == "FormProcessed") {
    sendAsyncMessage("formProcessed", data);
  }
});

addMessageListener("cleanup", () => {
  LoginManagerParent.setListenerForTests(null);
});
