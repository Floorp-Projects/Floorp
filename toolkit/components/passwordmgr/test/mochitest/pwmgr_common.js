/**
 * Helpers for password manager mochitest-plain tests.
 */

/* import-globals-from ../../../../../toolkit/components/satchel/test/satchel_common.js */

const { LoginTestUtils } = SpecialPowers.ChromeUtils.importESModule(
  "resource://testing-common/LoginTestUtils.sys.mjs"
);
const Services = SpecialPowers.Services;

// Setup LoginTestUtils to report assertions to the mochitest harness.
LoginTestUtils.setAssertReporter(
  SpecialPowers.wrapCallback((err, message, stack) => {
    SimpleTest.record(!err, err ? err.message : message, null, stack);
  })
);

const { LoginHelper } = SpecialPowers.ChromeUtils.importESModule(
  "resource://gre/modules/LoginHelper.sys.mjs"
);

const { LENGTH: GENERATED_PASSWORD_LENGTH, REGEX: GENERATED_PASSWORD_REGEX } =
  LoginTestUtils.generation;
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
  // eslint-disable-next-line no-self-assign
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

  isnot(
    actualValues.length,
    0,
    "There should be items in the autocomplete popup: " +
      JSON.stringify(actualValues)
  );

  // Check the footer first.
  let footerResult = actualValues[actualValues.length - 1];
  is(footerResult, "Manage Passwords", "the footer text is shown correctly");

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

/**
 * Wait for autocomplete popup to get closed
 * @return {Promise} resolving when the AC popup is closed
 */
