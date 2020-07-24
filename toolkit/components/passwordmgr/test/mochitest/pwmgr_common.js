/**
 * Helpers for password manager mochitest-plain tests.
 */

/* import-globals-from ../../../../../toolkit/components/satchel/test/satchel_common.js */

const { LoginTestUtils } = SpecialPowers.Cu.import(
  "resource://testing-common/LoginTestUtils.jsm",
  {}
);

// Setup LoginTestUtils to report assertions to the mochitest harness.
LoginTestUtils.setAssertReporter(
  SpecialPowers.wrapCallback((err, message, stack) => {
    SimpleTest.record(!err, err ? err.message : message, null, stack);
  })
);

const { LoginHelper } = SpecialPowers.Cu.import(
  "resource://gre/modules/LoginHelper.jsm",
  {}
);
const { Services } = SpecialPowers.Cu.import(
  "resource://gre/modules/Services.jsm",
  {}
);

const {
  LENGTH: GENERATED_PASSWORD_LENGTH,
  REGEX: GENERATED_PASSWORD_REGEX,
} = LoginTestUtils.generation;
const LOGIN_FIELD_UTILS = LoginTestUtils.loginField;
const TESTS_DIR = "/tests/toolkit/components/passwordmgr/test/";

/**
 * Returns the element with the specified |name| attribute.
 */
function $_(formNum, name) {
  var form = document.getElementById("form" + formNum);
  if (!form) {
    ok(false, "$_ couldn't find requested form " + formNum);
    return null;
  }

  var element = form.children.namedItem(name);
  if (!element) {
    ok(false, "$_ couldn't find requested element " + name);
    return null;
  }

  // Note that namedItem is a bit stupid, and will prefer an
  // |id| attribute over a |name| attribute when looking for
  // the element. Login Mananger happens to use .namedItem
  // anyway, but let's rigorously check it here anyway so
  // that we don't end up with tests that mistakenly pass.

  if (element.getAttribute("name") != name) {
    ok(false, "$_ got confused.");
    return null;
  }

  return element;
}

/**
 * Recreate a DOM tree using the outerHTML to ensure that any event listeners
 * and internal state for the elements are removed.
 */
function recreateTree(element) {
  // eslint-disable-next-line no-unsanitized/property, no-self-assign
  element.outerHTML = element.outerHTML;
}

/**
 * Check autocomplete popup results to ensure that expected
 * *labels* are being shown correctly as items in the popup.
 */
function checkAutoCompleteResults(actualValues, expectedValues, hostname, msg) {
  if (hostname === null) {
    checkArrayValues(actualValues, expectedValues, msg);
    return;
  }

  is(
    typeof hostname,
    "string",
    "checkAutoCompleteResults: hostname must be a string"
  );

  isnot(
    actualValues.length,
    0,
    "There should be items in the autocomplete popup: " +
      JSON.stringify(actualValues)
  );

  // Check the footer first.
  let footerResult = actualValues[actualValues.length - 1];
  is(footerResult, "View Saved Logins", "the footer text is shown correctly");

  if (actualValues.length == 1) {
    is(
      expectedValues.length,
      0,
      "If only the footer is present then there should be no expectedValues"
    );
    info("Only the footer is present in the popup");
    return;
  }

  // Check the rest of the autocomplete item values.
  checkArrayValues(actualValues.slice(0, -1), expectedValues, msg);
}

function getIframeBrowsingContext(window, iframeNumber = 0) {
  let bc = SpecialPowers.wrap(window).windowGlobalChild.browsingContext;
  return SpecialPowers.unwrap(bc.children[iframeNumber]);
}

/**
 * Set input values via setUserInput to emulate user input
 * and distinguish them from declarative or script-assigned values
 */
function setUserInputValues(parentNode, selectorValues) {
  for (let [selector, newValue] of Object.entries(selectorValues)) {
    info(`setUserInputValues, selector: ${selector}`);
    try {
      let field = SpecialPowers.wrap(parentNode.querySelector(selector));
      if (field.value == newValue) {
        // we don't get an input event if the new value == the old
        field.value += "#";
      }
      field.setUserInput(newValue);
    } catch (ex) {
      info(ex.message);
      info(ex.stack);
      ok(
        false,
        `setUserInputValues: Couldn't set value of field: ${ex.message}`
      );
    }
  }
}

/**
 * @param {Function} [aFilterFn = undefined] Function to filter out irrelevant submissions.
 * @return {Promise} resolving when a relevant form submission was processed.
 */
