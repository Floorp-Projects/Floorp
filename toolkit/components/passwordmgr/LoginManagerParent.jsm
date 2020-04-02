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

XPCOMUtils.defineLazyServiceGetter(
  this,
  "prompterSvc",
  "@mozilla.org/login-manager/prompter;1",
  Ci.nsILoginManagerPrompter
);

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let logger = LoginHelper.createLogger("LoginManagerParent");
  return logger.log.bind(logger);
});

const EXPORTED_SYMBOLS = ["LoginManagerParent"];

/**
 * A listener for notifications to tests.
 */
let gListenerForTests = null;

/**
 * A map of a principal's origin (including suffixes) to a generated password string and filled flag
 * so that we can offer the same password later (e.g. in a confirmation field).
 *
 * We don't currently evict from this cache so entries should last until the end of the browser
 * session. That may change later but for now a typical session would max out at a few entries.
 */
let gGeneratedPasswordsByPrincipalOrigin = new Map();

/**
 * Reference to the default LoginRecipesParent (instead of the initialization promise) for
 * synchronous access. This is a temporary hack and new consumers should yield on
 * recipeParentPromise instead.
 *
 * @type LoginRecipesParent
 * @deprecated
 */
let gRecipeManager = null;

/**
 * Tracks the last time the user cancelled the master password prompt,
 *  to avoid spamming master password prompts on autocomplete searches.
 * TODO: Bug XXX - Should be `Number.NEGATIVE_INFINITY`.
 */
let gLastMPLoginCancelled = Math.NEGATIVE_INFINITY;

let gGeneratedPasswordObserver = {
  addedObserver: false,

  observe(subject, topic, data) {
    if (topic == "last-pb-context-exited") {
      // The last private browsing context closed so clear all cached generated
      // passwords for private window origins.
      for (let principalOrigin of gGeneratedPasswordsByPrincipalOrigin.keys()) {
        let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
          principalOrigin
        );
        if (!principal.privateBrowsingId) {
          // The origin isn't for a private context so leave it alone.
          continue;
        }
        gGeneratedPasswordsByPrincipalOrigin.delete(principalOrigin);
      }
      return;
    }

    if (
      topic == "passwordmgr-autosaved-login-merged" ||
      (topic == "passwordmgr-storage-changed" && data == "removeLogin")
    ) {
      let { origin, guid } = subject;
      let generatedPW = gGeneratedPasswordsByPrincipalOrigin.get(origin);

      // in the case where an autosaved login removed or merged into an existing login,
      // clear the guid associated with the generated-password cache entry
      if (
        generatedPW &&
        (guid == generatedPW.storageGUID ||
          topic == "passwordmgr-autosaved-login-merged")
      ) {
        log(
          "Removing storageGUID for generated-password cache entry on origin:",
          origin
        );
        generatedPW.storageGUID = null;
      }
    }
  },
};

Services.ppmm.addMessageListener("PasswordManager:findRecipes", message => {
  let formHost = new URL(message.data.formOrigin).host;
  return gRecipeManager.getRecipesForHost(formHost);
});

class LoginManagerParent extends JSWindowActorParent {
  // This is used by tests to listen to form submission.
  static setListenerForTests(listener) {
    gListenerForTests = listener;
  }

  // Used by tests to clean up recipes only when they were actually used.
  static get _recipeManager() {
    return gRecipeManager;
  }

  // Some unit tests need to access this.
  static getGeneratedPasswordsByPrincipalOrigin() {
    return gGeneratedPasswordsByPrincipalOrigin;
  }

  getRootBrowser() {
    let browsingContext = null;
    if (this._overrideBrowsingContextId) {
      browsingContext = BrowsingContext.get(this._overrideBrowsingContextId);
    } else {
      browsingContext = this.browsingContext.top;
    }
    return browsingContext.embedderElement;
  }

  /**
   * @param {origin} formOrigin
   * @param {object} options
   * @param {origin?} options.formActionOrigin To match on. Omit this argument to match all action origins.
   * @param {origin?} options.httpRealm To match on. Omit this argument to match all realms.
   * @param {boolean} options.acceptDifferentSubdomains Include results for eTLD+1 matches
   * @param {boolean} options.ignoreActionAndRealm Include all form and HTTP auth logins for the site
   */
  static async searchAndDedupeLogins(
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
      logins = await Services.logins.searchLoginsAsync(matchData);
    } catch (e) {
      // Record the last time the user cancelled the MP prompt
      // to avoid spamming them with MP prompts for autocomplete.
      if (e.result == Cr.NS_ERROR_ABORT) {
        log("User cancelled master password prompt.");
        gLastMPLoginCancelled = Date.now();
        return [];
      }
      throw e;
    }

