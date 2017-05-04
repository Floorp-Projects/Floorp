const TESTS_DIR = "/tests/toolkit/components/passwordmgr/test/";

/**
 * Returns the element with the specified |name| attribute.
 */
function $_(formNum, name) {
  var form = document.getElementById("form" + formNum);
  if (!form) {
    logWarning("$_ couldn't find requested form " + formNum);
    return null;
  }

  var element = form.children.namedItem(name);
  if (!element) {
    logWarning("$_ couldn't find requested element " + name);
    return null;
  }

  // Note that namedItem is a bit stupid, and will prefer an
  // |id| attribute over a |name| attribute when looking for
  // the element. Login Mananger happens to use .namedItem
  // anyway, but let's rigorously check it here anyway so
  // that we don't end up with tests that mistakenly pass.

  if (element.getAttribute("name") != name) {
    logWarning("$_ got confused.");
    return null;
  }

  return element;
}

/**
 * Check a form for expected values. If an argument is null, a field's
 * expected value will be the default value.
 *
 * <form id="form#">
 * checkForm(#, "foo");
 */
function checkForm(formNum, val1, val2, val3) {
  var e, form = document.getElementById("form" + formNum);
  ok(form, "Locating form " + formNum);

  var numToCheck = arguments.length - 1;

  if (!numToCheck--)
    return;
  e = form.elements[0];
  if (val1 == null)
    is(e.value, e.defaultValue, "Test default value of field " + e.name +
       " in form " + formNum);
  else
    is(e.value, val1, "Test value of field " + e.name +
       " in form " + formNum);


  if (!numToCheck--)
    return;
  e = form.elements[1];
  if (val2 == null)
    is(e.value, e.defaultValue, "Test default value of field " + e.name +
       " in form " + formNum);
  else
    is(e.value, val2, "Test value of field " + e.name +
       " in form " + formNum);


  if (!numToCheck--)
    return;
  e = form.elements[2];
  if (val3 == null)
    is(e.value, e.defaultValue, "Test default value of field " + e.name +
       " in form " + formNum);
  else
    is(e.value, val3, "Test value of field " + e.name +
       " in form " + formNum);
}

/**
 * Check a form for unmodified values from when page was loaded.
 *
 * <form id="form#">
 * checkUnmodifiedForm(#);
 */
function checkUnmodifiedForm(formNum) {
  var form = document.getElementById("form" + formNum);
  ok(form, "Locating form " + formNum);

  for (var i = 0; i < form.elements.length; i++) {
    var ele = form.elements[i];

    // No point in checking form submit/reset buttons.
    if (ele.type == "submit" || ele.type == "reset")
      continue;

    is(ele.value, ele.defaultValue, "Test to default value of field " +
       ele.name + " in form " + formNum);
  }
}

/**
 * Mochitest gives us a sendKey(), but it's targeted to a specific element.
 * This basically sends an untargeted key event, to whatever's focused.
 */
function doKey(aKey, modifier) {
  var keyName = "DOM_VK_" + aKey.toUpperCase();
  var key = KeyEvent[keyName];

  // undefined --> null
  if (!modifier)
    modifier = null;

  // Window utils for sending fake sey events.
  var wutils = SpecialPowers.wrap(window).
               QueryInterface(SpecialPowers.Ci.nsIInterfaceRequestor).
               getInterface(SpecialPowers.Ci.nsIDOMWindowUtils);

  if (wutils.sendKeyEvent("keydown", key, 0, modifier)) {
    wutils.sendKeyEvent("keypress", key, 0, modifier);
  }
  wutils.sendKeyEvent("keyup", key, 0, modifier);
}

/**
 * Init with a common login
 * If selfFilling is true or non-undefined, fires an event at the page so that
 * the test can start checking filled-in values. Tests that check observer
 * notifications might be confused by this.
 */
