/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.importGlobalProperties(["URL"]);
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "UserAutoCompleteResult",
                                  "resource://gre/modules/LoginManagerContent.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AutoCompleteE10S",
                                  "resource://gre/modules/AutoCompleteE10S.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DeferredTask",
                                  "resource://gre/modules/DeferredTask.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LoginDoorhangers",
                                  "resource://gre/modules/LoginDoorhangers.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LoginHelper",
                                  "resource://gre/modules/LoginHelper.jsm");

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let logger = LoginHelper.createLogger("LoginManagerParent");
  return logger.log.bind(logger);
});

this.EXPORTED_SYMBOLS = [ "LoginManagerParent", "PasswordsMetricsProvider" ];

#ifndef ANDROID
#ifdef MOZ_SERVICES_HEALTHREPORT
XPCOMUtils.defineLazyModuleGetter(this, "Metrics",
                                  "resource://gre/modules/Metrics.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

function recordFHRDailyCounter(aField) {
    let reporter = Cc["@mozilla.org/datareporting/service;1"]
                      .getService()
                      .wrappedJSObject
                      .healthReporter;
    // This can happen if the FHR component of the data reporting service is
    // disabled. This is controlled by a pref that most will never use.
    if (!reporter) {
      return;
    }
      reporter.onInit().then(() => reporter.getProvider("org.mozilla.passwordmgr")
        .recordDailyCounter(aField));
  }

this.PasswordsMetricsProvider = function() {
  Metrics.Provider.call(this);
};

PasswordsMetricsProvider.prototype = Object.freeze({
  __proto__: Metrics.Provider.prototype,

  name: "org.mozilla.passwordmgr",

  measurementTypes: [
    PasswordsMeasurement1,
    PasswordsMeasurement2,
  ],

  collectDailyData: function () {
    return this.storage.enqueueTransaction(this._recordDailyPasswordData.bind(this));
  },

  _recordDailyPasswordData: function *() {
    let m = this.getMeasurement(PasswordsMeasurement2.prototype.name,
                                PasswordsMeasurement2.prototype.version);
    let enabled = Services.prefs.getBoolPref("signon.rememberSignons");
    yield m.setDailyLastNumeric("enabled", enabled ? 1 : 0);

    let loginsCount = Services.logins.countLogins("", "", "");
    yield m.setDailyLastNumeric("numSavedPasswords", loginsCount);

  },

  recordDailyCounter: function(aField) {
    let m = this.getMeasurement(PasswordsMeasurement2.prototype.name,
                                PasswordsMeasurement2.prototype.version);
    if (this.storage.hasFieldFromMeasurement(m.id, aField,
                                             Metrics.Storage.FIELD_DAILY_COUNTER)) {
      let fieldID = this.storage.fieldIDFromMeasurement(m.id, aField, Metrics.Storage.FIELD_DAILY_COUNTER);
      return this.enqueueStorageOperation(() => m.incrementDailyCounter(aField));
    }

    // Otherwise, we first need to create the field.
    return this.enqueueStorageOperation (() => this.storage.registerField(m.id, aField, 
      Metrics.Storage.FIELD_DAILY_COUNTER).then(() => m.incrementDailyCounter(aField)));
  },
});

function PasswordsMeasurement1() {
  Metrics.Measurement.call(this);
}

PasswordsMeasurement1.prototype = Object.freeze({
  __proto__: Metrics.Measurement.prototype,
  name: "passwordmgr",
  version: 1,
  fields: {
    enabled: {type: Metrics.Storage.FIELD_DAILY_LAST_NUMERIC},
    numSavedPasswords: {type: Metrics.Storage.FIELD_DAILY_LAST_NUMERIC},
  },
});

function PasswordsMeasurement2() {
  Metrics.Measurement.call(this);
}
PasswordsMeasurement2.prototype = Object.freeze({
  __proto__: Metrics.Measurement.prototype,
  name: "passwordmgr",
  version: 2,
  fields: {
    enabled: {type: Metrics.Storage.FIELD_DAILY_LAST_NUMERIC},
    numSavedPasswords: {type: Metrics.Storage.FIELD_DAILY_LAST_NUMERIC},
    numSuccessfulFills: {type: Metrics.Storage.FIELD_DAILY_COUNTER},
    numNewSavedPasswordsInSession: {type: Metrics.Storage.FIELD_DAILY_COUNTER},
    numTotalLoginsEncountered: {type: Metrics.Storage.FIELD_DAILY_COUNTER},
  },
});

#endif
#endif

var LoginManagerParent = {
  /**
   * Reference to the default LoginRecipesParent (instead of the initialization promise) for
   * synchronous access. This is a temporary hack and new consumers should yield on
   * recipeParentPromise instead.
   *
   * @type LoginRecipesParent
   * @deprecated
   */
  _recipeManager: null,

  init: function() {
    let mm = Cc["@mozilla.org/globalmessagemanager;1"]
               .getService(Ci.nsIMessageListenerManager);
    mm.addMessageListener("RemoteLogins:findLogins", this);
    mm.addMessageListener("RemoteLogins:findRecipes", this);
    mm.addMessageListener("RemoteLogins:onFormSubmit", this);
    mm.addMessageListener("RemoteLogins:autoCompleteLogins", this);
    mm.addMessageListener("RemoteLogins:updateLoginFormPresence", this);
    mm.addMessageListener("LoginStats:LoginEncountered", this);
    mm.addMessageListener("LoginStats:LoginFillSuccessful", this);
    Services.obs.addObserver(this, "LoginStats:NewSavedPassword", false);

    XPCOMUtils.defineLazyGetter(this, "recipeParentPromise", () => {
      const { LoginRecipesParent } = Cu.import("resource://gre/modules/LoginRecipes.jsm", {});
      this._recipeManager = new LoginRecipesParent({
        defaults: Services.prefs.getComplexValue("signon.recipes.path", Ci.nsISupportsString).data,
      });
      return this._recipeManager.initializationPromise;
    });

  },

  observe: function (aSubject, aTopic, aData) {
#ifndef ANDROID
#ifdef MOZ_SERVICES_HEALTHREPORT
    if (aTopic == "LoginStats:NewSavedPassword") {
      recordFHRDailyCounter("numNewSavedPasswordsInSession");

    }
#endif
#endif
  },

  receiveMessage: function (msg) {
    let data = msg.data;
    switch (msg.name) {
      case "RemoteLogins:findLogins": {
        // TODO Verify msg.target's principals against the formOrigin?
        this.sendLoginDataToChild(data.options.showMasterPassword,
                                  data.formOrigin,
                                  data.actionOrigin,
                                  data.requestId,
                                  msg.target.messageManager);
        break;
      }

      case "RemoteLogins:findRecipes": {
        let formHost = (new URL(data.formOrigin)).host;
        return this._recipeManager.getRecipesForHost(formHost);
      }

      case "RemoteLogins:onFormSubmit": {
        // TODO Verify msg.target's principals against the formOrigin?
        this.onFormSubmit(data.hostname,
                          data.formSubmitURL,
                          data.usernameField,
                          data.newPasswordField,
                          data.oldPasswordField,
                          msg.objects.openerWin,
                          msg.target);
        break;
      }

      case "RemoteLogins:updateLoginFormPresence": {
        this.updateLoginFormPresence(msg.target, data);
        break;
      }

      case "RemoteLogins:autoCompleteLogins": {
        this.doAutocompleteSearch(data, msg.target);
        break;
      }

      case "LoginStats:LoginFillSuccessful": {
#ifndef ANDROID
#ifdef MOZ_SERVICES_HEALTHREPORT
        recordFHRDailyCounter("numSuccessfulFills");
#endif
#endif
        break;
      }

      case "LoginStats:LoginEncountered": {
#ifndef ANDROID
#ifdef MOZ_SERVICES_HEALTHREPORT
        recordFHRDailyCounter("numTotalLoginsEncountered");
#endif
#endif
        break;
      }
    }
  },

  /**
   * Trigger a login form fill and send relevant data (e.g. logins and recipes)
   * to the child process (LoginManagerContent).
   */
  fillForm: Task.async(function* ({ browser, loginFormOrigin, login, inputElement }) {
    let recipes = [];
    if (loginFormOrigin) {
      let formHost;
      try {
        formHost = (new URL(loginFormOrigin)).host;
        let recipeManager = yield this.recipeParentPromise;
        recipes = recipeManager.getRecipesForHost(formHost);
      } catch (ex) {
        // Some schemes e.g. chrome aren't supported by URL
      }
    }

    // Convert the array of nsILoginInfo to vanilla JS objects since nsILoginInfo
    // doesn't support structured cloning.
    let jsLogins = JSON.parse(JSON.stringify([login]));

    let objects = inputElement ? {inputElement} : null;
    browser.messageManager.sendAsyncMessage("RemoteLogins:fillForm", {
      loginFormOrigin,
      logins: jsLogins,
      recipes,
    }, objects);
  }),

  /**
   * Send relevant data (e.g. logins and recipes) to the child process (LoginManagerContent).
   */
  sendLoginDataToChild: Task.async(function*(showMasterPassword, formOrigin, actionOrigin,
                                             requestId, target) {
    let recipes = [];
    if (formOrigin) {
      let formHost;
      try {
        formHost = (new URL(formOrigin)).host;
        let recipeManager = yield this.recipeParentPromise;
        recipes = recipeManager.getRecipesForHost(formHost);
      } catch (ex) {
        // Some schemes e.g. chrome aren't supported by URL
      }
    }

    if (!showMasterPassword && !Services.logins.isLoggedIn) {
      try {
        target.sendAsyncMessage("RemoteLogins:loginsFound", {
          requestId: requestId,
          logins: [],
          recipes,
        });
      } catch (e) {
        log("error sending message to target", e);
      }
      return;
    }

    let allLoginsCount = Services.logins.countLogins(formOrigin, "", null);
    // If there are no logins for this site, bail out now.
    if (!allLoginsCount) {
      try {
        target.sendAsyncMessage("RemoteLogins:loginsFound", {
          requestId: requestId,
          logins: [],
          recipes,
        });
      } catch (e) {
        log("error sending message to target", e);
      }
      return;
    }

    // If we're currently displaying a master password prompt, defer
    // processing this form until the user handles the prompt.
    if (Services.logins.uiBusy) {
      log("deferring sendLoginDataToChild for", formOrigin);
      let self = this;
      let observer = {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                               Ci.nsISupportsWeakReference]),

        observe: function (subject, topic, data) {
          log("Got deferred sendLoginDataToChild notification:", topic);
          // Only run observer once.
          Services.obs.removeObserver(this, "passwordmgr-crypto-login");
          Services.obs.removeObserver(this, "passwordmgr-crypto-loginCanceled");
          if (topic == "passwordmgr-crypto-loginCanceled") {
            target.sendAsyncMessage("RemoteLogins:loginsFound", {
              requestId: requestId,
              logins: [],
              recipes,
            });
            return;
          }

          self.sendLoginDataToChild(showMasterPassword, formOrigin, actionOrigin,
                                    requestId, target);
        },
      };

      // Possible leak: it's possible that neither of these notifications
      // will fire, and if that happens, we'll leak the observer (and
      // never return). We should guarantee that at least one of these
      // will fire.
      // See bug XXX.
      Services.obs.addObserver(observer, "passwordmgr-crypto-login", false);
      Services.obs.addObserver(observer, "passwordmgr-crypto-loginCanceled", false);
      return;
    }

    var logins = Services.logins.findLogins({}, formOrigin, actionOrigin, null);
    // Convert the array of nsILoginInfo to vanilla JS objects since nsILoginInfo
    // doesn't support structured cloning.
    var jsLogins = JSON.parse(JSON.stringify(logins));
    target.sendAsyncMessage("RemoteLogins:loginsFound", {
      requestId: requestId,
      logins: jsLogins,
      recipes,
    });

    const PWMGR_FORM_ACTION_EFFECT =  Services.telemetry.getHistogramById("PWMGR_FORM_ACTION_EFFECT");
    if (logins.length == 0) {
      PWMGR_FORM_ACTION_EFFECT.add(2);
    } else if (logins.length == allLoginsCount) {
      PWMGR_FORM_ACTION_EFFECT.add(0);
    } else {
      // logins.length < allLoginsCount
      PWMGR_FORM_ACTION_EFFECT.add(1);
    }
  }),

  doAutocompleteSearch: function({ formOrigin, actionOrigin,
                                   searchString, previousResult,
                                   rect, requestId, remote }, target) {
    // Note: previousResult is a regular object, not an
    // nsIAutoCompleteResult.
    var result;

    let searchStringLower = searchString.toLowerCase();
    let logins;
    if (previousResult &&
        searchStringLower.startsWith(previousResult.searchString.toLowerCase())) {
      log("Using previous autocomplete result");

      // We have a list of results for a shorter search string, so just
      // filter them further based on the new search string.
      logins = previousResult.logins;
    } else {
      log("Creating new autocomplete search result.");

      // Grab the logins from the database.
      logins = Services.logins.findLogins({}, formOrigin, actionOrigin, null);
    }

    let matchingLogins = logins.filter(function(fullMatch) {
      let match = fullMatch.username;

      // Remove results that are too short, or have different prefix.
      // Also don't offer empty usernames as possible results.
      return match && match.toLowerCase().startsWith(searchStringLower);
    });

    // XXX In the E10S case, we're responsible for showing our own
    // autocomplete popup here because the autocomplete protocol hasn't
    // been e10s-ized yet. In the non-e10s case, our caller is responsible
    // for showing the autocomplete popup (via the regular
    // nsAutoCompleteController).
    if (remote) {
      result = new UserAutoCompleteResult(searchString, matchingLogins);
      AutoCompleteE10S.showPopupWithResults(target.ownerDocument.defaultView, rect, result);
    }

    // Convert the array of nsILoginInfo to vanilla JS objects since nsILoginInfo
    // doesn't support structured cloning.
    var jsLogins = JSON.parse(JSON.stringify(matchingLogins));
    target.messageManager.sendAsyncMessage("RemoteLogins:loginsAutoCompleted", {
      requestId: requestId,
      logins: jsLogins,
    });
  },

  onFormSubmit: function(hostname, formSubmitURL,
                         usernameField, newPasswordField,
                         oldPasswordField, opener,
                         target) {
    function getPrompter() {
      var prompterSvc = Cc["@mozilla.org/login-manager/prompter;1"].
                        createInstance(Ci.nsILoginManagerPrompter);
      // XXX For E10S, we don't want to use the browser's contentWindow
      // because it's in another process, so we use our chrome window as
      // the window parent (the content process is responsible for
      // making sure that its window is not in private browsing mode).
      // In the same-process case, we can simply use the content window.
      prompterSvc.init(target.isRemoteBrowser ?
                          target.ownerDocument.defaultView :
                          target.contentWindow);
      if (target.isRemoteBrowser)
        prompterSvc.setE10sData(target, opener);
      return prompterSvc;
    }

    function recordLoginUse(login) {
      // Update the lastUsed timestamp and increment the use count.
      let propBag = Cc["@mozilla.org/hash-property-bag;1"].
                    createInstance(Ci.nsIWritablePropertyBag);
      propBag.setProperty("timeLastUsed", Date.now());
      propBag.setProperty("timesUsedIncrement", 1);
      Services.logins.modifyLogin(login, propBag);
    }

    if (!Services.logins.getLoginSavingEnabled(hostname)) {
      log("(form submission ignored -- saving is disabled for:", hostname, ")");
      return;
    }

    var formLogin = Cc["@mozilla.org/login-manager/loginInfo;1"].
                    createInstance(Ci.nsILoginInfo);
    formLogin.init(hostname, formSubmitURL, null,
                   (usernameField ? usernameField.value : ""),
                   newPasswordField.value,
                   (usernameField ? usernameField.name  : ""),
                   newPasswordField.name);

    let logins = Services.logins.findLogins({}, hostname, formSubmitURL, null);

    // If we didn't find a username field, but seem to be changing a
    // password, allow the user to select from a list of applicable
    // logins to update the password for.
    if (!usernameField && oldPasswordField && logins.length > 0) {
      var prompter = getPrompter();

      if (logins.length == 1) {
        var oldLogin = logins[0];

        if (oldLogin.password == formLogin.password) {
          recordLoginUse(oldLogin);
          log("(Not prompting to save/change since we have no username and the " +
              "only saved password matches the new password)");
          return;
        }

        formLogin.username      = oldLogin.username;
        formLogin.usernameField = oldLogin.usernameField;

        prompter.promptToChangePassword(oldLogin, formLogin);
      } else {
        prompter.promptToChangePasswordWithUsernames(
                            logins, logins.length, formLogin);
      }

      return;
    }


    var existingLogin = null;
    // Look for an existing login that matches the form login.
    for (var i = 0; i < logins.length; i++) {
      var same, login = logins[i];

      // If one login has a username but the other doesn't, ignore
      // the username when comparing and only match if they have the
      // same password. Otherwise, compare the logins and match even
      // if the passwords differ.
      if (!login.username && formLogin.username) {
        var restoreMe = formLogin.username;
        formLogin.username = "";
        same = formLogin.matches(login, false);
        formLogin.username = restoreMe;
      } else if (!formLogin.username && login.username) {
        formLogin.username = login.username;
        same = formLogin.matches(login, false);
        formLogin.username = ""; // we know it's always blank.
      } else {
        same = formLogin.matches(login, true);
      }

      if (same) {
        existingLogin = login;
        break;
      }
    }

    if (existingLogin) {
      log("Found an existing login matching this form submission");

      // Change password if needed.
      if (existingLogin.password != formLogin.password) {
        log("...passwords differ, prompting to change.");
        prompter = getPrompter();
        prompter.promptToChangePassword(existingLogin, formLogin);
      } else {
        recordLoginUse(existingLogin);
      }

      return;
    }


    // Prompt user to save login (via dialog or notification bar)
    prompter = getPrompter();
    prompter.promptToSavePassword(formLogin);
  },

  /**
   * Maps all the <browser> elements for tabs in the parent process to the
   * current state used to display tab-specific UI.
   *
   * This mapping is not updated in case a web page is moved to a different
   * chrome window by the swapDocShells method. In this case, it is possible
   * that a UI update just requested for the login fill doorhanger and then
   * delayed by a few hundred milliseconds will be lost. Later requests would
   * use the new browser reference instead.
   *
   * Given that the case above is rare, and it would not cause any origin
   * mismatch at the time of filling because the origin is checked later in the
   * content process, this case is left unhandled.
   */
  loginFormStateByBrowser: new WeakMap(),

  /**
   * Retrieves a reference to the state object associated with the given
   * browser. This is initialized to an empty object.
   */
  stateForBrowser(browser) {
    let loginFormState = this.loginFormStateByBrowser.get(browser);
    if (!loginFormState) {
      loginFormState = {};
      this.loginFormStateByBrowser.set(browser, loginFormState);
    }
    return loginFormState;
  },

  /**
   * Called to indicate whether a login form on the currently loaded page is
   * present or not. This is one of the factors used to control the visibility
   * of the password fill doorhanger.
   */
  updateLoginFormPresence(browser, { loginFormOrigin, loginFormPresent }) {
    const ANCHOR_DELAY_MS = 200;

    let state = this.stateForBrowser(browser);

    // Update the data to use to the latest known values. Since messages are
    // processed in order, this will always be the latest version to use.
    state.loginFormOrigin = loginFormOrigin;
    state.loginFormPresent = loginFormPresent;

    // Apply the data to the currently displayed icon later.
    if (!state.anchorDeferredTask) {
      state.anchorDeferredTask = new DeferredTask(
        () => this.updateLoginAnchor(browser),
        ANCHOR_DELAY_MS
      );
    }
    state.anchorDeferredTask.arm();
  },
  updateLoginAnchor: Task.async(function* (browser) {
    // Copy the state to use for this execution of the task. These will not
    // change during this execution of the asynchronous function, but in case a
    // change happens in the state, the function will be retriggered.
    let { loginFormOrigin, loginFormPresent } = this.stateForBrowser(browser);

    yield Services.logins.initializationPromise;

    // Check if there are form logins for the site, ignoring formSubmitURL.
    let hasLogins = loginFormOrigin &&
                    Services.logins.countLogins(loginFormOrigin, "", null) > 0;

    // Once this preference is removed, this version of the fill doorhanger
    // should be enabled for Desktop only, and not for Android or B2G.
    if (!Services.prefs.getBoolPref("signon.ui.experimental")) {
      return;
    }

    let showLoginAnchor = loginFormPresent || hasLogins;

    let fillDoorhanger = LoginDoorhangers.FillDoorhanger.find({ browser });
    if (fillDoorhanger) {
      if (!showLoginAnchor) {
        fillDoorhanger.remove();
        return;
      }
      // We should only update the state of the doorhanger while it is hidden.
      yield fillDoorhanger.promiseHidden;
      fillDoorhanger.loginFormPresent = loginFormPresent;
      fillDoorhanger.loginFormOrigin = loginFormOrigin;
      fillDoorhanger.filterString = hasLogins ? loginFormOrigin : "";
      fillDoorhanger.detailLogin = null;
      fillDoorhanger.autoDetailLogin = true;
      return;
    }
    if (showLoginAnchor) {
      fillDoorhanger = new LoginDoorhangers.FillDoorhanger({
        browser,
        loginFormPresent,
        loginFormOrigin,
        filterString: hasLogins ? loginFormOrigin : "",
        autoDetailLogin: true,
      });
    }
  }),
};
