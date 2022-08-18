/**
 * Helpers for password manager mochitest-plain tests.
 */

/* import-globals-from ../../../../../toolkit/components/satchel/test/satchel_common.js */

const { LoginTestUtils } = SpecialPowers.ChromeUtils.import(
  "resource://testing-common/LoginTestUtils.jsm"
);
const Services = SpecialPowers.Services;

// Setup LoginTestUtils to report assertions to the mochitest harness.
LoginTestUtils.setAssertReporter(
  SpecialPowers.wrapCallback((err, message, stack) => {
    SimpleTest.record(!err, err ? err.message : message, null, stack);
  })
);

const { LoginHelper } = SpecialPowers.ChromeUtils.import(
  "resource://gre/modules/LoginHelper.jsm"
);

const {
  LENGTH: GENERATED_PASSWORD_LENGTH,
  REGEX: GENERATED_PASSWORD_REGEX,
} = LoginTestUtils.generation;
const LOGIN_FIELD_UTILS = LoginTestUtils.loginField;
const TESTS_DIR = "/tests/toolkit/components/passwordmgr/test/";

// Depending on pref state we either show auth prompts as windows or on tab level.
let authPromptModalType = SpecialPowers.Services.prefs.getIntPref(
  "prompts.modalType.httpAuth"
);

// Whether the auth prompt is a commonDialog.xhtml or a TabModalPrompt
let authPromptIsCommonDialog =
  authPromptModalType === SpecialPowers.Services.prompt.MODAL_TYPE_WINDOW ||
  (authPromptModalType === SpecialPowers.Services.prompt.MODAL_TYPE_TAB &&
    SpecialPowers.Services.prefs.getBoolPref(
      "prompts.tabChromePromptSubDialog",
      false
    ));

/**
 * Recreate a DOM tree using the outerHTML to ensure that any event listeners
 * and internal state for the elements are removed.
 */
function recreateTree(element) {
  // eslint-disable-next-line no-unsanitized/property, no-self-assign
  element.outerHTML = element.outerHTML;
}

function _checkArrayValues(actualValues, expectedValues, msg) {
  is(
    actualValues.length,
    expectedValues.length,
    "Checking array values: " + msg
  );
  for (let i = 0; i < expectedValues.length; i++) {
    is(actualValues[i], expectedValues[i], msg + " Checking array entry #" + i);
  }
}

/**
 * Check autocomplete popup results to ensure that expected
 * *labels* are being shown correctly as items in the popup.
 */
