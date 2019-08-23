/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const LoginInfo = new Components.Constructor(
  "@mozilla.org/login-manager/loginInfo;1",
  Ci.nsILoginInfo,
  "init"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

ChromeUtils.defineModuleGetter(
  this,
  "AutoCompletePopup",
  "resource://gre/modules/AutoCompletePopup.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "DeferredTask",
  "resource://gre/modules/DeferredTask.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "LoginHelper",
  "resource://gre/modules/LoginHelper.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PasswordGenerator",
  "resource://gre/modules/PasswordGenerator.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let logger = LoginHelper.createLogger("LoginManagerParent");
  return logger.log.bind(logger);
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "INCLUDE_OTHER_SUBDOMAINS_IN_LOOKUP",
  "signon.includeOtherSubdomainsInLookup",
  false
);

const EXPORTED_SYMBOLS = ["LoginManagerParent"];

this.LoginManagerParent = {
  /**
   * A map of a principal's origin (including suffixes) to a generated password string and filled flag
   * so that we can offer the same password later (e.g. in a confirmation field).
   *
   * We don't currently evict from this cache so entries should last until the end of the browser
   * session. That may change later but for now a typical session would max out at a few entries.
   */
  _generatedPasswordsByPrincipalOrigin: new Map(),

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

  /**
   * @param {origin} formOrigin
   * @param {object} options
   * @param {origin?} options.formActionOrigin To match on. Omit this argument to match all action origins.
   * @param {origin?} options.httpRealm To match on. Omit this argument to match all realms.
   * @param {boolean} options.acceptDifferentSubdomains Include results for eTLD+1 matches
   * @param {boolean} options.ignoreActionAndRealm Include all form and HTTP auth logins for the site
   */
  _searchAndDedupeLogins(
    formOrigin,
    {
      acceptDifferentSubdomains,
      formActionOrigin,
      httpRealm,
      ignoreActionAndRealm,
    } = {}
  ) {
    let logins;
    let matchData = {
      origin: formOrigin,
      schemeUpgrades: LoginHelper.schemeUpgrades,
      acceptDifferentSubdomains,
    };
    if (!ignoreActionAndRealm) {
      if (typeof formActionOrigin != "undefined") {
        matchData.formActionOrigin = formActionOrigin;
      } else if (typeof httpRealm != "undefined") {
        matchData.httpRealm = httpRealm;
      }
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

    logins = LoginHelper.shadowHTTPLogins(logins);

    let resolveBy = [
      "actionOrigin",
      "scheme",
      "subdomain",
      "timePasswordChanged",
    ];
    return LoginHelper.dedupeLogins(
      logins,
      ["username", "password"],
      resolveBy,
      formOrigin,
      formActionOrigin
    );
  },

  // Listeners are added in BrowserGlue.jsm on desktop
  // and in BrowserCLH.js on mobile.
  receiveMessage(msg) {
    let data = msg.data;
    switch (msg.name) {
      case "PasswordManager:findLogins": {
        // TODO Verify msg.target's principals against the formOrigin?
        this.sendLoginDataToChild(
          data.formOrigin,
          data.actionOrigin,
          data.requestId,
          msg.target.messageManager,
          data.options
        );
        break;
      }

      case "PasswordManager:findRecipes": {
        let formHost = new URL(data.formOrigin).host;
        return this._recipeManager.getRecipesForHost(formHost);
      }

      case "PasswordManager:onFormSubmit": {
        // TODO Verify msg.target's principals against the formOrigin?
        this.onFormSubmit(msg.target, data);
        break;
      }

      case "PasswordManager:onGeneratedPasswordFilledOrEdited": {
        this._onGeneratedPasswordFilledOrEdited(data);
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

  // Observers are added in BrowserGlue.jsm on desktop
  observe(subject, topic, data) {
    if (topic == "passwordmgr-autosaved-login-merged") {
      // in the case where an autosaved login is merged into an existing login,
      // remove the generated-password cache entry entirely
      let { origin } = subject;
      if (this._generatedPasswordsByPrincipalOrigin.has(origin)) {
        log("Removing generated-password cache entry for origin:", origin);
        this._generatedPasswordsByPrincipalOrigin.delete(origin);
      }
    } else if (
      topic == "passwordmgr-storage-changed" &&
      data == "removeLogin"
    ) {
      // in the case where an autosaved login is deleted, remove storageGUID for that entry
      let { origin } = subject;
      let generatedPW = this._generatedPasswordsByPrincipalOrigin.get(origin);
      if (generatedPW) {
        log(
          "Removing storageGUID for generated-password cache entry on origin:",
          origin
        );
        generatedPW.storageGUID = null;
      }
    }
  },

  /**
   * Trigger a login form fill and send relevant data (e.g. logins and recipes)
   * to the child process (LoginManagerContent).
   */
  async fillForm({ browser, loginFormOrigin, login, inputElementIdentifier }) {
    let recipes = [];
    if (loginFormOrigin) {
      let formHost;
      try {
        formHost = new URL(loginFormOrigin).host;
        let recipeManager = await this.recipeParentPromise;
        recipes = recipeManager.getRecipesForHost(formHost);
      } catch (ex) {
        // Some schemes e.g. chrome aren't supported by URL
      }
    }

    // Convert the array of nsILoginInfo to vanilla JS objects since nsILoginInfo
    // doesn't support structured cloning.
    let jsLogins = [LoginHelper.loginToVanillaObject(login)];

    browser.messageManager.sendAsyncMessage("PasswordManager:fillForm", {
      inputElementIdentifier,
      loginFormOrigin,
      logins: jsLogins,
      recipes,
    });
  },

  /**
   * Send relevant data (e.g. logins and recipes) to the child process (LoginManagerContent).
   */
  async sendLoginDataToChild(
    formOrigin,
    actionOrigin,
    requestId,
    target,
    { guid, showMasterPassword }
  ) {
    let recipes = [];
    if (formOrigin) {
      let formHost;
      try {
        formHost = new URL(formOrigin).host;
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
        QueryInterface: ChromeUtils.generateQI([
          Ci.nsIObserver,
          Ci.nsISupportsWeakReference,
        ]),

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

          self.sendLoginDataToChild(
            formOrigin,
            actionOrigin,
            requestId,
            target,
            {
              showMasterPassword,
            }
          );
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

    // Autocomplete results do not need to match actionOrigin or exact origin.
    let logins = null;
    if (guid) {
      logins = LoginHelper.searchLoginsWithObject({
        guid,
      });
    } else {
      logins = this._searchAndDedupeLogins(formOrigin, {
        formActionOrigin: actionOrigin,
        ignoreActionAndRealm: true,
        acceptDifferentSubdomains: INCLUDE_OTHER_SUBDOMAINS_IN_LOOKUP, // TODO: for TAB case
      });
    }

    log("sendLoginDataToChild:", logins.length, "deduped logins");
    // Convert the array of nsILoginInfo to vanilla JS objects since nsILoginInfo
    // doesn't support structured cloning.
    let jsLogins = LoginHelper.loginsToVanillaObjects(logins);
    target.sendAsyncMessage("PasswordManager:loginsFound", {
      requestId,
      logins: jsLogins,
      recipes,
    });
  },

  doAutocompleteSearch(
    {
      autocompleteInfo,
      browsingContextId,
      formOrigin,
      actionOrigin,
      searchString,
      previousResult,
      requestId,
      isSecure,
      isPasswordField,
    },
    target
  ) {
    // Note: previousResult is a regular object, not an
    // nsIAutoCompleteResult.

    // Cancel if we unsuccessfully prompted for the master password too recently.
    if (!Services.logins.isLoggedIn) {
      let timeDiff = Date.now() - this._lastMPLoginCancelled;
      if (timeDiff < this._repromptTimeout) {
        log(
          "Not searching logins for autocomplete since the master password " +
            `prompt was last cancelled ${Math.round(
              timeDiff / 1000
            )} seconds ago.`
        );
        // Send an empty array to make LoginManagerContent clear the
        // outstanding request it has temporarily saved.
        target.messageManager.sendAsyncMessage(
          "PasswordManager:loginsAutoCompleted",
          {
            requestId,
            logins: [],
          }
        );
        return;
      }
    }

    let searchStringLower = searchString.toLowerCase();
    let logins;
    if (
      previousResult &&
      searchStringLower.startsWith(previousResult.searchString.toLowerCase())
    ) {
      log("Using previous autocomplete result");

      // We have a list of results for a shorter search string, so just
      // filter them further based on the new search string.
      logins = LoginHelper.vanillaObjectsToLogins(previousResult.logins);
    } else {
      log("Creating new autocomplete search result.");

      // Autocomplete results do not need to match actionOrigin or exact origin.
      logins = this._searchAndDedupeLogins(formOrigin, {
        formActionOrigin: actionOrigin,
        ignoreActionAndRealm: true,
        acceptDifferentSubdomains: INCLUDE_OTHER_SUBDOMAINS_IN_LOOKUP,
      });
    }

    let matchingLogins = logins.filter(function(fullMatch) {
      let match = fullMatch.username;

      // Remove results that are too short, or have different prefix.
      // Also don't offer empty usernames as possible results except
      // for on password fields.
      if (isPasswordField) {
        return true;
      }
      return match && match.toLowerCase().startsWith(searchStringLower);
    });

    let generatedPassword = null;
    if (
      isPasswordField &&
      autocompleteInfo.fieldName == "new-password" &&
      Services.logins.getLoginSavingEnabled(formOrigin) &&
      !PrivateBrowsingUtils.isWindowPrivate(target.ownerGlobal)
    ) {
      generatedPassword = this.getGeneratedPassword(browsingContextId);
    }

    // Convert the array of nsILoginInfo to vanilla JS objects since nsILoginInfo
    // doesn't support structured cloning.
    let jsLogins = LoginHelper.loginsToVanillaObjects(matchingLogins);
    target.messageManager.sendAsyncMessage(
      "PasswordManager:loginsAutoCompleted",
      {
        requestId,
        generatedPassword,
        logins: jsLogins,
      }
    );
  },

  /**
   * Expose `BrowsingContext` so we can stub it in tests.
   */
  get _browsingContextGlobal() {
    return BrowsingContext;
  },

  getGeneratedPassword(browsingContextId) {
    if (
      !LoginHelper.enabled ||
      !LoginHelper.generationAvailable ||
      !LoginHelper.generationEnabled
    ) {
      return null;
    }

    let browsingContext = BrowsingContext.get(browsingContextId);
    if (!browsingContext) {
      return null;
    }
    let framePrincipalOrigin =
      browsingContext.currentWindowGlobal.documentPrincipal.origin;
    // Use the same password if we already generated one for this origin so that it doesn't change
    // with each search/keystroke and the user can easily re-enter a password in a confirmation field.
    let generatedPW = this._generatedPasswordsByPrincipalOrigin.get(
      framePrincipalOrigin
    );
    if (generatedPW) {
      return generatedPW.value;
    }

    generatedPW = {
      edited: false,
      filled: false,
      /**
       * GUID of a login that was already saved for this generated password that
       * will be automatically updated with password changes. This shouldn't be
       * an existing saved login for the site unless the user chose to
       * merge/overwrite via a doorhanger.
       */
      storageGUID: null,
      value: PasswordGenerator.generatePassword(),
    };
    this._generatedPasswordsByPrincipalOrigin.set(
      framePrincipalOrigin,
      generatedPW
    );
    return generatedPW.value;
  },

  _getPrompter(browser, openerTopWindowID) {
    let prompterSvc = Cc[
      "@mozilla.org/login-manager/prompter;1"
    ].createInstance(Ci.nsILoginManagerPrompter);
    prompterSvc.init(browser.ownerGlobal);
    prompterSvc.browser = browser;

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
  },

  onFormSubmit(
    browser,
    {
      origin,
      formActionOrigin,
      autoFilledLoginGuid,
      usernameField,
      newPasswordField,
      oldPasswordField,
      openerTopWindowID,
      dismissedPrompt,
    }
  ) {
    function recordLoginUse(login) {
      if (!browser || PrivateBrowsingUtils.isBrowserPrivate(browser)) {
        // don't record non-interactive use in private browsing
        return;
      }
      // Update the lastUsed timestamp and increment the use count.
      let propBag = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
        Ci.nsIWritablePropertyBag
      );
      propBag.setProperty("timeLastUsed", Date.now());
      propBag.setProperty("timesUsedIncrement", 1);
      Services.logins.modifyLogin(login, propBag);
    }

    if (!Services.logins.getLoginSavingEnabled(origin)) {
      log("(form submission ignored -- saving is disabled for:", origin, ")");
      return;
    }

    let formLogin = new LoginInfo(
      origin,
      formActionOrigin,
      null,
      usernameField ? usernameField.value : "",
      newPasswordField.value,
      usernameField ? usernameField.name : "",
      newPasswordField.name
    );

    if (autoFilledLoginGuid) {
      let loginsForGuid = LoginHelper.searchLoginsWithObject({
        guid: autoFilledLoginGuid,
      });
      if (
        loginsForGuid.length == 1 &&
        loginsForGuid[0].password == formLogin.password &&
        (!formLogin.username || // Also cover cases where only the password is requested.
          loginsForGuid[0].username == formLogin.username)
      ) {
        log("The filled login matches the form submission. Nothing to change.");
        recordLoginUse(loginsForGuid[0]);
        return;
      }
    }

    // Below here we have one login per hostPort + action + username with the
    // matching scheme being preferred.
    let logins = this._searchAndDedupeLogins(origin, {
      formActionOrigin,
    });

    let { browsingContext } = browser;
    let framePrincipalOrigin =
      browsingContext.currentWindowGlobal.documentPrincipal.origin;
    let generatedPW = this._generatedPasswordsByPrincipalOrigin.get(
      framePrincipalOrigin
    );

    // If we didn't find a username field, but seem to be changing a
    // password, allow the user to select from a list of applicable
    // logins to update the password for.
    if (!usernameField && oldPasswordField && logins.length > 0) {
      let prompter = this._getPrompter(browser, openerTopWindowID);

      if (logins.length == 1) {
        let oldLogin = logins[0];

        if (oldLogin.password == formLogin.password) {
          recordLoginUse(oldLogin);
          log(
            "(Not prompting to save/change since we have no username and the " +
              "only saved password matches the new password)"
          );
          return;
        }

        let autoSavedStorageGUID = "";
        if (
          generatedPW &&
          generatedPW.storageGUID == oldLogin.guid &&
          generatedPW.value == formLogin.password
        ) {
          // this login has a generated password, auto-saved in this session
          autoSavedStorageGUID = generatedPW.storageGUID;
        }
        formLogin.username = oldLogin.username;
        formLogin.usernameField = oldLogin.usernameField;

        prompter.promptToChangePassword(
          oldLogin,
          formLogin,
          dismissedPrompt,
          false, // notifySaved
          autoSavedStorageGUID
        );
        return;
      } else if (!generatedPW || generatedPW.value != newPasswordField.value) {
        // Note: It's possible that that we already have the correct u+p saved
        // but since we don't have the username, we don't know if the user is
        // changing a second account to the new password so we ask anyways.
        prompter.promptToChangePasswordWithUsernames(logins, formLogin);
        return;
      }
    }

    let existingLogin = null;
    // Look for an existing login that matches the form login.
    for (let login of logins) {
      let same;

      // If one login has a username but the other doesn't, ignore
      // the username when comparing and only match if they have the
      // same password. Otherwise, compare the logins and match even
      // if the passwords differ.
      if (!login.username && formLogin.username) {
        let restoreMe = formLogin.username;
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
      let autoSavedStorageGUID = "";
      if (
        generatedPW &&
        generatedPW.storageGUID == existingLogin.guid &&
        generatedPW.value == formLogin.password
      ) {
        // this login has a generated password, auto-saved in this session
        autoSavedStorageGUID = generatedPW.storageGUID;
      }

      // Change password if needed.
      if (existingLogin.password != formLogin.password) {
        log("...passwords differ, prompting to change.");
        let prompter = this._getPrompter(browser, openerTopWindowID);
        prompter.promptToChangePassword(
          existingLogin,
          formLogin,
          dismissedPrompt,
          false, // notifySaved
          autoSavedStorageGUID
        );
      } else if (!existingLogin.username && formLogin.username) {
        log("...empty username update, prompting to change.");
        let prompter = this._getPrompter(browser, openerTopWindowID);
        prompter.promptToChangePassword(
          existingLogin,
          formLogin,
          dismissedPrompt,
          false, // notifySaved
          autoSavedStorageGUID
        );
      } else {
        recordLoginUse(existingLogin);
      }

      return;
    }

    // Prompt user to save login (via dialog or notification bar)
    let prompter = this._getPrompter(browser, openerTopWindowID);
    prompter.promptToSavePassword(formLogin, dismissedPrompt);
  },

  _onGeneratedPasswordFilledOrEdited({
    browsingContextId,
    formActionOrigin,
    openerTopWindowID,
    password,
    username = "",
  }) {
    log("_onGeneratedPasswordFilledOrEdited");

    if (!password) {
      log("_onGeneratedPasswordFilledOrEdited: The password field is empty");
      return;
    }

    let browsingContext = BrowsingContext.get(browsingContextId);
    let {
      originNoSuffix,
    } = browsingContext.currentWindowGlobal.documentPrincipal;
    let formOrigin = LoginHelper.getLoginOrigin(originNoSuffix);
    if (!formOrigin) {
      log(
        "_onGeneratedPasswordFilledOrEdited: Invalid form origin:",
        browsingContext.currentWindowGlobal.documentPrincipal
      );
      return;
    }

    if (!Services.logins.getLoginSavingEnabled(formOrigin)) {
      // No UI should be shown to offer generation in thie case but a user may
      // disable saving for the site after already filling one and they may then
      // edit it.
      log(
        "_onGeneratedPasswordFilledOrEdited: saving is disabled for:",
        formOrigin
      );
      return;
    }

    let framePrincipalOrigin =
      browsingContext.currentWindowGlobal.documentPrincipal.origin;
    let generatedPW = this._generatedPasswordsByPrincipalOrigin.get(
      framePrincipalOrigin
    );

    let autoSaveLogin = true;
    let loginToChange = null;

    if (password != generatedPW.value) {
      // The user edited the field after generation to a non-empty value.
      log("The field containing the generated password has changed");

      // Record telemetry for the first edit
      if (!generatedPW.edited) {
        Services.telemetry.recordEvent(
          "pwmgr",
          "filled_field_edited",
          "generatedpassword"
        );
        log("filled_field_edited telemetry event recorded");
        generatedPW.edited = true;
      }

      // The edit was to a login that was auto-saved.
      // Note that it could have been saved in a totally different tab in the session.
      if (generatedPW.storageGUID) {
        let existingLogins = LoginHelper.searchLoginsWithObject({
          guid: generatedPW.storageGUID,
        });

        if (existingLogins.length) {
          loginToChange = existingLogins[0];
        }
        // The generated password login may have been deleted in the meantime.
        // Proceed to maybe save a new login below.
      }

      generatedPW.value = password;
    }

    let formLogin = new LoginInfo(
      formOrigin,
      formActionOrigin,
      null,
      username,
      generatedPW.value
    );

    let formLoginWithoutUsername = new LoginInfo(
      formOrigin,
      formActionOrigin,
      null,
      "",
      generatedPW.value
    );

    // This will throw if we can't look up the entry in the password/origin map
    if (!generatedPW.filled) {
      if (generatedPW.storageGUID) {
        throw new Error(
          "Generated password was saved in storage without being filled first"
        );
      }
      // record first use of this generated password
      Services.telemetry.recordEvent(
        "pwmgr",
        "autocomplete_field",
        "generatedpassword"
      );
      log("autocomplete_field telemetry event recorded");
      generatedPW.filled = true;
    }

    if (!loginToChange) {
      // Check if we already have a login saved for this site since we don't want to overwrite it in
      // case the user still needs their old password to successfully complete a password change.
      // An empty formActionOrigin is used as a wildcard to not restrict to action matches.
      let logins = this._searchAndDedupeLogins(formOrigin, {
        acceptDifferentSubdomains: false,
        httpRealm: null,
        ignoreActionAndRealm: false,
      });

      let matchedLogin = logins.find(login =>
        formLoginWithoutUsername.matches(login, true)
      );
      if (matchedLogin) {
        autoSaveLogin = false;
        if (matchedLogin.password == formLoginWithoutUsername.password) {
          // This login is already saved so show no new UI.
          log(
            "_onGeneratedPasswordFilledOrEdited: Matching login already saved"
          );
          return;
        }
        log(
          "_onGeneratedPasswordFilledOrEdited: Login with empty username already saved for this site"
        );
      }

      if (
        (matchedLogin = logins.find(login => formLogin.matches(login, true)))
      ) {
        // We're updating a previously-saved login
        loginToChange = matchedLogin;
      }
    }

    if (autoSaveLogin) {
      if (loginToChange) {
        log(
          "_onGeneratedPasswordFilledOrEdited: auto-updating login with generated password"
        );

        Services.logins.modifyLogin(
          loginToChange,
          LoginHelper.newPropertyBag({
            password,
          })
        );
        // Update `loginToChange` with the new password if modifyLogin didn't
        // throw so that the prompts later uses the new password.
        loginToChange.password = password;
      } else {
        log(
          "_onGeneratedPasswordFilledOrEdited: auto-saving new login with empty username"
        );
        loginToChange = Services.logins.addLogin(formLoginWithoutUsername);
        // Remember the GUID where we saved the generated password so we can update
        // the login if the user later edits the generated password.
        generatedPW.storageGUID = loginToChange.guid;
      }
    } else {
      log(
        "_onGeneratedPasswordFilledOrEdited: not auto-saving/updating this login"
      );
    }
    let browser = browsingContext.top.embedderElement;
    let prompter = this._getPrompter(browser, openerTopWindowID);

    if (loginToChange) {
      // Show a change doorhanger to allow modifying an already-saved login
      // e.g. to add a username or update the password.
      let autoSavedStorageGUID = "";
      if (
        generatedPW.value == loginToChange.password &&
        generatedPW.storageGUID == loginToChange.guid
      ) {
        autoSavedStorageGUID = generatedPW.storageGUID;
      }

      log(
        "_onGeneratedPasswordFilledOrEdited: promptToChangePassword with autoSavedStorageGUID: " +
          autoSavedStorageGUID
      );
      prompter.promptToChangePassword(
        loginToChange,
        formLogin,
        true, // dismissed prompt
        autoSaveLogin, // notifySaved
        autoSavedStorageGUID // autoSavedLoginGuid
      );
      return;
    }
    log("_onGeneratedPasswordFilledOrEdited: no matching login to save/update");
    prompter.promptToSavePassword(
      formLogin,
      true, // dismissed prompt
      autoSaveLogin // notifySaved
    );
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
    browser.dispatchEvent(
      new browser.ownerGlobal.CustomEvent("InsecureLoginFormsStateChange")
    );
  },
};

XPCOMUtils.defineLazyGetter(
  LoginManagerParent,
  "recipeParentPromise",
  function() {
    const { LoginRecipesParent } = ChromeUtils.import(
      "resource://gre/modules/LoginRecipes.jsm"
    );
    this._recipeManager = new LoginRecipesParent({
      defaults: Services.prefs.getStringPref("signon.recipes.path"),
    });
    return this._recipeManager.initializationPromise;
  }
);

XPCOMUtils.defineLazyPreferenceGetter(
  LoginManagerParent,
  "_repromptTimeout",
  "signon.masterPasswordReprompt.timeout_ms",
  900000
); // 15 Minutes