function commonInit(selfFilling) {
  var pwmgr = SpecialPowers.Cc["@mozilla.org/login-manager;1"].
              getService(SpecialPowers.Ci.nsILoginManager);
  ok(pwmgr != null, "Access LoginManager");

  // Check that initial state has no logins
  var logins = pwmgr.getAllLogins();
  is(logins.length, 0, "Not expecting logins to be present");
  var disabledHosts = pwmgr.getAllDisabledHosts();
  if (disabledHosts.length) {
    ok(false, "Warning: wasn't expecting disabled hosts to be present.");
    for (var host of disabledHosts)
      pwmgr.setLoginSavingEnabled(host, true);
  }

  // Add a login that's used in multiple tests
  var login = SpecialPowers.Cc["@mozilla.org/login-manager/loginInfo;1"].
              createInstance(SpecialPowers.Ci.nsILoginInfo);
  login.init("http://mochi.test:8888", "http://mochi.test:8888", null,
             "testuser", "testpass", "uname", "pword");
  pwmgr.addLogin(login);

  // Last sanity check
  logins = pwmgr.getAllLogins();
  is(logins.length, 1, "Checking for successful init login");
  disabledHosts = pwmgr.getAllDisabledHosts();
  is(disabledHosts.length, 0, "Checking for no disabled hosts");

  if (selfFilling)
    return;

  if (this.sendAsyncMessage) {
    sendAsyncMessage("registerRunTests");
  } else {
    registerRunTests();
  }
}

function registerRunTests() {
  return new Promise(resolve => {
    // We provide a general mechanism for our tests to know when they can
    // safely run: we add a final form that we know will be filled in, wait
    // for the login manager to tell us that it's filled in and then continue
    // with the rest of the tests.
    window.addEventListener("DOMContentLoaded", (event) => {
      var form = document.createElement("form");
      form.id = "observerforcer";
      var username = document.createElement("input");
      username.name = "testuser";
      form.appendChild(username);
      var password = document.createElement("input");
      password.name = "testpass";
      password.type = "password";
      form.appendChild(password);

      var observer = SpecialPowers.wrapCallback(function(subject, topic, data) {
        var formLikeRoot = subject.QueryInterface(SpecialPowers.Ci.nsIDOMNode);
        if (formLikeRoot.id !== "observerforcer")
          return;
        SpecialPowers.removeObserver(observer, "passwordmgr-processed-form");
        formLikeRoot.remove();
        SimpleTest.executeSoon(() => {
          var runTestEvent = new Event("runTests");
          window.dispatchEvent(runTestEvent);
          resolve();
        });
      });
      SpecialPowers.addObserver(observer, "passwordmgr-processed-form");

      document.body.appendChild(form);
    });
  });
}

const masterPassword = "omgsecret!";

function enableMasterPassword() {
  setMasterPassword(true);
}

function disableMasterPassword() {
  setMasterPassword(false);
}

function setMasterPassword(enable) {
  var oldPW, newPW;
  if (enable) {
    oldPW = "";
    newPW = masterPassword;
  } else {
    oldPW = masterPassword;
    newPW = "";
  }
  // Set master password. Note that this does not log you in, so the next
  // invocation of pwmgr can trigger a MP prompt.

  var pk11db = Cc["@mozilla.org/security/pk11tokendb;1"].getService(Ci.nsIPK11TokenDB);
  var token = pk11db.getInternalKeyToken();
  info("MP change from " + oldPW + " to " + newPW);
  token.changePassword(oldPW, newPW);
}

function logoutMasterPassword() {
  var sdr = Cc["@mozilla.org/security/sdr;1"].getService(Ci.nsISecretDecoderRing);
  sdr.logoutAndTeardown();
}

function dumpLogins(pwmgr) {
  var logins = pwmgr.getAllLogins();
  ok(true, "----- dumpLogins: have " + logins.length + " logins. -----");
  for (var i = 0; i < logins.length; i++)
    dumpLogin("login #" + i + " --- ", logins[i]);
}

function dumpLogin(label, login) {
  var loginText = "";
  loginText += "host: ";
  loginText += login.hostname;
  loginText += " / formURL: ";
  loginText += login.formSubmitURL;
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
  ok(true, label + loginText);
}