function getSubmitMessage(aFilterFn = undefined) {
  info("getSubmitMessage");
  return new Promise((resolve, reject) => {
    PWMGR_COMMON_PARENT.addMessageListener(
      "formSubmissionProcessed",
      function processed(...args) {
        if (aFilterFn && !aFilterFn(...args)) {
          // This submission isn't the one we're waiting for.
          return;
        }

        info("got formSubmissionProcessed");
        PWMGR_COMMON_PARENT.removeMessageListener(
          "formSubmissionProcessed",
          processed
        );
        resolve(args[0]);
      }
    );
  });
}

/**
 * @return {Promise} resolves when a onPasswordEditedOrGenerated message is received at the parent
 */
function getPasswordEditedMessage() {
  info("getPasswordEditedMessage");
  return new Promise((resolve, reject) => {
    PWMGR_COMMON_PARENT.addMessageListener(
      "passwordEditedOrGenerated",
      function listener(...args) {
        info("got passwordEditedOrGenerated");
        PWMGR_COMMON_PARENT.removeMessageListener(
          "passwordEditedOrGenerated",
          listener
        );
        resolve(args[0]);
      }
    );
  });
}

/**
 * Check for expected username/password in form.
 * @see `checkForm` below for a similar function.
 */
function checkLoginForm(
  usernameField,
  expectedUsername,
  passwordField,
  expectedPassword
) {
  let formID = usernameField.parentNode.id;
  is(
    usernameField.value,
    expectedUsername,
    "Checking " + formID + " username is: " + expectedUsername
  );
  is(
    passwordField.value,
    expectedPassword,
    "Checking " + formID + " password is: " + expectedPassword
  );
}

function checkLoginFormInChildFrame(
  iframeBC,
  usernameFieldId,
  expectedUsername,
  passwordFieldId,
  expectedPassword
) {
  return SpecialPowers.spawn(
    iframeBC,
    [usernameFieldId, expectedUsername, passwordFieldId, expectedPassword],
    (
      usernameFieldIdF,
      expectedUsernameF,
      passwordFieldIdF,
      expectedPasswordF
    ) => {
      let usernameField = this.content.document.getElementById(
        usernameFieldIdF
      );
      let passwordField = this.content.document.getElementById(
        passwordFieldIdF
      );

      let formID = usernameField.parentNode.id;
      Assert.equal(
        usernameField.value,
        expectedUsernameF,
        "Checking " + formID + " username is: " + expectedUsernameF
      );
      Assert.equal(
        passwordField.value,
        expectedPasswordF,
        "Checking " + formID + " password is: " + expectedPasswordF
      );
    }
  );
}

/**
 * Check a form for expected values. If an argument is null, a field's
 * expected value will be the default value.
 *
 * <form id="form#">
 * checkForm(#, "foo");
 */
function checkForm(formNum, val1, val2, val3) {
  var e,
    form = document.getElementById("form" + formNum);
  ok(form, "Locating form " + formNum);

  var numToCheck = arguments.length - 1;

  if (!numToCheck--) {
    return;
  }
  e = form.elements[0];
  if (val1 == null) {
    is(
      e.value,
      e.defaultValue,
      "Test default value of field " + e.name + " in form " + formNum
    );
  } else {
    is(e.value, val1, "Test value of field " + e.name + " in form " + formNum);
  }

  if (!numToCheck--) {
    return;
  }
  e = form.elements[1];
  if (val2 == null) {
    is(
      e.value,
      e.defaultValue,
      "Test default value of field " + e.name + " in form " + formNum
    );
  } else {
    is(e.value, val2, "Test value of field " + e.name + " in form " + formNum);
  }

  if (!numToCheck--) {
    return;
  }
  e = form.elements[2];
  if (val3 == null) {
    is(
      e.value,
      e.defaultValue,
      "Test default value of field " + e.name + " in form " + formNum
    );
  } else {
    is(e.value, val3, "Test value of field " + e.name + " in form " + formNum);
  }
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
    if (ele.type == "submit" || ele.type == "reset") {
      continue;
    }

    is(
      ele.value,
      ele.defaultValue,
      "Test to default value of field " + ele.name + " in form " + formNum
    );
  }
}