async function untilAutocompletePopupClosed() {
  return SimpleTest.promiseWaitForCondition(async () => {
    const popupState = await getPopupState();
    return !popupState.open;
  }, "Wait for autocomplete popup to be closed");
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
 * Create a login form and insert into contents dom (identified by id
 * `content`). If the form (identified by its number) is already present in the
 * dom, it gets replaced.
 *
 * @param {number} [num = 1] - number of the form, used as id, eg `form1`
 * @param {string} [action = ""] - action attribute of the form
 * @param {string} [autocomplete  = null] - forms autocomplete attribute. Default is none
 * @param {object} [username = {}] - object describing attributes to the username field:
 * @param {string} [username.id = null] - id of the field
 * @param {string} [username.name = "uname"] - name attribute
 * @param {string} [username.type = "text"] - type of the field
 * @param {string} [username.value = null] - initial value of the field
 * @param {string} [username.autocomplete = null] - autocomplete attribute
 * @param {object} [password = {}] - an object describing attributes to the password field. If falsy, do not create a password field
 * @param {string} [password.id = null] - id of the field
 * @param {string} [password.name = "pword"] - name attribute
 * @param {string} [password.type = "password"] - type of the field
 * @param {string} [password.value = null] - initial value of the field
 * @param {string} [password.label = null] - if present, wrap field in a label containing its value
 * @param {string} [password.autocomplete = null] - autocomplete attribute
 *
 * @return {HTMLDomElement} the form
 */
function createLoginForm({
  num = 1,
  action = "",
  autocomplete = null,
  username = {},
  password = {},
} = {}) {
  username.name ||= "uname";
  username.type ||= "text";
  username.id ||= null;
  username.value ||= null;
  username.autocomplete ||= null;

  password.name ||= "pword";
  password.type ||= "password";
  password.id ||= null;
  password.value ||= null;
  password.label ||= null;
  password.autocomplete ||= null;
  password.readonly ||= null;
  password.disabled ||= null;

  info(
    `Creating login form ${JSON.stringify({ num, action, username, password })}`
  );

  const form = document.createElement("form");
  form.id = `form${num}`;
  form.action = action;
  form.onsubmit = () => false;

  if (autocomplete != null) {
    form.setAttribute("autocomplete", autocomplete);
  }

  const usernameInput = document.createElement("input");

  usernameInput.type = username.type;
  usernameInput.name = username.name;

  if (username.id != null) {
    usernameInput.id = username.id;
  }
  if (username.value != null) {
    usernameInput.value = username.value;
  }
  if (username.autocomplete != null) {
    usernameInput.setAttribute("autocomplete", username.autocomplete);
  }

  form.appendChild(usernameInput);

  if (password) {
    const passwordInput = document.createElement("input");

    passwordInput.type = password.type;
    passwordInput.name = password.name;

    if (password.id != null) {
      passwordInput.id = password.id;
    }
    if (password.value != null) {
      passwordInput.value = password.value;
    }
    if (password.autocomplete != null) {
      passwordInput.setAttribute("autocomplete", password.autocomplete);
    }
    if (password.readonly != null) {
      passwordInput.setAttribute("readonly", password.readonly);
    }
    if (password.disabled != null) {
      passwordInput.setAttribute("disabled", password.disabled);
    }

    if (password.label != null) {
      const passwordLabel = document.createElement("label");
      passwordLabel.innerText = password.label;
      passwordLabel.appendChild(passwordInput);
      form.appendChild(passwordLabel);
    } else {
      form.appendChild(passwordInput);
    }
  }

  const submitButton = document.createElement("button");
  submitButton.type = "submit";
  submitButton.name = "submit";
  submitButton.innerText = "Submit";
  form.appendChild(submitButton);

  const content = document.getElementById("content");

  const oldForm = document.getElementById(form.id);
  if (oldForm) {
    content.replaceChild(form, oldForm);
  } else {
    content.appendChild(form);
  }

  return form;
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

/**
 * Check repeatedly for a while to see if a particular condition still applies.
 * This function checks the return value of `condition` repeatedly until either
 * the condition has a falsy return value, or `retryTimes` is exceeded.
 */

function ensureCondition(
  condition,
  errorMsg = "Condition did not last.",
  retryTimes = 10
) {
  return new Promise((resolve, reject) => {
    let tries = 0;
    let conditionFailed = false;
    let interval = setInterval(async function () {
      try {
        const conditionPassed = await condition();
        conditionFailed ||= !conditionPassed;
      } catch (e) {
        ok(false, e + "\n" + e.stack);
        conditionFailed = true;
      }
      if (conditionFailed || tries >= retryTimes) {
        ok(!conditionFailed, errorMsg);
        clearInterval(interval);
        if (conditionFailed) {
          reject(errorMsg);
        } else {
          resolve();
        }
      }
      tries++;
    }, 100);
  });
}

/**
 * Wait a while to ensure login form stays filled with username and password
 * @see `checkLoginForm` below for a similar function.
 * @returns a promise, resolving when done
 *
 * TODO: eventually get rid of this time based check, and transition to an
 * event based approach. See Bug 1811142.
 * Filling happens by `_fillForm()` which can report it's decision and we can
 * wait for it. One of the options is to have `didFillFormAsync()` from
 * https://phabricator.services.mozilla.com/D167214#change-3njWgUgqswws
 */
function ensureLoginFormStaysFilledWith(
  usernameField,
  expectedUsername,
  passwordField,
  expectedPassword
) {
  return ensureCondition(() => {
    return (
      Object.is(usernameField.value, expectedUsername) &&
      Object.is(passwordField.value, expectedPassword)
    );
  }, `Ensuring form ${usernameField.parentNode.id} stays filled with "${expectedUsername}:${expectedPassword}"`);
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
      let usernameField =
        this.content.document.getElementById(usernameFieldIdF);
      let passwordField =
        this.content.document.getElementById(passwordFieldIdF);

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
function registerRunTests(existingPasswordFieldsCount = 0, callback) {
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
      var observer = SpecialPowers.wrapCallback(function (
        subject,
        topic,
        data
      ) {
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
          callback?.();
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
  info(`waiting for ${expectedCount} forms to be processed`);
  var processedCount = 0;
  return new Promise(resolve => {
    PWMGR_COMMON_PARENT.addMessageListener(
      "formProcessed",
      function formProcessed() {
        processedCount++;
        info(`processed form ${processedCount} of ${expectedCount}`);
        if (processedCount == expectedCount) {
          info(`processing of ${expectedCount} forms complete`);
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

async function loadFormIntoWindow(origin, html, win, expectedCount = 1, task) {
  let loadedPromise = new Promise(resolve => {
    win.addEventListener(
      "load",
      function (event) {
        if (event.target.location.href.endsWith("blank.html")) {
          resolve();
        }
      },
      { once: true }
    );
  });

  let processedPromise = promiseFormsProcessed(expectedCount);
  win.location =
    origin + "/tests/toolkit/components/passwordmgr/test/mochitest/blank.html";
  info(`Waiting for window to load for origin: ${origin}`);
  await loadedPromise;

  await SpecialPowers.spawn(
    win,
    [html, task?.toString()],
    function (contentHtml, contentTask = null) {
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
    }
  );

  info("Waiting for the form to be processed");
  await processedPromise;
}

async function getTelemetryEvents(options) {
  let events = await PWMGR_COMMON_PARENT.sendQuery(
    "getTelemetryEvents",
    options
  );
  info("CONTENT: getTelemetryEvents gotResult: " + JSON.stringify(events));
  return events;
}

function loadRecipes(recipes) {
  info("Loading recipes");
  return PWMGR_COMMON_PARENT.sendQuery("loadRecipes", recipes);
}

function resetRecipes() {
  info("Resetting recipes");
  return PWMGR_COMMON_PARENT.sendQuery("resetRecipes");
}

async function promiseStorageChanged(expectedChangeTypes) {
  let result = await PWMGR_COMMON_PARENT.sendQuery("storageChanged", {
    expectedChangeTypes,
  });

  if (result) {
    ok(false, result);
  }
}

async function promisePromptShown(expectedTopic) {
  let topic = await PWMGR_COMMON_PARENT.sendQuery("promptShown");
  is(topic, expectedTopic, "Check expected prompt topic");
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

/** Manage logins in parent chrome process.
 * */
function manageLoginsInParent() {
  return runInParent(function addLoginsInParentInner() {
    /* eslint-env mozilla/chrome-script */
    addMessageListener("removeAllUserFacingLogins", () => {
      Services.logins.removeAllUserFacingLogins();
    });

    /* eslint-env mozilla/chrome-script */
    addMessageListener("getLogins", async () => {
      const logins = await Services.logins.getAllLogins();
      return logins.map(
        ({
          origin,
          formActionOrigin,
          httpRealm,
          username,
          password,
          usernameField,
          passwordField,
        }) => [
          origin,
          formActionOrigin,
          httpRealm,
          username,
          password,
          usernameField,
          passwordField,
        ]
      );
    });

    /* eslint-env mozilla/chrome-script */
    addMessageListener("addLogins", async logins => {
      let nsLoginInfo = Components.Constructor(
        "@mozilla.org/login-manager/loginInfo;1",
        Ci.nsILoginInfo,
        "init"
      );

      const loginInfos = logins.map(login => new nsLoginInfo(...login));
      try {
        await Services.logins.addLogins(loginInfos);
      } catch (e) {
        assert.ok(false, "addLogins threw: " + e);
      }
    });
  });
}

/** Initialize with a list of logins. The logins are added within the parent chrome process.
 * @param {array} aLogins - a list of logins to add. Each login is an array of the arguments
 *                          that would be passed to nsLoginInfo.init().
 */
async function addLoginsInParent(...aLogins) {
  const script = manageLoginsInParent();
  await script.sendQuery("addLogins", aLogins);
  return script;
}

/** Initialize with a list of logins, after removing all user facing logins.
 * The logins are added within the parent chrome process.
 * @param {array} aLogins - a list of logins to add. Each login is an array of the arguments
 *                          that would be passed to nsLoginInfo.init().
 */
async function setStoredLoginsAsync(...aLogins) {
  const script = manageLoginsInParent();
  script.sendQuery("removeAllUserFacingLogins");
  await script.sendQuery("addLogins", aLogins);
  return script;
}

/**
 * Sets given logins for the duration of the test. Existing logins are first
 * removed and finally restored when the test is finished.
 * The logins are added within the parent chrome process.
 * @param {array} logins - a list of logins to add. Each login is an array of the arguments
 *                          that would be passed to nsLoginInfo.init().
 */
async function setStoredLoginsDuringTest(...logins) {
  const script = manageLoginsInParent();
  const loginsBefore = await script.sendQuery("getLogins");
  await script.sendQuery("removeAllUserFacingLogins");
  await script.sendQuery("addLogins", logins);
  SimpleTest.registerCleanupFunction(async () => {
    await script.sendQuery("removeAllUserFacingLogins");
    await script.sendQuery("addLogins", loginsBefore);
  });
}

/**
 * Sets given logins for the duration of the task. Existing logins are first
 * removed and finally restored when the task is finished.
 * @param {array} logins - a list of logins to add. Each login is an array of the arguments
 *                          that would be passed to nsLoginInfo.init().
 */
async function setStoredLoginsDuringTask(...logins) {
  const script = manageLoginsInParent();
  const loginsBefore = await script.sendQuery("getLogins");
  await script.sendQuery("removeAllUserFacingLogins");
  await script.sendQuery("addLogins", logins);
  SimpleTest.registerTaskCleanupFunction(async () => {
    await script.sendQuery("removeAllUserFacingLogins");
    await script.sendQuery("addLogins", loginsBefore);
  });
}

/** Returns a promise which resolves to a list of logins
 */
function getLogins() {
  const script = manageLoginsInParent();
  return script.sendQuery("getLogins");
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
  document.querySelector("#content").innerHTML = form;
  return SimpleTest.promiseWaitForCondition(() => {
    let ancestor = formId
      ? document.querySelector("#" + formId)
      : document.documentElement;
    return ancestor.querySelector(fieldSelector).value == fieldValue;
  }, "Wait for password manager to fill form");
}

/**
 * Run commonInit synchronously in the parent then run the test function if supplied.
 *
 * @param {Function} aFunction The test function to run
 */
async function runChecksAfterCommonInit(aFunction = null) {
  SimpleTest.waitForExplicitFinish();
  await PWMGR_COMMON_PARENT.sendQuery("setupParent", {
    testDependsOnDeprecatedLogin: gTestDependsOnDeprecatedLogin,
  });

  if (aFunction) {
    await registerRunTests(0, aFunction);
  }

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
    const { LoginManagerParent } = ChromeUtils.importESModule(
      "resource://gre/modules/LoginManagerParent.sys.mjs"
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

/**
 * Set the inner html of the content div and ensure it gets reset after current
 * task finishes.
 * Returns the first child node of the newly created content div for convenient
 * access of the newly created dom node.
 *
 * @param {String} html
 *        string of dom content or dom element to be inserted into content element
 */
function setContentForTask(html) {
  const content = document.querySelector("#content");
  const innerHTMLBefore = content.innerHTML || "";
  SimpleTest.registerCurrentTaskCleanupFunction(
    () => (content.innerHTML = innerHTMLBefore)
  );
  if (html.content?.cloneNode) {
    const clone = html.content.cloneNode(true);
    content.replaceChildren(clone);
  } else {
    content.innerHTML = html;
  }
  return content.firstElementChild;
}

/*
 * Set preferences via SpecialPowers.pushPrefEnv and reset them after current
 * task has finished.
 *
 * @param {*Object} preferences
 * */
async function setPreferencesForTask(...preferences) {
  await SpecialPowers.pushPrefEnv({
    set: preferences,
  });
  SimpleTest.registerCurrentTaskCleanupFunction(() => SpecialPowers.popPrefEnv);
}

// capture form autofill results between tasks
let gPwmgrCommonCapturedAutofillResults = {};
PWMGR_COMMON_PARENT.addMessageListener(
  "formProcessed",
  ({ formId, autofillResult }) => {
    if (formId === "observerforcer") {
      return;
    }

    gPwmgrCommonCapturedAutofillResults[formId] = autofillResult;
  }
);
SimpleTest.registerTaskCleanupFunction(() => {
  gPwmgrCommonCapturedAutofillResults = {};
});

/**
 * Create a promise that resolves when the form has been processed.
 * Works with forms processed in the past since the task started and in the future,
 * across parent and child processes.
 *
 * @param {String} formId / the id of the form of which to expect formautofill events
 * @returns promise, resolving with the autofill result.
 */
async function formAutofillResult(formId) {
  if (formId in gPwmgrCommonCapturedAutofillResults) {
    const autofillResult = gPwmgrCommonCapturedAutofillResults[formId];
    delete gPwmgrCommonCapturedAutofillResults[formId];
    return autofillResult;
  }
  return new Promise((resolve, reject) => {
    PWMGR_COMMON_PARENT.addMessageListener(
      "formProcessed",
      ({ formId: id, autofillResult }) => {
        if (id !== formId) {
          return;
        }
        delete gPwmgrCommonCapturedAutofillResults[formId];
        resolve(autofillResult);
      },
      { once: true }
    );
  });
}

function sendFakeAutocompleteEvent(element) {
  const acEvent = document.createEvent("HTMLEvents");
  acEvent.initEvent("DOMAutoComplete", true, false);
  element.dispatchEvent(acEvent);
}