function getRecipeParent() {
  var { LoginManagerParent } = SpecialPowers.Cu.import("resource://gre/modules/LoginManagerParent.jsm", {});
  if (!LoginManagerParent.recipeParentPromise) {
    return null;
  }
  return LoginManagerParent.recipeParentPromise.then((recipeParent) => {
    return SpecialPowers.wrap(recipeParent);
  });
}

/**
 * Resolves when a specified number of forms have been processed.
 */
function promiseFormsProcessed(expectedCount = 1) {
  var processedCount = 0;
  return new Promise((resolve, reject) => {
    function onProcessedForm(subject, topic, data) {
      processedCount++;
      if (processedCount == expectedCount) {
        SpecialPowers.removeObserver(onProcessedForm, "passwordmgr-processed-form");
        resolve(SpecialPowers.Cu.waiveXrays(subject), data);
      }
    }
    SpecialPowers.addObserver(onProcessedForm, "passwordmgr-processed-form");
  });
}

function loadRecipes(recipes) {
  info("Loading recipes");
  return new Promise(resolve => {
    chromeScript.addMessageListener("loadedRecipes", function loaded() {
      chromeScript.removeMessageListener("loadedRecipes", loaded);
      resolve(recipes);
    });
    chromeScript.sendAsyncMessage("loadRecipes", recipes);
  });
}

function resetRecipes() {
  info("Resetting recipes");
  return new Promise(resolve => {
    chromeScript.addMessageListener("recipesReset", function reset() {
      chromeScript.removeMessageListener("recipesReset", reset);
      resolve();
    });
    chromeScript.sendAsyncMessage("resetRecipes");
  });
}

function promiseStorageChanged(expectedChangeTypes) {
  return new Promise((resolve, reject) => {
    function onStorageChanged({ topic, data }) {
      let changeType = expectedChangeTypes.shift();
      is(data, changeType, "Check expected passwordmgr-storage-changed type");
      if (expectedChangeTypes.length === 0) {
        chromeScript.removeMessageListener("storageChanged", onStorageChanged);
        resolve();
      }
    }
    chromeScript.addMessageListener("storageChanged", onStorageChanged);
  });
}

function promisePromptShown(expectedTopic) {
  return new Promise((resolve, reject) => {
    function onPromptShown({ topic, data }) {
      is(topic, expectedTopic, "Check expected prompt topic");
      chromeScript.removeMessageListener("promptShown", onPromptShown);
      resolve();
    }
    chromeScript.addMessageListener("promptShown", onPromptShown);
  });
}

/**
 * Run a function synchronously in the parent process and destroy it in the test cleanup function.
 * @param {Function|String} aFunctionOrURL - either a function that will be stringified and run
 *                                           or the URL to a JS file.
 * @return {Object} - the return value of loadChromeScript providing message-related methods.
 *                    @see loadChromeScript in specialpowersAPI.js
 */
function runInParent(aFunctionOrURL) {
  let chromeScript = SpecialPowers.loadChromeScript(aFunctionOrURL);
  SimpleTest.registerCleanupFunction(() => {
    chromeScript.destroy();
  });
  return chromeScript;
}

/**
 * Run commonInit synchronously in the parent then run the test function after the runTests event.
 *
 * @param {Function} aFunction The test function to run
 */
function runChecksAfterCommonInit(aFunction = null) {
  SimpleTest.waitForExplicitFinish();
  let pwmgrCommonScript = runInParent(SimpleTest.getTestFileURL("pwmgr_common.js"));
  if (aFunction) {
    window.addEventListener("runTests", aFunction);
    pwmgrCommonScript.addMessageListener("registerRunTests", () => registerRunTests());
  }
  pwmgrCommonScript.sendSyncMessage("setupParent");
  return pwmgrCommonScript;
}

