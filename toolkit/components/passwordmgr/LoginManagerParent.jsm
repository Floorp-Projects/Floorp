/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

ChromeUtils.defineModuleGetter(this, "AutoCompletePopup",
                               "resource://gre/modules/AutoCompletePopup.jsm");
ChromeUtils.defineModuleGetter(this, "DeferredTask",
                               "resource://gre/modules/DeferredTask.jsm");
ChromeUtils.defineModuleGetter(this, "LoginHelper",
                               "resource://gre/modules/LoginHelper.jsm");
ChromeUtils.defineModuleGetter(this, "PrivateBrowsingUtils",
                               "resource://gre/modules/PrivateBrowsingUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let logger = LoginHelper.createLogger("LoginManagerParent");
  return logger.log.bind(logger);
});

var EXPORTED_SYMBOLS = [ "LoginManagerParent" ];

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

  // Tracks the last time the user cancelled the master password prompt,
  // to avoid spamming master password prompts on autocomplete searches.
  _lastMPLoginCancelled: Math.NEGATIVE_INFINITY,

  _searchAndDedupeLogins(formOrigin, actionOrigin, {looseActionOriginMatch} = {}) {
    let logins;
    let matchData = {
      hostname: formOrigin,
      schemeUpgrades: LoginHelper.schemeUpgrades,
    };
    if (!looseActionOriginMatch) {
      matchData.formSubmitURL = actionOrigin;
    }
    try {
      logins = LoginHelper.searchLoginsWithObject(matchData);
    } catch (e) {
      // Record the last time the user cancelled the MP prompt
      // to avoid spamming them with MP prompts for autocomplete.
      if (e.result == Cr.NS_ERROR_ABORT) {
        log("User cancelled master password prompt.");
        this._lastMPLoginCancelled = Date.now();
        return [];
      }
      throw e;
    }

    // Dedupe so the length checks below still make sense with scheme upgrades.
    let resolveBy = [
      "actionOrigin",
      "scheme",
      "timePasswordChanged",
    ];
    return LoginHelper.dedupeLogins(logins, ["username"], resolveBy, formOrigin, actionOrigin);
  },

  // Listeners are added in BrowserGlue.jsm on desktop
  // and in BrowserCLH.js on mobile.
  receiveMessage(msg) {
    let data = msg.data;
    switch (msg.name) {
      case "PasswordManager:findLogins": {
        // TODO Verify msg.target's principals against the formOrigin?
        this.sendLoginDataToChild(data.options.showMasterPassword,
                                  data.formOrigin,
                                  data.actionOrigin,
                                  data.requestId,
                                  msg.target.messageManager);
        break;
      }

      case "PasswordManager:findRecipes": {
        let formHost = (new URL(data.formOrigin)).host;
        return this._recipeManager.getRecipesForHost(formHost);
      }

      case "PasswordManager:onFormSubmit": {
        // TODO Verify msg.target's principals against the formOrigin?
        this.onFormSubmit({hostname: data.hostname,
                           formSubmitURL: data.formSubmitURL,
                           autoFilledLoginGuid: data.autoFilledLoginGuid,
                           usernameField: data.usernameField,
                           newPasswordField: data.newPasswordField,
                           oldPasswordField: data.oldPasswordField,
                           openerTopWindowID: data.openerTopWindowID,
                           dismissedPrompt: data.dismissedPrompt,
                           target: msg.target});
        break;
      }

      case "PasswordManager:insecureLoginFormPresent": {
        this.setHasInsecureLoginForms(msg.target, data.hasInsecureLoginForms);
        break;
      }

      case "PasswordManager:autoCompleteLogins": {
        this.doAutocompleteSearch(data, msg.target);
        break;
      }

      case "PasswordManager:removeLogin": {
        let login = LoginHelper.vanillaObjectToLogin(data.login);
        AutoCompletePopup.removeLogin(login);
        break;
      }

      case "PasswordManager:OpenPreferences": {
        LoginHelper.openPasswordManager(msg.target.ownerGlobal, {
          filterString: msg.data.hostname,
          entryPoint: msg.data.entryPoint,
        });
        break;
      }
    }

    return undefined;
  },

  /**
   * Trigger a login form fill and send relevant data (e.g. logins and recipes)
   * to the child process (LoginManagerContent).
   */
  async fillForm({ browser, loginFormOrigin, login, inputElement }) {
    let recipes = [];
    if (loginFormOrigin) {
      let formHost;
      try {
        formHost = (new URL(loginFormOrigin)).host;
        let recipeManager = await this.recipeParentPromise;
        recipes = recipeManager.getRecipesForHost(formHost);
      } catch (ex) {
        // Some schemes e.g. chrome aren't supported by URL
      }
    }

    // Convert the array of nsILoginInfo to vanilla JS objects since nsILoginInfo
    // doesn't support structured cloning.
    let jsLogins = [LoginHelper.loginToVanillaObject(login)];

    let objects = inputElement ? {inputElement} : null;
    browser.messageManager.sendAsyncMessage("PasswordManager:fillForm", {
      loginFormOrigin,
      logins: jsLogins,
      recipes,
    }, objects);
  },

  /**
   * Send relevant data (e.g. logins and recipes) to the child process (LoginManagerContent).
   */
  async sendLoginDataToChild(showMasterPassword, formOrigin, actionOrigin,
                             requestId, target) {
    let recipes = [];
    if (formOrigin) {
      let formHost;
      try {
        formHost = (new URL(formOrigin)).host;
        let recipeManager = await this.recipeParentPromise;
        recipes = recipeManager.getRecipesForHost(formHost);
      } catch (ex) {
        // Some schemes e.g. chrome aren't supported by URL
      }
    }

    if (!showMasterPassword && !Services.logins.isLoggedIn) {
      try {
        target.sendAsyncMessage("PasswordManager:loginsFound", {
          requestId,
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
        QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver,
                                                Ci.nsISupportsWeakReference]),

        observe(subject, topic, data) {
          log("Got deferred sendLoginDataToChild notification:", topic);
          // Only run observer once.
          Services.obs.removeObserver(this, "passwordmgr-crypto-login");
          Services.obs.removeObserver(this, "passwordmgr-crypto-loginCanceled");
          if (topic == "passwordmgr-crypto-loginCanceled") {
            target.sendAsyncMessage("PasswordManager:loginsFound", {
              requestId,
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
      Services.obs.addObserver(observer, "passwordmgr-crypto-login");
      Services.obs.addObserver(observer, "passwordmgr-crypto-loginCanceled");
      return;
    }

    // Autocomplete results do not need to match actionOrigin.
    let logins = this._searchAndDedupeLogins(formOrigin, actionOrigin, {looseActionOriginMatch: true});

    log("sendLoginDataToChild:", logins.length, "deduped logins");
    // Convert the array of nsILoginInfo to vanilla JS objects since nsILoginInfo
    // doesn't support structured cloning.
    var jsLogins = LoginHelper.loginsToVanillaObjects(logins);
    target.sendAsyncMessage("PasswordManager:loginsFound", {
      requestId,
      logins: jsLogins,
      recipes,
    });
  },

  doAutocompleteSearch({ formOrigin, actionOrigin,
                         searchString, previousResult,
                         rect, requestId, isSecure, isPasswordField,
  }, target) {
    // Note: previousResult is a regular object, not an
    // nsIAutoCompleteResult.

    // Cancel if we unsuccessfully prompted for the master password too recently.
    if (!Services.logins.isLoggedIn) {
      let timeDiff = Date.now() - this._lastMPLoginCancelled;
      if (timeDiff < this._repromptTimeout) {
        log("Not searching logins for autocomplete since the master password " +
            `prompt was last cancelled ${Math.round(timeDiff / 1000)} seconds ago.`);
        // Send an empty array to make LoginManagerContent clear the
        // outstanding request it has temporarily saved.
        target.messageManager.sendAsyncMessage("PasswordManager:loginsAutoCompleted", {
          requestId,
          logins: [],
        });
        return;
      }
    }

    let searchStringLower = searchString.toLowerCase();
    let logins;
    if (previousResult &&
        searchStringLower.startsWith(previousResult.searchString.toLowerCase())) {
      log("Using previous autocomplete result");

      // We have a list of results for a shorter search string, so just
      // filter them further based on the new search string.
      logins = LoginHelper.vanillaObjectsToLogins(previousResult.logins);
    } else {
      log("Creating new autocomplete search result.");

      // Autocomplete results do not need to match actionOrigin.
      logins = this._searchAndDedupeLogins(formOrigin, actionOrigin, {looseActionOriginMatch: true});
    }

    let matchingLogins = logins.filter(function(fullMatch) {
      let match = fullMatch.username;

      // Remove results that are too short, or have different prefix.
      // Also don't offer empty usernames as possible results except
      // for password field.
      if (isPasswordField) {
        return true;
      }
      return match && match.toLowerCase().startsWith(searchStringLower);
    });

    // Convert the array of nsILoginInfo to vanilla JS objects since nsILoginInfo
    // doesn't support structured cloning.
    var jsLogins = LoginHelper.loginsToVanillaObjects(matchingLogins);
    target.messageManager.sendAsyncMessage("PasswordManager:loginsAutoCompleted", {
      requestId,
      logins: jsLogins,
    });
  },

  onFormSubmit({hostname, formSubmitURL, autoFilledLoginGuid,
                usernameField, newPasswordField,
                oldPasswordField, openerTopWindowID,
                dismissedPrompt, target}) {
    function getPrompter() {
      var prompterSvc = Cc["@mozilla.org/login-manager/prompter;1"].
                        createInstance(Ci.nsILoginManagerPrompter);
      prompterSvc.init(target.ownerGlobal);
      prompterSvc.browser = target;

      for (let win of Services.wm.getEnumerator(null)) {
        let tabbrowser = win.gBrowser;
        if (tabbrowser) {
          let browser = tabbrowser.getBrowserForOuterWindowID(openerTopWindowID);
          if (browser) {
            prompterSvc.openerBrowser = browser;
            break;
          }
        }
      }

      return prompterSvc;
    }

    function recordLoginUse(login) {
      if (!target || PrivateBrowsingUtils.isBrowserPrivate(target)) {
        // don't record non-interactive use in private browsing
        return;
      }
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
                   (usernameField ? usernameField.name : ""),
                   newPasswordField.name);

    if (autoFilledLoginGuid) {
      let loginsForGuid = LoginHelper.searchLoginsWithObject({
        guid: autoFilledLoginGuid,
      });
      if (loginsForGuid.length == 1 &&
          loginsForGuid[0].password == formLogin.password &&
          (!formLogin.username || // Also cover cases where only the password is requested.
           loginsForGuid[0].username == formLogin.username)) {
        log("The filled login matches the form submission. Nothing to change.");
        recordLoginUse(loginsForGuid[0]);
        return;
      }
    }

    // Below here we have one login per hostPort + action + username with the
    // matching scheme being preferred.
    let logins = this._searchAndDedupeLogins(hostname, formSubmitURL);

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

        prompter.promptToChangePassword(oldLogin, formLogin, dismissedPrompt);
      } else {
        // Note: It's possible that that we already have the correct u+p saved
        // but since we don't have the username, we don't know if the user is
        // changing a second account to the new password so we ask anyways.

        prompter.promptToChangePasswordWithUsernames(logins, formLogin);
      }

      return;
    }


    var existingLogin = null;
    // Look for an existing login that matches the form login.
    for (let login of logins) {
      let same;

      // If one login has a username but the other doesn't, ignore
      // the username when comparing and only match if they have the
      // same password. Otherwise, compare the logins and match even
      // if the passwords differ.
      if (!login.username && formLogin.username) {
        var restoreMe = formLogin.username;
        formLogin.username = "";
        same = LoginHelper.doLoginsMatch(formLogin, login, {
          ignorePassword: false,
          ignoreSchemes: LoginHelper.schemeUpgrades,
        });
        formLogin.username = restoreMe;
      } else if (!formLogin.username && login.username) {
        formLogin.username = login.username;
        same = LoginHelper.doLoginsMatch(formLogin, login, {
          ignorePassword: false,
          ignoreSchemes: LoginHelper.schemeUpgrades,
        });
        formLogin.username = ""; // we know it's always blank.
      } else {
        same = LoginHelper.doLoginsMatch(formLogin, login, {
          ignorePassword: true,
          ignoreSchemes: LoginHelper.schemeUpgrades,
        });
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
        prompter.promptToChangePassword(existingLogin, formLogin, dismissedPrompt);
      } else if (!existingLogin.username && formLogin.username) {
        log("...empty username update, prompting to change.");
        prompter = getPrompter();
        prompter.promptToChangePassword(existingLogin, formLogin, dismissedPrompt);
      } else {
        recordLoginUse(existingLogin);
      }

      return;
    }

    // Prompt user to save login (via dialog or notification bar)
    prompter = getPrompter();
    prompter.promptToSavePassword(formLogin, dismissedPrompt);
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
   * Returns true if the page currently loaded in the given browser element has
   * insecure login forms. This state may be updated asynchronously, in which
   * case a custom event named InsecureLoginFormsStateChange will be dispatched
   * on the browser element.
   */
  hasInsecureLoginForms(browser) {
    return !!this.stateForBrowser(browser).hasInsecureLoginForms;
  },

  /**
   * Called to indicate whether an insecure password field is present so
   * insecure password UI can know when to show.
   */
  setHasInsecureLoginForms(browser, hasInsecureLoginForms) {
    let state = this.stateForBrowser(browser);

    // Update the data to use to the latest known values. Since messages are
    // processed in order, this will always be the latest version to use.
    state.hasInsecureLoginForms = hasInsecureLoginForms;

    // Report the insecure login form state immediately.
    browser.dispatchEvent(new browser.ownerGlobal
                                 .CustomEvent("InsecureLoginFormsStateChange"));
  },
};

XPCOMUtils.defineLazyGetter(LoginManagerParent, "recipeParentPromise", function() {
  const { LoginRecipesParent } = ChromeUtils.import("resource://gre/modules/LoginRecipes.jsm");
  this._recipeManager = new LoginRecipesParent({
    defaults: Services.prefs.getStringPref("signon.recipes.path"),
  });
  return this._recipeManager.initializationPromise;
});

XPCOMUtils.defineLazyPreferenceGetter(LoginManagerParent, "_repromptTimeout",
                                      "signon.masterPasswordReprompt.timeout_ms", 900000); // 15 Minutes