function checkAutoCompleteResults(actualValues, expectedValues, hostname, msg) {
  if (hostname === null) {
    _checkArrayValues(actualValues, expectedValues, msg);
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
  _checkArrayValues(actualValues.slice(0, -1), expectedValues, msg);
}

function getIframeBrowsingContext(window, iframeNumber = 0) {
  let bc = SpecialPowers.wrap(window).windowGlobalChild.browsingContext;
  return SpecialPowers.unwrap(bc.children[iframeNumber]);
}

/**
 * Set input values via setUserInput to emulate user input
 * and distinguish them from declarative or script-assigned values
 */
function setUserInputValues(parentNode, selectorValues, userInput = true) {
  for (let [selector, newValue] of Object.entries(selectorValues)) {
    info(`setUserInputValues, selector: ${selector}`);
    try {
      let field = SpecialPowers.wrap(parentNode.querySelector(selector));
      if (field.value == newValue) {
        // we don't get an input event if the new value == the old
        field.value += "#";
      }
      if (userInput) {
        field.setUserInput(newValue);
      } else {
        field.value = newValue;
      }
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

function checkLoginFormInFrame(
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

async function checkUnmodifiedFormInFrame(bc, formNum) {
  return SpecialPowers.spawn(bc, [formNum], formNumF => {
    let form = this.content.document.getElementById(`form${formNumF}`);
    ok(form, "Locating form " + formNumF);

    for (var i = 0; i < form.elements.length; i++) {
      var ele = form.elements[i];

      // No point in checking form submit/reset buttons.
      if (ele.type == "submit" || ele.type == "reset") {
        continue;
      }

      is(
        ele.value,
        ele.defaultValue,
        "Test to default value of field " + ele.name + " in form " + formNumF
      );
    }
  });
}

/**
 * Check a form for expected values even if it is in a different top level window
 * or process. If an argument is null, a field's expected value will be the default
 * value.
 *
 * Similar to the checkForm helper, but it works across (cross-origin) frames.
 *
 * <form id="form#">
 * checkLoginFormInFrameWithElementValues(#, "foo");
 */
async function checkLoginFormInFrameWithElementValues(
  browsingContext,
  formNum,
  ...values
) {
  return SpecialPowers.spawn(
    browsingContext,
    [formNum, values],
    function checkFormWithElementValues(formNumF, valuesF) {
      let [val1F, val2F, val3F] = valuesF;
      let doc = this.content.document;
      let e;
      let form = doc.getElementById("form" + formNumF);
      ok(form, "Locating form " + formNumF);

      let numToCheck = arguments.length - 1;

      if (!numToCheck--) {
        return;
      }
      e = form.elements[0];
      if (val1F == null) {
        is(
          e.value,
          e.defaultValue,
          "Test default value of field " + e.name + " in form " + formNumF
        );
      } else {
        is(
          e.value,
          val1F,
          "Test value of field " + e.name + " in form " + formNumF
        );
      }

      if (!numToCheck--) {
        return;
      }

      e = form.elements[1];
      if (val2F == null) {
        is(
          e.value,
          e.defaultValue,
          "Test default value of field " + e.name + " in form " + formNumF
        );
      } else {
        is(
          e.value,
          val2F,
          "Test value of field " + e.name + " in form " + formNumF
        );
      }

      if (!numToCheck--) {
        return;
      }
      e = form.elements[2];
      if (val3F == null) {
        is(
          e.value,
          e.defaultValue,
          "Test default value of field " + e.name + " in form " + formNumF
        );
      } else {
        is(
          e.value,
          val3F,
          "Test value of field " + e.name + " in form " + formNumF
        );
      }
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

/**
 * Wait for the document to be ready and any existing password fields on
 * forms to be processed.
 *
 * @param existingPasswordFieldsCount the number of password fields
 * that begin on the test page.
 */
function registerRunTests(existingPasswordFieldsCount = 0) {
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

      let foundForcer = false;
      var observer = SpecialPowers.wrapCallback(function(subject, topic, data) {
        if (data === "observerforcer") {
          foundForcer = true;
        } else {
          existingPasswordFieldsCount--;
        }

        if (!foundForcer || existingPasswordFieldsCount > 0) {
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

function enablePrimaryPassword() {
  setPrimaryPassword(true);
}

function disablePrimaryPassword() {
  setPrimaryPassword(false);
}

function setPrimaryPassword(enable) {
  PWMGR_COMMON_PARENT.sendAsyncMessage("setPrimaryPassword", { enable });
}

function isLoggedIn() {
  return PWMGR_COMMON_PARENT.sendQuery("isLoggedIn");
}

function logoutPrimaryPassword() {
  runInParent(function parent_logoutPrimaryPassword() {
    var sdr = Cc["@mozilla.org/security/sdr;1"].getService(
      Ci.nsISecretDecoderRing
    );
    sdr.logoutAndTeardown();
  });
}

/**
 * Resolves when a specified number of forms have been processed for (potential) filling.
 * This relies on the observer service which only notifies observers within the same process.
 */
function promiseFormsProcessedInSameProcess(expectedCount = 1) {
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

/**
 * Resolves when a form has been processed for (potential) filling.
 * This works across processes.
 */
async function promiseFormsProcessed(expectedCount = 1) {
  var processedCount = 0;
  return new Promise(resolve => {
    PWMGR_COMMON_PARENT.addMessageListener(
      "formProcessed",
      function formProcessed() {
        processedCount++;
        if (processedCount == expectedCount) {
          PWMGR_COMMON_PARENT.removeMessageListener(
            "formProcessed",
            formProcessed
          );
          resolve();
        }
      }
    );
  });
}

async function loadFormIntoWindow(origin, html, win, task) {
  let loadedPromise = new Promise(resolve => {
    win.addEventListener(
      "load",
      function(event) {
        if (event.target.location.href.endsWith("blank.html")) {
          resolve();
        }
      },
      { once: true }
    );
  });

  let processedPromise = promiseFormsProcessed();
  win.location =
    origin + "/tests/toolkit/components/passwordmgr/test/mochitest/blank.html";
  info(`Waiting for window to load for origin: ${origin}`);
  await loadedPromise;

  await SpecialPowers.spawn(win, [html, task?.toString()], function(
    contentHtml,
    contentTask = null
  ) {
    // eslint-disable-next-line no-unsanitized/property
    this.content.document.documentElement.innerHTML = contentHtml;
    // Similar to the invokeContentTask helper in accessible/tests/browser/shared-head.js
    if (contentTask) {
      // eslint-disable-next-line no-eval
      const runnableTask = eval(`
      (() => {
        return (${contentTask});
      })();`);
      runnableTask.call(this);
    }
  });

  info("Waiting for the form to be processed");
  await processedPromise;
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

function resetWebsitesWithSharedCredential() {
  info("Resetting the 'websites-with-shared-credential-backend' collection");
  return new Promise(resolve => {
    PWMGR_COMMON_PARENT.addMessageListener(
      "resetWebsitesWithSharedCredential",
      function reset() {
        PWMGR_COMMON_PARENT.removeMessageListener(
          "resetWebsitesWithSharedCredential",
          reset
        );
        resolve();
      }
    );
    PWMGR_COMMON_PARENT.sendAsyncMessage("resetWebsitesWithSharedCredential");
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

/** Initialize with a list of logins. The logins are added within the parent chrome process.
 * @param {array} aLogins - a list of logins to add. Each login is an array of the arguments
 *                          that would be passed to nsLoginInfo.init().
 */
function addLoginsInParent(...aLogins) {
  let script = runInParent(function addLoginsInParentInner() {
    /* eslint-env mozilla/chrome-script */
    addMessageListener("addLogins", logins => {
      let nsLoginInfo = Components.Constructor(
        "@mozilla.org/login-manager/loginInfo;1",
        Ci.nsILoginInfo,
        "init"
      );

      for (let login of logins) {
        let loginInfo = new nsLoginInfo(...login);
        try {
          Services.logins.addLogin(loginInfo);
        } catch (e) {
          assert.ok(false, "addLogin threw: " + e);
        }
      }
    });
  });
  script.sendQuery("addLogins", aLogins);
  return script;
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
    /* eslint-env mozilla/chrome-script */
    // eslint-disable-next-line no-shadow
    const { LoginManagerParent } = ChromeUtils.import(
      "resource://gre/modules/LoginManagerParent.jsm"
    );

    // Remove all logins and disabled hosts
    Services.logins.removeAllUserFacingLogins();

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