// Code to run when loaded as a chrome script in tests via loadChromeScript
if (this.addMessageListener) {
  const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;
  var SpecialPowers = { Cc, Ci, Cr, Cu, };
  var ok, is;
  // Ignore ok/is in commonInit since they aren't defined in a chrome script.
  ok = is = () => {}; // eslint-disable-line no-native-reassign

  Cu.import("resource://gre/modules/AppConstants.jsm");
  Cu.import("resource://gre/modules/LoginHelper.jsm");
  Cu.import("resource://gre/modules/LoginManagerParent.jsm");
  Cu.import("resource://gre/modules/Services.jsm");
  Cu.import("resource://gre/modules/Task.jsm");

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

  addMessageListener("setupParent", ({selfFilling = false} = {selfFilling: false}) => {
    // Force LoginManagerParent to init for the tests since it's normally delayed
    // by apps such as on Android.
    if (AppConstants.platform == "android") {
      LoginManagerParent.init();
    }

    commonInit(selfFilling);
    sendAsyncMessage("doneSetup");
  });

  addMessageListener("loadRecipes", Task.async(function*(recipes) {
    var recipeParent = yield LoginManagerParent.recipeParentPromise;
    yield recipeParent.load(recipes);
    sendAsyncMessage("loadedRecipes", recipes);
  }));

  addMessageListener("resetRecipes", Task.async(function*() {
    let recipeParent = yield LoginManagerParent.recipeParentPromise;
    yield recipeParent.reset();
    sendAsyncMessage("recipesReset");
  }));

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
    }
    return rv;
  });

  var globalMM = Cc["@mozilla.org/globalmessagemanager;1"].getService(Ci.nsIMessageListenerManager);
  globalMM.addMessageListener("RemoteLogins:onFormSubmit", function onFormSubmit(message) {
    sendAsyncMessage("formSubmissionProcessed", message.data, message.objects);
  });
} else {
  // Code to only run in the mochitest pages (not in the chrome script).
  SpecialPowers.pushPrefEnv({"set": [["signon.rememberSignons", true],
                                     ["signon.autofillForms.http", true],
                                     ["security.insecure_field_warning.contextual.enabled", false]]
                           });
  SimpleTest.registerCleanupFunction(() => {
    SpecialPowers.popPrefEnv();
    runInParent(function cleanupParent() {
      const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;
      Cu.import("resource://gre/modules/Services.jsm");
      Cu.import("resource://gre/modules/LoginManagerParent.jsm");

      // Remove all logins and disabled hosts
      Services.logins.removeAllLogins();

      let disabledHosts = Services.logins.getAllDisabledHosts();
      disabledHosts.forEach(host => Services.logins.setLoginSavingEnabled(host, true));

      let authMgr = Cc["@mozilla.org/network/http-auth-manager;1"].
                    getService(Ci.nsIHttpAuthManager);
      authMgr.clearAll();

      if (LoginManagerParent._recipeManager) {
        LoginManagerParent._recipeManager.reset();
      }

      // Cleanup PopupNotifications (if on a relevant platform)
      let chromeWin = Services.wm.getMostRecentWindow("navigator:browser");
      if (chromeWin && chromeWin.PopupNotifications) {
        let notes = chromeWin.PopupNotifications._currentNotifications;
        if (notes.length > 0) {
          dump("Removing " + notes.length + " popup notifications.\n");
        }
        for (let note of notes) {
	  note.remove();
        }
      }
    });
  });


  let { LoginHelper } = SpecialPowers.Cu.import("resource://gre/modules/LoginHelper.jsm", {});
  /**
   * Proxy for Services.logins (nsILoginManager).
   * Only supports arguments which support structured clone plus {nsILoginInfo}
   * Assumes properties are methods.
   */
  this.LoginManager = new Proxy({}, {
    get(target, prop, receiver) {
      return (...args) => {
        let loginInfoIndices = [];
        let cloneableArgs = args.map((val, index) => {
          if (SpecialPowers.call_Instanceof(val, SpecialPowers.Ci.nsILoginInfo)) {
            loginInfoIndices.push(index);
            return LoginHelper.loginToVanillaObject(val);
          }

          return val;
        });

        return chromeScript.sendSyncMessage("proxyLoginManager", {
          args: cloneableArgs,
          loginInfoIndices,
          methodName: prop,
        })[0][0];
      };
    },
  });
}