function registerRunTests() {
  return new Promise(resolve => {
    function onDOMContentLoaded() {
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
        if (data !== "observerforcer") {
          return;
        }

        SpecialPowers.removeObserver(observer, "passwordmgr-processed-form");
        form.remove();
        SimpleTest.executeSoon(() => {
          var runTestEvent = new Event("runTests");
          window.dispatchEvent(runTestEvent);
          resolve();
        });
      });
      SpecialPowers.addObserver(observer, "passwordmgr-processed-form");

      document.body.appendChild(form);
    }
    // We provide a general mechanism for our tests to know when they can
    // safely run: we add a final form that we know will be filled in, wait
    // for the login manager to tell us that it's filled in and then continue
    // with the rest of the tests.
    if (
      document.readyState == "complete" ||
      document.readyState == "interactive"
    ) {
      onDOMContentLoaded();
    } else {
      window.addEventListener("DOMContentLoaded", onDOMContentLoaded);
    }
  });
}

function enableMasterPassword() {
  setMasterPassword(true);
}

function disableMasterPassword() {
  setMasterPassword(false);
}

function setMasterPassword(enable) {
  PWMGR_COMMON_PARENT.sendAsyncMessage("setMasterPassword", { enable });
}

function isLoggedIn() {
  return PWMGR_COMMON_PARENT.sendQuery("isLoggedIn");
}

function logoutMasterPassword() {
  runInParent(function parent_logoutMasterPassword() {
    var sdr = Cc["@mozilla.org/security/sdr;1"].getService(
      Ci.nsISecretDecoderRing
    );
    sdr.logoutAndTeardown();
  });
}

/**
 * Resolves when a specified number of forms have been processed for (potential) filling.
 */
function promiseFormsProcessed(expectedCount = 1) {
  var processedCount = 0;
  return new Promise((resolve, reject) => {
    function onProcessedForm(subject, topic, data) {
      processedCount++;
      if (processedCount == expectedCount) {
        info(`${processedCount} form(s) processed`);
        SpecialPowers.removeObserver(
          onProcessedForm,
          "passwordmgr-processed-form"
        );
        resolve(SpecialPowers.Cu.waiveXrays(subject), data);
      }
    }
    SpecialPowers.addObserver(onProcessedForm, "passwordmgr-processed-form");
  });
}

function getTelemetryEvents(options) {
  return new Promise(resolve => {
    PWMGR_COMMON_PARENT.addMessageListener(
      "getTelemetryEvents",
      function gotResult(events) {
        info(
          "CONTENT: getTelemetryEvents gotResult: " + JSON.stringify(events)
        );
        PWMGR_COMMON_PARENT.removeMessageListener(
          "getTelemetryEvents",
          gotResult
        );
        resolve(events);
      }
    );
    PWMGR_COMMON_PARENT.sendAsyncMessage("getTelemetryEvents", options);
  });
}

function loadRecipes(recipes) {
  info("Loading recipes");
  return new Promise(resolve => {
    PWMGR_COMMON_PARENT.addMessageListener("loadedRecipes", function loaded() {
      PWMGR_COMMON_PARENT.removeMessageListener("loadedRecipes", loaded);
      resolve(recipes);
    });
    PWMGR_COMMON_PARENT.sendAsyncMessage("loadRecipes", recipes);
  });
}

function resetRecipes() {
  info("Resetting recipes");
  return new Promise(resolve => {
    PWMGR_COMMON_PARENT.addMessageListener("recipesReset", function reset() {
      PWMGR_COMMON_PARENT.removeMessageListener("recipesReset", reset);
      resolve();
    });
    PWMGR_COMMON_PARENT.sendAsyncMessage("resetRecipes");
  });
}

function promiseStorageChanged(expectedChangeTypes) {
  return new Promise((resolve, reject) => {
    function onStorageChanged({ topic, data }) {
      let changeType = expectedChangeTypes.shift();
      is(data, changeType, "Check expected passwordmgr-storage-changed type");
      if (expectedChangeTypes.length === 0) {
        PWMGR_COMMON_PARENT.removeMessageListener(
          "storageChanged",
          onStorageChanged
        );
        resolve();
      }
    }
    PWMGR_COMMON_PARENT.addMessageListener("storageChanged", onStorageChanged);
  });
}