    logins = LoginHelper.shadowHTTPLogins(logins);

    let resolveBy = [
      "subdomain",
      "actionOrigin",
      "scheme",
      "timePasswordChanged",
    ];
    return LoginHelper.dedupeLogins(
      logins,
      ["username", "password"],
      resolveBy,
      formOrigin,
      formActionOrigin
    );
  }

  receiveMessage(msg) {
    let data = msg.data;
    switch (msg.name) {
      case "PasswordManager:findLogins": {
        // TODO Verify the target's principals against the formOrigin?
        return this.sendLoginDataToChild(
          data.formOrigin,
          data.actionOrigin,
          data.options
        );
      }

      case "PasswordManager:onFormSubmit": {
        // TODO Verify msg.target's principals against the formOrigin?
        let browser = this.getRootBrowser();
        let submitPromise = this.onFormSubmit(browser, data);
        if (gListenerForTests) {
          submitPromise.then(() => {
            gListenerForTests("FormSubmit", data);
          });
        }
        break;
      }

      case "PasswordManager:onPasswordEditedOrGenerated": {
        log("Received PasswordManager:onPasswordEditedOrGenerated");
        if (gListenerForTests) {
          log("calling gListenerForTests");
          gListenerForTests("PasswordEditedOrGenerated", {});
        }
        let browser = this.getRootBrowser();
        this._onPasswordEditedOrGenerated(browser, data);
        break;
      }

      case "PasswordManager:autoCompleteLogins": {
        return this.doAutocompleteSearch(data);
      }

      case "PasswordManager:removeLogin": {
        let login = LoginHelper.vanillaObjectToLogin(data.login);
        Services.logins.removeLogin(login);
        break;
      }

      case "PasswordManager:OpenPreferences": {
        let window = this.getRootBrowser().ownerGlobal;
        LoginHelper.openPasswordManager(window, {
          filterString: msg.data.hostname,
          entryPoint: msg.data.entryPoint,
        });
        break;
      }

      // Used by tests to detect that a form-fill has occurred. This redirects
      // to the top-level browsing context.
      case "PasswordManager:formProcessed": {
        let topActor = this.browsingContext.top.currentWindowGlobal.getActor(
          "LoginManager"
        );
        topActor.sendAsyncMessage("PasswordManager:formProcessed", {
          formid: data.formid,
        });
        if (gListenerForTests) {
          gListenerForTests("FormProcessed", {
            browsingContext: this.browsingContext,
          });
        }
        break;
      }
    }

    return undefined;
  }

  /**
   * Trigger a login form fill and send relevant data (e.g. logins and recipes)
   * to the child process (LoginManagerChild).
   */
  async fillForm({ browser, loginFormOrigin, login, inputElementIdentifier }) {
    let recipes = [];
    if (loginFormOrigin) {
      let formHost;
      try {
        formHost = new URL(loginFormOrigin).host;
        let recipeManager = await LoginManagerParent.recipeParentPromise;
        recipes = recipeManager.getRecipesForHost(formHost);
      } catch (ex) {
        // Some schemes e.g. chrome aren't supported by URL
      }
    }

    // Convert the array of nsILoginInfo to vanilla JS objects since nsILoginInfo
    // doesn't support structured cloning.
    let jsLogins = [LoginHelper.loginToVanillaObject(login)];

    let browserURI = browser.currentURI.spec;
    let originMatches =
      LoginHelper.getLoginOrigin(browserURI) == loginFormOrigin;

    this.sendAsyncMessage("PasswordManager:fillForm", {
      inputElementIdentifier,
      loginFormOrigin,
      originMatches,
      logins: jsLogins,
      recipes,
    });
  }

  /**
   * Send relevant data (e.g. logins and recipes) to the child process (LoginManagerChild).
   */
  async sendLoginDataToChild(
    formOrigin,
    actionOrigin,
    { guid, showMasterPassword }
  ) {
    let recipes = [];
    let formHost;
    try {
      formHost = new URL(formOrigin).host;
      let recipeManager = await LoginManagerParent.recipeParentPromise;
      recipes = recipeManager.getRecipesForHost(formHost);
    } catch (ex) {
      // Some schemes e.g. chrome aren't supported by URL
    }

    if (!showMasterPassword && !Services.logins.isLoggedIn) {
      return { logins: [], recipes };
    }

    // If we're currently displaying a master password prompt, defer
    // processing this form until the user handles the prompt.
    if (Services.logins.uiBusy) {
      log("deferring sendLoginDataToChild for", formOrigin);

      let uiBusyPromiseResolve;
      let uiBusyPromise = new Promise(resolve => {
        uiBusyPromiseResolve = resolve;
      });

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
            uiBusyPromiseResolve({ logins: [], recipes });
            return;
          }

          let result = self.sendLoginDataToChild(formOrigin, actionOrigin, {
            showMasterPassword,
          });
          uiBusyPromiseResolve(result);
        },
      };

      // Possible leak: it's possible that neither of these notifications
      // will fire, and if that happens, we'll leak the observer (and
      // never return). We should guarantee that at least one of these
      // will fire.
      // See bug XXX.
      Services.obs.addObserver(observer, "passwordmgr-crypto-login");
      Services.obs.addObserver(observer, "passwordmgr-crypto-loginCanceled");

      return uiBusyPromise;
    }

    // Autocomplete results do not need to match actionOrigin or exact origin.
    let logins = null;
    if (guid) {
      logins = await Services.logins.searchLoginsAsync({
        guid,
        origin: formOrigin,
      });
    } else {
      logins = await LoginManagerParent.searchAndDedupeLogins(formOrigin, {
        formActionOrigin: actionOrigin,
        ignoreActionAndRealm: true,
        acceptDifferentSubdomains: LoginHelper.includeOtherSubdomainsInLookup,
      });
    }

    log("sendLoginDataToChild:", logins.length, "deduped logins");
    // Convert the array of nsILoginInfo to vanilla JS objects since nsILoginInfo
    // doesn't support structured cloning.
    let jsLogins = LoginHelper.loginsToVanillaObjects(logins);
    return { logins: jsLogins, recipes };
  }

  async doAutocompleteSearch({
    formOrigin,
    actionOrigin,
    searchString,
    previousResult,
    forcePasswordGeneration,
    hasBeenTypePassword,
    isSecure,
    isProbablyANewPasswordField,
  }) {
    // Note: previousResult is a regular object, not an
    // nsIAutoCompleteResult.

    // Cancel if the master password prompt is already showing or we unsuccessfully prompted for it too recently.
    if (!Services.logins.isLoggedIn) {
      if (Services.logins.uiBusy) {
        log(
          "Not searching logins for autocomplete since the master password prompt is already showing"
        );
        // Return an empty array to make LoginManagerChild clear the
        // outstanding request it has temporarily saved.
        return { logins: [] };
      }

      let timeDiff = Date.now() - gLastMPLoginCancelled;
      if (timeDiff < LoginManagerParent._repromptTimeout) {
        log(
          "Not searching logins for autocomplete since the master password " +
            `prompt was last cancelled ${Math.round(
              timeDiff / 1000
            )} seconds ago.`
        );
        // Return an empty array to make LoginManagerChild clear the
        // outstanding request it has temporarily saved.
        return { logins: [] };
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
      logins = await LoginManagerParent.searchAndDedupeLogins(formOrigin, {
        formActionOrigin: actionOrigin,
        ignoreActionAndRealm: true,
        acceptDifferentSubdomains: LoginHelper.includeOtherSubdomainsInLookup,
      });
    }

    let matchingLogins = logins.filter(function(fullMatch) {
      let match = fullMatch.username;

      // Remove results that are too short, or have different prefix.
      // Also don't offer empty usernames as possible results except
      // for on password fields.
      if (hasBeenTypePassword) {
        return true;
      }
      return match && match.toLowerCase().startsWith(searchStringLower);
    });

    let generatedPassword = null;
    let willAutoSaveGeneratedPassword = false;
    if (
      // If MP was cancelled above, don't try to offer pwgen or access storage again (causing a new MP prompt).
      Services.logins.isLoggedIn &&
      (forcePasswordGeneration ||
        (isProbablyANewPasswordField &&
          Services.logins.getLoginSavingEnabled(formOrigin)))
    ) {
      generatedPassword = this.getGeneratedPassword();
      let potentialConflictingLogins = LoginHelper.searchLoginsWithObject({
        origin: formOrigin,
        formActionOrigin: actionOrigin,
        httpRealm: null,
      });
      willAutoSaveGeneratedPassword = !potentialConflictingLogins.find(
        login => login.username == ""
      );
    }

    // Convert the array of nsILoginInfo to vanilla JS objects since nsILoginInfo
    // doesn't support structured cloning.
    let jsLogins = LoginHelper.loginsToVanillaObjects(matchingLogins);
    return {
      generatedPassword,
      logins: jsLogins,
      willAutoSaveGeneratedPassword,
    };
  }

  /**
   * Expose `BrowsingContext` so we can stub it in tests.
   */
  static get _browsingContextGlobal() {
    return BrowsingContext;
  }

  // Set an override context within a test.
  useBrowsingContext(browsingContextId = 0) {
    this._overrideBrowsingContextId = browsingContextId;
  }

  getBrowsingContextToUse() {
    if (this._overrideBrowsingContextId) {
      return BrowsingContext.get(this._overrideBrowsingContextId);
    }

    return this.browsingContext;
  }

  getGeneratedPassword() {
    if (
      !LoginHelper.enabled ||
      !LoginHelper.generationAvailable ||
      !LoginHelper.generationEnabled
    ) {
      return null;
    }

    let browsingContext = this.getBrowsingContextToUse();
    if (!browsingContext) {
      return null;
    }
    let framePrincipalOrigin =
      browsingContext.currentWindowGlobal.documentPrincipal.origin;
    // Use the same password if we already generated one for this origin so that it doesn't change
    // with each search/keystroke and the user can easily re-enter a password in a confirmation field.
    let generatedPW = gGeneratedPasswordsByPrincipalOrigin.get(
      framePrincipalOrigin
    );
    if (generatedPW) {
      return generatedPW.value;
    }

    generatedPW = {
      autocompleteShown: false,
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

    // Add these observers when a password is assigned.
    if (!gGeneratedPasswordObserver.addedObserver) {
      Services.obs.addObserver(
        gGeneratedPasswordObserver,
        "passwordmgr-autosaved-login-merged"
      );
      Services.obs.addObserver(
        gGeneratedPasswordObserver,
        "passwordmgr-storage-changed"
      );
      Services.obs.addObserver(
        gGeneratedPasswordObserver,
        "last-pb-context-exited"
      );
      gGeneratedPasswordObserver.addedObserver = true;
    }

    gGeneratedPasswordsByPrincipalOrigin.set(framePrincipalOrigin, generatedPW);
    return generatedPW.value;
  }

  maybeRecordPasswordGenerationShownTelemetryEvent(autocompleteResults) {
    if (!autocompleteResults.some(r => r.style == "generatedPassword")) {
      return;
    }

    let browsingContext = this.getBrowsingContextToUse();

    let framePrincipalOrigin =
      browsingContext.currentWindowGlobal.documentPrincipal.origin;
    let generatedPW = gGeneratedPasswordsByPrincipalOrigin.get(
      framePrincipalOrigin
    );

    // We only want to record the first time it was shown for an origin
    if (generatedPW.autocompleteShown) {
      return;
    }

    generatedPW.autocompleteShown = true;

    Services.telemetry.recordEvent(
      "pwmgr",
      "autocomplete_shown",
      "generatedpassword"
    );
  }

  /**
   * Used for stubbing by tests.
   */
  _getPrompter() {
    return prompterSvc;
  }

  async onFormSubmit(
    browser,
    {
      origin,
      browsingContextId,
      formActionOrigin,
      autoFilledLoginGuid,
      usernameField,
      newPasswordField,
      oldPasswordField,
      dismissedPrompt,
    }
  ) {
    function recordLoginUse(login) {
      if (!browser || PrivateBrowsingUtils.isBrowserPrivate(browser)) {
        // don't record non-interactive use in private browsing
        return;
      }
      Services.logins.recordPasswordUse(login);
    }

    // If password storage is disabled, bail out.
    if (!LoginHelper.storageEnabled) {
      return;
    }

    if (!Services.logins.getLoginSavingEnabled(origin)) {
      log("(form submission ignored -- saving is disabled for:", origin, ")");
      return;
    }

    let browsingContext = BrowsingContext.get(browsingContextId);
    let framePrincipalOrigin =
      browsingContext.currentWindowGlobal.documentPrincipal.origin;
    log("onFormSubmit, got framePrincipalOrigin: ", framePrincipalOrigin);

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
      let loginsForGuid = await Services.logins.searchLoginsAsync({
        guid: autoFilledLoginGuid,
        origin,
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

    let existingLogin = null;
    let canMatchExistingLogin = true;
    // Below here we have one login per hostPort + action + username with the
    // matching scheme being preferred.
    let logins = await LoginManagerParent.searchAndDedupeLogins(origin, {
      formActionOrigin,
    });

    let generatedPW = gGeneratedPasswordsByPrincipalOrigin.get(
      framePrincipalOrigin
    );
    let autoSavedStorageGUID = "";
    if (generatedPW && generatedPW.storageGUID) {
      autoSavedStorageGUID = generatedPW.storageGUID;
    }

    // If we didn't find a username field, but seem to be changing a
    // password, allow the user to select from a list of applicable
    // logins to update the password for.
    if (!usernameField && oldPasswordField && logins.length) {
      if (logins.length == 1) {
        existingLogin = logins[0];

        if (existingLogin.password == formLogin.password) {
          recordLoginUse(existingLogin);
          log(
            "(Not prompting to save/change since we have no username and the " +
              "only saved password matches the new password)"
          );
          return;
        }

        formLogin.username = existingLogin.username;
        formLogin.usernameField = existingLogin.usernameField;
      } else if (!generatedPW || generatedPW.value != newPasswordField.value) {
        // Note: It's possible that that we already have the correct u+p saved
        // but since we don't have the username, we don't know if the user is
        // changing a second account to the new password so we ask anyways.
        canMatchExistingLogin = false;
      }
    }

    if (canMatchExistingLogin && !existingLogin) {
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
    }

    let promptBrowser = LoginHelper.getBrowserForPrompt(browser);
    let prompter = this._getPrompter(browser);

    if (!canMatchExistingLogin) {
      prompter.promptToChangePasswordWithUsernames(
        promptBrowser,
        logins,
        formLogin
      );
      return;
    }
    if (existingLogin) {
      log("Found an existing login matching this form submission");

      // Change password if needed.
      if (existingLogin.password != formLogin.password) {
        log("...passwords differ, prompting to change.");
        prompter.promptToChangePassword(
          promptBrowser,
          existingLogin,
          formLogin,
          dismissedPrompt,
          false, // notifySaved
          autoSavedStorageGUID,
          autoFilledLoginGuid
        );
      } else if (!existingLogin.username && formLogin.username) {
        log("...empty username update, prompting to change.");
        let prompter = this._getPrompter(browser);
        prompter.promptToChangePassword(
          promptBrowser,
          existingLogin,
          formLogin,
          dismissedPrompt,
          false, // notifySaved
          autoSavedStorageGUID,
          autoFilledLoginGuid
        );
      } else {
        recordLoginUse(existingLogin);
      }

      return;
    }

    // Prompt user to save login (via dialog or notification bar)
    prompter.promptToSavePassword(
      promptBrowser,
      formLogin,
      dismissedPrompt,
      false, // notifySaved
      autoFilledLoginGuid
    );
  }

  async _onPasswordEditedOrGenerated(
    browser,
    {
      origin,
      formActionOrigin,
      autoFilledLoginGuid,
      newPasswordField,
      usernameField = null,
      oldPasswordField,
      triggeredByFillingGenerated = false,
    }
  ) {
    log(
      "_onPasswordEditedOrGenerated, triggeredByFillingGenerated:",
      triggeredByFillingGenerated
    );

    // If password storage is disabled, bail out.
    if (!LoginHelper.storageEnabled) {
      return;
    }

    if (!Services.logins.getLoginSavingEnabled(origin)) {
      // No UI should be shown to offer generation in this case but a user may
      // disable saving for the site after already filling one and they may then
      // edit it.
      log("_onPasswordEditedOrGenerated: saving is disabled for:", origin);
      return;
    }

    if (!newPasswordField.value) {
      log("_onPasswordEditedOrGenerated: The password field is empty");
      return;
    }

    if (!browser) {
      log("_onPasswordEditedOrGenerated: The browser is gone");
      return;
    }

    let browsingContext = this.getBrowsingContextToUse();
    if (!browsingContext) {
      return;
    }

    let framePrincipalOrigin =
      browsingContext.currentWindowGlobal.documentPrincipal.origin;
    log(
      "_onPasswordEditedOrGenerated: got framePrincipalOrigin: ",
      framePrincipalOrigin
    );

    let {
      originNoSuffix,
    } = browsingContext.currentWindowGlobal.documentPrincipal;
    let formOrigin = LoginHelper.getLoginOrigin(originNoSuffix);
    if (formOrigin !== origin) {
      log(
        "_onPasswordEditedOrGenerated: Invalid form origin:",
        browsingContext.currentWindowGlobal.documentPrincipal
      );
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
    let existingLogin = null;
    let canMatchExistingLogin = true;
    let shouldAutoSaveLogin = triggeredByFillingGenerated;
    let autoSavedLogin = null;

    if (autoFilledLoginGuid) {
      let [matchedLogin] = await Services.logins.searchLoginsAsync({
        guid: autoFilledLoginGuid,
        origin,
      });
      if (
        matchedLogin &&
        matchedLogin.password == formLogin.password &&
        (!formLogin.username || // Also cover cases where only the password is requested.
          matchedLogin.username == formLogin.username)
      ) {
        log("The filled login matches the changed fields. Nothing to change.");
        // We may want to update an existing doorhanger
        existingLogin = matchedLogin;
      }
    }

    let generatedPW = gGeneratedPasswordsByPrincipalOrigin.get(
      framePrincipalOrigin
    );

    // Below here we have one login per hostPort + action + username with the
    // matching scheme being preferred.
    let logins = await LoginManagerParent.searchAndDedupeLogins(origin, {
      formActionOrigin,
    });
    // only used in the generated pw case where we auto-save
    let formLoginWithoutUsername;

    if (triggeredByFillingGenerated && generatedPW) {
      log("Got cached generatedPW");
      formLoginWithoutUsername = new LoginInfo(
        formOrigin,
        formActionOrigin,
        null,
        "",
        newPasswordField.value
      );

      if (newPasswordField.value != generatedPW.value) {
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
      }

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

      // We may have already autosaved this login
      // Note that it could have been saved in a totally different tab in the session.
      if (generatedPW.storageGUID) {
        [autoSavedLogin] = await Services.logins.searchLoginsAsync({
          guid: generatedPW.storageGUID,
          origin: formOrigin,
        });

        if (autoSavedLogin) {
          log(
            "_onPasswordEditedOrGenerated: login to change is the auto-saved login"
          );
          existingLogin = autoSavedLogin;
        }
        // The generated password login may have been deleted in the meantime.
        // Proceed to maybe save a new login below.
      }
      generatedPW.value = newPasswordField.value;

      if (!existingLogin) {
        log(
          "_onPasswordEditedOrGenerated: Didnt match generated-password login"
        );

        // Check if we already have a login saved for this site since we don't want to overwrite it in
        // case the user still needs their old password to successfully complete a password change.
        let matchedLogin = logins.find(login =>
          formLoginWithoutUsername.matches(login, true)
        );
        if (matchedLogin) {
          shouldAutoSaveLogin = false;
          if (matchedLogin.password == formLoginWithoutUsername.password) {
            // This login is already saved so show no new UI.
            // We may want to update an existing doorhanger though...
            log("_onPasswordEditedOrGenerated: Matching login already saved");
            existingLogin = matchedLogin;
          }
          log(
            "_onPasswordEditedOrGenerated: Login with empty username already saved for this site"
          );
        }
      }
    }

    // If we didn't find a username field, but seem to be changing a
    // password, use the first match if there is only one
    // If there's more than one we'll prompt to save with the initial formLogin
    // and let the doorhanger code resolve this
    if (
      !triggeredByFillingGenerated &&
      !existingLogin &&
      !usernameField &&
      oldPasswordField &&
      logins.length
    ) {
      if (logins.length == 1) {
        existingLogin = logins[0];

        if (existingLogin.password == formLogin.password) {
          log(
            "(Not prompting to save/change since we have no username and the " +
              "only saved password matches the new password)"
          );
          return;
        }

        formLogin.username = existingLogin.username;
        formLogin.usernameField = existingLogin.usernameField;
      } else if (!generatedPW || generatedPW.value != newPasswordField.value) {
        // Note: It's possible that that we already have the correct u+p saved
        // but since we don't have the username, we don't know if the user is
        // changing a second account to the new password so we ask anyways.
        canMatchExistingLogin = false;
      }
    }

    if (canMatchExistingLogin && !existingLogin) {
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
          log("_onPasswordEditedOrGenerated: matched saved login");
          break;
        }
      }
    }

    if (shouldAutoSaveLogin) {
      if (existingLogin && existingLogin == autoSavedLogin) {
        log(
          "_onPasswordEditedOrGenerated: updating auto-saved login with changed password"
        );

        Services.logins.modifyLogin(
          existingLogin,
          LoginHelper.newPropertyBag({
            password: formLogin.password,
          })
        );
        // Update `existingLogin` with the new password if modifyLogin didn't
        // throw so that the prompts later uses the new password.
        existingLogin.password = formLogin.password;
      } else {
        log(
          "_onPasswordEditedOrGenerated: auto-saving new login with empty username"
        );
        existingLogin = Services.logins.addLogin(formLoginWithoutUsername);
        // Remember the GUID where we saved the generated password so we can update
        // the login if the user later edits the generated password.
        generatedPW.storageGUID = existingLogin.guid;
      }
    } else {
      log("_onPasswordEditedOrGenerated: not auto-saving this login");
    }

    let prompter = this._getPrompter(browser);
    let promptBrowser = LoginHelper.getBrowserForPrompt(browser);

    if (existingLogin) {
      // Show a change doorhanger to allow modifying an already-saved login
      // e.g. to add a username or update the password.
      let autoSavedStorageGUID = "";
      if (
        generatedPW &&
        generatedPW.value == existingLogin.password &&
        generatedPW.storageGUID == existingLogin.guid
      ) {
        autoSavedStorageGUID = generatedPW.storageGUID;
      }

      // Change password if needed.
      if (
        (shouldAutoSaveLogin && !formLogin.username) ||
        existingLogin.password != formLogin.password
      ) {
        log(
          "_onPasswordEditedOrGenerated: promptToChangePassword with autoSavedStorageGUID: " +
            autoSavedStorageGUID
        );
        prompter.promptToChangePassword(
          promptBrowser,
          existingLogin,
          formLogin,
          true, // dismissed prompt
          shouldAutoSaveLogin, // notifySaved
          autoSavedStorageGUID, // autoSavedLoginGuid
          autoFilledLoginGuid
        );
      } else if (!existingLogin.username && formLogin.username) {
        log("...empty username update, prompting to change.");
        prompter.promptToChangePassword(
          promptBrowser,
          existingLogin,
          formLogin,
          true, // dismissed prompt
          shouldAutoSaveLogin, // notifySaved
          autoSavedStorageGUID, // autoSavedLoginGuid
          autoFilledLoginGuid
        );
      } else {
        log("_onPasswordEditedOrGenerated: No change to existing login");
        // is there a doorhanger we should update?
        let popupNotifications = promptBrowser.ownerGlobal.PopupNotifications;
        let notif = popupNotifications.getNotification("password", browser);
        log(
          "_onPasswordEditedOrGenerated: Has doorhanger?",
          notif && notif.dismissed
        );
        if (notif && notif.dismissed) {
          prompter.promptToChangePassword(
            promptBrowser,
            existingLogin,
            formLogin,
            true, // dismissed prompt
            false, // notifySaved
            autoSavedStorageGUID, // autoSavedLoginGuid
            autoFilledLoginGuid
          );
        }
      }
      return;
    }
    log("_onPasswordEditedOrGenerated: no matching login to save/update");
    prompter.promptToSavePassword(
      promptBrowser,
      formLogin,
      true, // dismissed prompt
      shouldAutoSaveLogin, // notifySaved
      autoFilledLoginGuid
    );
  }

  static get recipeParentPromise() {
    if (!gRecipeManager) {
      const { LoginRecipesParent } = ChromeUtils.import(
        "resource://gre/modules/LoginRecipes.jsm"
      );
      gRecipeManager = new LoginRecipesParent({
        defaults: Services.prefs.getStringPref("signon.recipes.path"),
      });
    }

    return gRecipeManager.initializationPromise;
  }
}

XPCOMUtils.defineLazyPreferenceGetter(
  LoginManagerParent,
  "_repromptTimeout",
  "signon.masterPasswordReprompt.timeout_ms",
  900000
); // 15 Minutes