function promisePromptShown(expectedTopic) {
  return new Promise((resolve, reject) => {
    function onPromptShown({ topic, data }) {
      is(topic, expectedTopic, "Check expected prompt topic");
      PWMGR_COMMON_PARENT.removeMessageListener("promptShown", onPromptShown);
      resolve();
    }
    PWMGR_COMMON_PARENT.addMessageListener("promptShown", onPromptShown);
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

/*
 * gTestDependsOnDeprecatedLogin Set this global to true if your test relies
 * on the testuser/testpass login that is created in pwmgr_common.js. New tests
 * should not rely on this login.
 */
var gTestDependsOnDeprecatedLogin = false;

/**
 * Replace the content innerHTML with the provided form and wait for autofill to fill in the form.
 *
 * @param {string} form The form to be appended to the #content element.
 * @param {string} fieldSelector The CSS selector for the field to-be-filled
 * @param {string} fieldValue The value expected to be filled
 * @param {string} formId The ID (excluding the # character) of the form
 */
function setFormAndWaitForFieldFilled(
  form,
  { fieldSelector, fieldValue, formId }
) {
  // eslint-disable-next-line no-unsanitized/property
  document.querySelector("#content").innerHTML = form;
  return SimpleTest.promiseWaitForCondition(() => {
    let ancestor = formId
      ? document.querySelector("#" + formId)
      : document.documentElement;
    return ancestor.querySelector(fieldSelector).value == fieldValue;
  }, "Wait for password manager to fill form");
}

/**
 * Run commonInit synchronously in the parent then run the test function after the runTests event.
 *
 * @param {Function} aFunction The test function to run
 */
function runChecksAfterCommonInit(aFunction = null) {
  SimpleTest.waitForExplicitFinish();
  if (aFunction) {
    window.addEventListener("runTests", aFunction);
    PWMGR_COMMON_PARENT.addMessageListener("registerRunTests", () =>
      registerRunTests()
    );
  }
  PWMGR_COMMON_PARENT.sendAsyncMessage("setupParent", {
    testDependsOnDeprecatedLogin: gTestDependsOnDeprecatedLogin,
  });
  return PWMGR_COMMON_PARENT;
}

// Begin code that runs immediately for all tests that include this file.

const PWMGR_COMMON_PARENT = runInParent(
  SimpleTest.getTestFileURL("pwmgr_common_parent.js")
);

SimpleTest.registerCleanupFunction(() => {
  SpecialPowers.flushPrefEnv();

  PWMGR_COMMON_PARENT.sendAsyncMessage("cleanup");

  runInParent(function cleanupParent() {
    // eslint-disable-next-line no-shadow
    const { Services } = ChromeUtils.import(
      "resource://gre/modules/Services.jsm"
    );
    // eslint-disable-next-line no-shadow
    const { LoginManagerParent } = ChromeUtils.import(
      "resource://gre/modules/LoginManagerParent.jsm"
    );

    // Remove all logins and disabled hosts
    Services.logins.removeAllLogins();

    let disabledHosts = Services.logins.getAllDisabledHosts();
    disabledHosts.forEach(host =>
      Services.logins.setLoginSavingEnabled(host, true)
    );

    let authMgr = Cc["@mozilla.org/network/http-auth-manager;1"].getService(
      Ci.nsIHttpAuthManager
    );
    authMgr.clearAll();

    // Check that it's not null, instead of truthy to catch it becoming undefined
    // in a refactoring.
    if (LoginManagerParent._recipeManager !== null) {
      LoginManagerParent._recipeManager.reset();
    }

    // Cleanup PopupNotifications (if on a relevant platform)
    let chromeWin = Services.wm.getMostRecentWindow("navigator:browser");
    if (chromeWin && chromeWin.PopupNotifications) {
      let notes = chromeWin.PopupNotifications._currentNotifications;
      if (notes.length) {
        dump("Removing " + notes.length + " popup notifications.\n");
      }
      for (let note of notes) {
        note.remove();
      }
    }

    // Clear events last in case the above cleanup records events.
    Services.telemetry.clearEvents();
  });
});

/**
 * Proxy for Services.logins (nsILoginManager).
 * Only supports arguments which support structured clone plus {nsILoginInfo}
 * Assumes properties are methods.
 */
this.LoginManager = new Proxy(
  {},
  {
    get(target, prop, receiver) {
      return (...args) => {
        let loginInfoIndices = [];
        let cloneableArgs = args.map((val, index) => {
          if (
            SpecialPowers.call_Instanceof(val, SpecialPowers.Ci.nsILoginInfo)
          ) {
            loginInfoIndices.push(index);
            return LoginHelper.loginToVanillaObject(val);
          }

          return val;
        });

        return PWMGR_COMMON_PARENT.sendQuery("proxyLoginManager", {
          args: cloneableArgs,
          loginInfoIndices,
          methodName: prop,
        });
      };
    },
  }
);
