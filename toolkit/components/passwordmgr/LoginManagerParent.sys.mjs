/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { FirefoxRelayTelemetry } from "resource://gre/modules/FirefoxRelayTelemetry.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const LoginInfo = new Components.Constructor(
  "@mozilla.org/login-manager/loginInfo;1",
  Ci.nsILoginInfo,
  "init"
);

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "LoginRelatedRealmsParent", () => {
  const { LoginRelatedRealmsParent } = ChromeUtils.importESModule(
    "resource://gre/modules/LoginRelatedRealms.sys.mjs"
  );
  return new LoginRelatedRealmsParent();
});

ChromeUtils.defineLazyGetter(lazy, "PasswordRulesManager", () => {
  const { PasswordRulesManagerParent } = ChromeUtils.importESModule(
    "resource://gre/modules/PasswordRulesManager.sys.mjs"
  );
  return new PasswordRulesManagerParent();
});

ChromeUtils.defineESModuleGetters(lazy, {
  ChromeMigrationUtils: "resource:///modules/ChromeMigrationUtils.sys.mjs",
  FirefoxRelay: "resource://gre/modules/FirefoxRelay.sys.mjs",
  LoginHelper: "resource://gre/modules/LoginHelper.sys.mjs",
  MigrationUtils: "resource:///modules/MigrationUtils.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  PasswordGenerator: "resource://gre/modules/PasswordGenerator.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "prompterSvc",
  "@mozilla.org/login-manager/prompter;1",
  Ci.nsILoginManagerPrompter
);

ChromeUtils.defineLazyGetter(lazy, "log", () => {
  let logger = lazy.LoginHelper.createLogger("LoginManagerParent");
  return logger.log.bind(logger);
});
ChromeUtils.defineLazyGetter(lazy, "debug", () => {
  let logger = lazy.LoginHelper.createLogger("LoginManagerParent");
  return logger.debug.bind(logger);
});

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
 * Tracks the last time the user cancelled the primary password prompt,
 *  to avoid spamming primary password prompts on autocomplete searches.
 */
let gLastMPLoginCancelled = Number.NEGATIVE_INFINITY;

let gGeneratedPasswordObserver = {
  addedObserver: false,

  observe(subject, topic, data) {
    if (topic == "last-pb-context-exited") {
      // The last private browsing context closed so clear all cached generated
      // passwords for private window origins.
      for (let principalOrigin of gGeneratedPasswordsByPrincipalOrigin.keys()) {
        let principal =
          Services.scriptSecurityManager.createContentPrincipalFromOrigin(
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

    // We cache generated passwords in gGeneratedPasswordsByPrincipalOrigin.
    // When generated password used on the page,
    // we store a login with generated password and without username.
    // When user updates that autosaved login with username,
    // we must clear cached generated password.
    // This will generate a new password next time user needs it.
    if (topic == "passwordmgr-storage-changed" && data == "modifyLogin") {
      const originalLogin = subject.GetElementAt(0);
      const updatedLogin = subject.GetElementAt(1);

      if (originalLogin && !originalLogin.username && updatedLogin?.username) {
        const generatedPassword = gGeneratedPasswordsByPrincipalOrigin.get(
          originalLogin.origin
        );

        if (
          originalLogin.password == generatedPassword.value &&
          updatedLogin.password == generatedPassword.value
        ) {
          gGeneratedPasswordsByPrincipalOrigin.delete(originalLogin.origin);
        }
      }
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
        lazy.log(
          `Removing storageGUID for generated-password cache entry on origin: ${origin}.`
        );
        generatedPW.storageGUID = null;
      }
    }
  },
};

Services.ppmm.addMessageListener("PasswordManager:findRecipes", message => {
  let formHost = new URL(message.data.formOrigin).host;
  return gRecipeManager?.getRecipesForHost(formHost) ?? [];
});

/**
 * Lazily create a Map of origins to array of browsers with importable logins.
 *
 * @param {origin} formOrigin
 * @returns {Object?} containing array of migration browsers and experiment state.
 */
async function getImportableLogins(formOrigin) {
  // Include the experiment state for data and UI decisions; otherwise skip
  // importing if not supported or disabled.
  const state =
    lazy.LoginHelper.suggestImportCount > 0 &&
    lazy.LoginHelper.showAutoCompleteImport;
  return state
    ? {
        browsers: await lazy.ChromeMigrationUtils.getImportableLogins(
          formOrigin
        ),
        state,
      }
    : null;
}

export class LoginManagerParent extends JSWindowActorParent {
  possibleValues = {
    // This is stored at the parent (i.e., frame) scope because the LoginManagerPrompter
    // is shared across all frames.
    //
    // It is mutated to update values without forcing us to set a new doorhanger.
    usernames: new Set(),
    passwords: new Set(),
  };

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
   * @param {string[]} options.relatedRealms Related realms to match against when searching
   */
  static async searchAndDedupeLogins(
    formOrigin,
    {
      acceptDifferentSubdomains,
      formActionOrigin,
      httpRealm,
      ignoreActionAndRealm,
      relatedRealms,
    } = {}
  ) {
    let logins;
    let matchData = {
      origin: formOrigin,
      schemeUpgrades: lazy.LoginHelper.schemeUpgrades,
      acceptDifferentSubdomains,
    };
    if (!ignoreActionAndRealm) {
      if (typeof formActionOrigin != "undefined") {
        matchData.formActionOrigin = formActionOrigin;
      } else if (typeof httpRealm != "undefined") {
        matchData.httpRealm = httpRealm;
      }
    }
    if (lazy.LoginHelper.relatedRealmsEnabled) {
      matchData.acceptRelatedRealms = lazy.LoginHelper.relatedRealmsEnabled;
      matchData.relatedRealms = relatedRealms;
    }
    try {
      logins = await Services.logins.searchLoginsAsync(matchData);
    } catch (e) {
      // Record the last time the user cancelled the MP prompt
      // to avoid spamming them with MP prompts for autocomplete.
      if (e.result == Cr.NS_ERROR_ABORT) {
        lazy.log("User cancelled primary password prompt.");
        gLastMPLoginCancelled = Date.now();
        return [];
      }
      throw e;
    }

    logins = lazy.LoginHelper.shadowHTTPLogins(logins);

    let resolveBy = [
      "subdomain",
      "actionOrigin",
      "scheme",
      "timePasswordChanged",
    ];
    return lazy.LoginHelper.dedupeLogins(
      logins,
      ["username", "password"],
      resolveBy,
      formOrigin,
      formActionOrigin
    );
  }

  async receiveMessage(msg) {
    let data = msg.data;
    if (data.origin || data.formOrigin) {
      throw new Error(
        "The child process should not send an origin to the parent process. See bug 1513003"
      );
    }
    let context = {};
    ChromeUtils.defineLazyGetter(context, "origin", () => {
      // We still need getLoginOrigin to remove the path for file: URIs until we fix bug 1625391.
      let origin = lazy.LoginHelper.getLoginOrigin(
        this.manager.documentPrincipal?.originNoSuffix
      );
      if (!origin) {
        throw new Error("An origin is required. Message name: " + msg.name);
      }
      return origin;
    });

    switch (msg.name) {
      case "PasswordManager:updateDoorhangerSuggestions": {
        this.#onUpdateDoorhangerSuggestions(data.possibleValues);
        break;
      }

      case "PasswordManager:decreaseSuggestImportCount": {
        this.decreaseSuggestImportCount(data);
        break;
      }

      case "PasswordManager:findLogins": {
        return this.sendLoginDataToChild(
          context.origin,
          data.actionOrigin,
          data.options
        );
      }

      case "PasswordManager:onFormSubmit": {
        this.#onFormSubmit(context);
        break;
      }

      case "PasswordManager:onPasswordEditedOrGenerated": {
        this.#onPasswordEditedOrGenerated(context, data);
        break;
      }

      case "PasswordManager:onIgnorePasswordEdit": {
        this.#onIgnorePasswordEdit();
        break;
      }

      case "PasswordManager:ShowDoorhanger": {
        this.#onShowDoorhanger(context, data);
        break;
      }

      case "PasswordManager:autoCompleteLogins": {
        return this.doAutocompleteSearch(context.origin, data);
      }

      case "PasswordManager:removeLogin": {
        this.#onRemoveLogin(data.login);
        break;
      }

      case "PasswordManager:OpenImportableLearnMore": {
        this.#onOpenImportableLearnMore();
        break;
      }

      case "PasswordManager:HandleImportable": {
        await this.#onHandleImportable(data.browserId);
        break;
      }

      case "PasswordManager:OpenPreferences": {
        this.#onOpenPreferences(data.hostname, data.entryPoint);
        break;
      }

      // Used by tests to detect that a form-fill has occurred. This redirects
      // to the top-level browsing context.
      case "PasswordManager:formProcessed": {
        this.#onFormProcessed(data.formid, data.autofillResult);
        break;
      }

      case "PasswordManager:offerRelayIntegration": {
        FirefoxRelayTelemetry.recordRelayOfferedEvent(
          "clicked",
          data.telemetry.flowId,
          data.telemetry.scenarioName
        );
        return this.#offerRelayIntegration(context.origin);
      }

      case "PasswordManager:generateRelayUsername": {
        FirefoxRelayTelemetry.recordRelayUsernameFilledEvent(
          "clicked",
          data.telemetry.flowId
        );
        return this.#generateRelayUsername(context.origin);
      }
    }

    return undefined;
  }

  #onUpdateDoorhangerSuggestions(possibleValues) {
    this.possibleValues.usernames = possibleValues.usernames;
    this.possibleValues.passwords = possibleValues.passwords;
  }

  #onFormSubmit(context) {
    Services.obs.notifyObservers(
      null,
      "passwordmgr-form-submission-detected",
      context.origin
    );
  }

  #onPasswordEditedOrGenerated(context, data) {
    lazy.log("#onPasswordEditedOrGenerated: Received PasswordManager.");
    if (gListenerForTests) {
      lazy.log("#onPasswordEditedOrGenerated: Calling gListenerForTests.");
      gListenerForTests("PasswordEditedOrGenerated", {});
    }
    let browser = this.getRootBrowser();
    this._onPasswordEditedOrGenerated(browser, context.origin, data);
  }

  #onIgnorePasswordEdit() {
    lazy.log("#onIgnorePasswordEdit: Received PasswordManager.");
    if (gListenerForTests) {
      lazy.log("#onIgnorePasswordEdit: Calling gListenerForTests.");
      gListenerForTests("PasswordIgnoreEdit", {});
    }
  }

  #onShowDoorhanger(context, data) {
    const browser = this.getRootBrowser();
    const submitPromise = this.showDoorhanger(browser, context.origin, data);
    if (gListenerForTests) {
      submitPromise.then(() => {
        gListenerForTests("ShowDoorhanger", {
          origin: context.origin,
          data,
        });
      });
    }
  }

  #onRemoveLogin(login) {
    login = lazy.LoginHelper.vanillaObjectToLogin(login);
    Services.logins.removeLogin(login);
  }

  #onOpenImportableLearnMore() {
    const window = this.getRootBrowser().ownerGlobal;
    window.openTrustedLinkIn(
      Services.urlFormatter.formatURLPref("app.support.baseURL") +
        "password-import",
      "tab",
      { relatedToCurrent: true }
    );
  }

  async #onHandleImportable(browserId) {
    // Directly migrate passwords for a single profile.
    const migrator = await lazy.MigrationUtils.getMigrator(browserId);
    const profiles = await migrator.getSourceProfiles();
    if (
      profiles.length == 1 &&
      lazy.NimbusFeatures["password-autocomplete"].getVariable(
        "directMigrateSingleProfile"
      )
    ) {
      const loginAdded = new Promise(resolve => {
        const obs = (_subject, _topic, data) => {
          if (data == "addLogin") {
            Services.obs.removeObserver(obs, "passwordmgr-storage-changed");
            resolve();
          }
        };
        Services.obs.addObserver(obs, "passwordmgr-storage-changed");
      });

      await migrator.migrate(
        lazy.MigrationUtils.resourceTypes.PASSWORDS,
        null,
        profiles[0]
      );
      await loginAdded;

      // Reshow the popup with the imported password.
      this.sendAsyncMessage("PasswordManager:repopulateAutocompletePopup");
    } else {
      // Open the migration wizard pre-selecting the appropriate browser.
      lazy.MigrationUtils.showMigrationWizard(
        this.getRootBrowser().ownerGlobal,
        {
          entrypoint: lazy.MigrationUtils.MIGRATION_ENTRYPOINTS.PASSWORDS,
          migratorKey: browserId,
        }
      );
    }
  }

  #onOpenPreferences(hostname, entryPoint) {
    const window = this.getRootBrowser().ownerGlobal;
    lazy.LoginHelper.openPasswordManager(window, {
      filterString: hostname,
      entryPoint,
    });
  }

  #onFormProcessed(formid, autofillResult) {
    const topActor =
      this.browsingContext.currentWindowGlobal.getActor("LoginManager");
    topActor.sendAsyncMessage("PasswordManager:formProcessed", { formid });
    if (gListenerForTests) {
      gListenerForTests("FormProcessed", {
        browsingContext: this.browsingContext,
        data: {
          formId: formid,
          autofillResult,
        },
      });
    }
  }

  async #offerRelayIntegration(origin) {
    const browser = lazy.LoginHelper.getBrowserForPrompt(this.getRootBrowser());
    return lazy.FirefoxRelay.offerRelayIntegration(browser, origin);
  }

  async #generateRelayUsername(origin) {
    const browser = lazy.LoginHelper.getBrowserForPrompt(this.getRootBrowser());
    return lazy.FirefoxRelay.generateUsername(browser, origin);
  }

  /**
   * Update the remaining number of import suggestion impressions with debounce
   * to allow multiple popups showing the "same" items to count as one.
   */
  decreaseSuggestImportCount(count) {
    // Delay an existing timer with a potentially larger count.
    if (this._suggestImportTimer) {
      this._suggestImportTimer.delay =
        LoginManagerParent.SUGGEST_IMPORT_DEBOUNCE_MS;
      this._suggestImportCount = Math.max(count, this._suggestImportCount);
      return;
    }

    this._suggestImportTimer = Cc["@mozilla.org/timer;1"].createInstance(
      Ci.nsITimer
    );
    this._suggestImportTimer.init(
      () => {
        this._suggestImportTimer = null;
        Services.prefs.setIntPref(
          "signon.suggestImportCount",
          lazy.LoginHelper.suggestImportCount - this._suggestImportCount
        );
      },
      LoginManagerParent.SUGGEST_IMPORT_DEBOUNCE_MS,
      Ci.nsITimer.TYPE_ONE_SHOT
    );
    this._suggestImportCount = count;
  }

  async #getRecipesForHost(origin) {
    let recipes;
    if (origin) {
      try {
        const formHost = new URL(origin).host;
        let recipeManager = await LoginManagerParent.recipeParentPromise;
        recipes = recipeManager.getRecipesForHost(formHost);
      } catch (ex) {
        // Some schemes e.g. chrome aren't supported by URL
      }
    }

    return recipes ?? [];
  }

  /**
   * Trigger a login form fill and send relevant data (e.g. logins and recipes)
   * to the child process (LoginManagerChild).
   */
  async fillForm({
    browser,
    loginFormOrigin,
    login,
    inputElementIdentifier,
    style,
  }) {
    const recipes = await this.#getRecipesForHost(loginFormOrigin);

    // Convert the array of nsILoginInfo to vanilla JS objects since nsILoginInfo
    // doesn't support structured cloning.
    const jsLogins = [lazy.LoginHelper.loginToVanillaObject(login)];

    const browserURI = browser.currentURI.spec;
    const originMatches =
      lazy.LoginHelper.getLoginOrigin(browserURI) == loginFormOrigin;

    this.sendAsyncMessage("PasswordManager:fillForm", {
      inputElementIdentifier,
      loginFormOrigin,
      originMatches,
      logins: jsLogins,
      recipes,
      style,
    });
  }

  /**
   * Send relevant data (e.g. logins and recipes) to the child process (LoginManagerChild).
   */
  async sendLoginDataToChild(
    formOrigin,
    actionOrigin,
    { guid, showPrimaryPassword }
  ) {
    const recipes = await this.#getRecipesForHost(formOrigin);

    if (!showPrimaryPassword && !Services.logins.isLoggedIn) {
      return { logins: [], recipes };
    }

    // If we're currently displaying a primary password prompt, defer
    // processing this form until the user handles the prompt.
    if (Services.logins.uiBusy) {
      lazy.log(
        "UI is busy. Deferring sendLoginDataToChild for form: ",
        formOrigin
      );

      let uiBusyPromiseResolve;
      const uiBusyPromise = new Promise(resolve => {
        uiBusyPromiseResolve = resolve;
      });

      const self = this;
      const observer = {
        QueryInterface: ChromeUtils.generateQI([
          "nsIObserver",
          "nsISupportsWeakReference",
        ]),

        observe(_subject, topic, _data) {
          lazy.log("Got deferred sendLoginDataToChild notification:", topic);
          // Only run observer once.
          Services.obs.removeObserver(this, "passwordmgr-crypto-login");
          Services.obs.removeObserver(this, "passwordmgr-crypto-loginCanceled");
          if (topic == "passwordmgr-crypto-loginCanceled") {
            uiBusyPromiseResolve({ logins: [], recipes });
            return;
          }

          const result = self.sendLoginDataToChild(formOrigin, actionOrigin, {
            showPrimaryPassword,
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
      let relatedRealmsOrigins = [];
      if (lazy.LoginHelper.relatedRealmsEnabled) {
        relatedRealmsOrigins =
          await lazy.LoginRelatedRealmsParent.findRelatedRealms(formOrigin);
      }
      logins = await LoginManagerParent.searchAndDedupeLogins(formOrigin, {
        formActionOrigin: actionOrigin,
        ignoreActionAndRealm: true,
        acceptDifferentSubdomains:
          lazy.LoginHelper.includeOtherSubdomainsInLookup,
        relatedRealms: relatedRealmsOrigins,
      });

      if (lazy.LoginHelper.relatedRealmsEnabled) {
        lazy.debug(
          "Adding related logins on page load",
          logins.map(l => l.origin)
        );
      }
    }
    lazy.log(`Deduped ${logins.length} logins.`);
    // Convert the array of nsILoginInfo to vanilla JS objects since nsILoginInfo
    // doesn't support structured cloning.
    let jsLogins = lazy.LoginHelper.loginsToVanillaObjects(logins);
    return {
      importable: await getImportableLogins(formOrigin),
      logins: jsLogins,
      recipes,
    };
  }

  async doAutocompleteSearch(
    formOrigin,
    {
      actionOrigin,
      searchString,
      previousResult,
      forcePasswordGeneration,
      hasBeenTypePassword,
      isProbablyANewPasswordField,
      scenarioName,
      inputMaxLength,
    }
  ) {
    // Note: previousResult is a regular object, not an
    // nsIAutoCompleteResult.

    // Cancel if the primary password prompt is already showing or we unsuccessfully prompted for it too recently.
    if (!Services.logins.isLoggedIn) {
      if (Services.logins.uiBusy) {
        lazy.log(
          "Not searching logins for autocomplete since the primary password prompt is already showing."
        );
        // Return an empty array to make LoginManagerChild clear the
        // outstanding request it has temporarily saved.
        return { logins: [] };
      }

      const timeDiff = Date.now() - gLastMPLoginCancelled;
      if (timeDiff < LoginManagerParent._repromptTimeout) {
        lazy.log(
          `Not searching logins for autocomplete since the primary password prompt was last cancelled ${Math.round(
            timeDiff / 1000
          )} seconds ago.`
        );
        // Return an empty array to make LoginManagerChild clear the
        // outstanding request it has temporarily saved.
        return { logins: [] };
      }
    }

    const searchStringLower = searchString.toLowerCase();
    let logins;
    if (
      previousResult &&
      searchStringLower.startsWith(previousResult.searchString.toLowerCase())
    ) {
      lazy.log("Using previous autocomplete result.");

      // We have a list of results for a shorter search string, so just
      // filter them further based on the new search string.
      logins = lazy.LoginHelper.vanillaObjectsToLogins(previousResult.logins);
    } else {
      lazy.log("Creating new autocomplete search result.");
      let relatedRealmsOrigins = [];
      if (lazy.LoginHelper.relatedRealmsEnabled) {
        relatedRealmsOrigins =
          await lazy.LoginRelatedRealmsParent.findRelatedRealms(formOrigin);
      }
      // Autocomplete results do not need to match actionOrigin or exact origin.
      logins = await LoginManagerParent.searchAndDedupeLogins(formOrigin, {
        formActionOrigin: actionOrigin,
        ignoreActionAndRealm: true,
        acceptDifferentSubdomains:
          lazy.LoginHelper.includeOtherSubdomainsInLookup,
        relatedRealms: relatedRealmsOrigins,
      });
    }

    const matchingLogins = logins.filter(fullMatch => {
      // Remove results that are too short, or have different prefix.
      // Also don't offer empty usernames as possible results except
      // for on password fields.
      if (hasBeenTypePassword) {
        return true;
      }

      const match = fullMatch.username;

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
      // We either generate a new password here, or grab the previously generated password
      // if we're still on the same domain when we generated the password
      generatedPassword = await this.getGeneratedPassword({ inputMaxLength });
      const potentialConflictingLogins =
        await Services.logins.searchLoginsAsync({
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
    let jsLogins = lazy.LoginHelper.loginsToVanillaObjects(matchingLogins);

    return {
      generatedPassword,
      importable: await getImportableLogins(formOrigin),
      autocompleteItems: hasBeenTypePassword
        ? []
        : await lazy.FirefoxRelay.autocompleteItemsAsync({
            formOrigin,
            scenarioName,
            hasInput: !!searchStringLower.length,
          }),
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

  async getGeneratedPassword({ inputMaxLength } = {}) {
    if (
      !lazy.LoginHelper.enabled ||
      !lazy.LoginHelper.generationAvailable ||
      !lazy.LoginHelper.generationEnabled
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
    let generatedPW =
      gGeneratedPasswordsByPrincipalOrigin.get(framePrincipalOrigin);
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
    };
    if (lazy.LoginHelper.improvedPasswordRulesEnabled) {
      generatedPW.value = await lazy.PasswordRulesManager.generatePassword(
        browsingContext.currentWindowGlobal.documentURI,
        { inputMaxLength }
      );
    } else {
      generatedPW.value = lazy.PasswordGenerator.generatePassword({
        inputMaxLength,
      });
    }

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
    let generatedPW =
      gGeneratedPasswordsByPrincipalOrigin.get(framePrincipalOrigin);

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
    return lazy.prompterSvc;
  }

  // Look for an existing login that matches the form login.
  #findSameLogin(logins, formLogin) {
    return logins.find(login => {
      let same;

      // If one login has a username but the other doesn't, ignore
      // the username when comparing and only match if they have the
      // same password. Otherwise, compare the logins and match even
      // if the passwords differ.
      if (!login.username && formLogin.username) {
        let restoreMe = formLogin.username;
        formLogin.username = "";
        same = lazy.LoginHelper.doLoginsMatch(formLogin, login, {
          ignorePassword: false,
          ignoreSchemes: lazy.LoginHelper.schemeUpgrades,
        });
        formLogin.username = restoreMe;
      } else if (!formLogin.username && login.username) {
        formLogin.username = login.username;
        same = lazy.LoginHelper.doLoginsMatch(formLogin, login, {
          ignorePassword: false,
          ignoreSchemes: lazy.LoginHelper.schemeUpgrades,
        });
        formLogin.username = ""; // we know it's always blank.
      } else {
        same = lazy.LoginHelper.doLoginsMatch(formLogin, login, {
          ignorePassword: true,
          ignoreSchemes: lazy.LoginHelper.schemeUpgrades,
        });
      }

      return same;
    });
  }

  async showDoorhanger(
    browser,
    formOrigin,
    {
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
      Services.logins.recordPasswordUse(
        login,
        browser && lazy.PrivateBrowsingUtils.isBrowserPrivate(browser),
        login.username ? "form_login" : "form_password",
        !!autoFilledLoginGuid
      );
    }

    // If password storage is disabled, bail out.
    if (!lazy.LoginHelper.storageEnabled) {
      return;
    }

    if (!Services.logins.getLoginSavingEnabled(formOrigin)) {
      lazy.log(
        `Form submission ignored because saving is disabled for origin: ${formOrigin}.`
      );
      return;
    }

    let browsingContext = BrowsingContext.get(browsingContextId);
    let framePrincipalOrigin =
      browsingContext.currentWindowGlobal.documentPrincipal.origin;

    let formLogin = new LoginInfo(
      formOrigin,
      formActionOrigin,
      null,
      usernameField?.value ?? "",
      newPasswordField.value,
      usernameField?.name ?? "",
      newPasswordField.name
    );
    // we don't auto-save logins on form submit
    let notifySaved = false;

    if (autoFilledLoginGuid) {
      let loginsForGuid = await Services.logins.searchLoginsAsync({
        guid: autoFilledLoginGuid,
        origin: formOrigin, // Ignored outside of GV.
      });
      if (
        loginsForGuid.length == 1 &&
        loginsForGuid[0].password == formLogin.password &&
        (!formLogin.username || // Also cover cases where only the password is requested.
          loginsForGuid[0].username == formLogin.username)
      ) {
        lazy.log(
          "The filled login matches the form submission. Nothing to change."
        );
        recordLoginUse(loginsForGuid[0]);
        return;
      }
    }

    let existingLogin = null;
    let canMatchExistingLogin = true;
    // Below here we have one login per hostPort + action + username with the
    // matching scheme being preferred.
    const logins = await LoginManagerParent.searchAndDedupeLogins(formOrigin, {
      formActionOrigin,
    });

    const generatedPW =
      gGeneratedPasswordsByPrincipalOrigin.get(framePrincipalOrigin);
    const autoSavedStorageGUID = generatedPW?.storageGUID ?? "";

    // If we didn't find a username field, but seem to be changing a
    // password, allow the user to select from a list of applicable
    // logins to update the password for.
    if (!usernameField && oldPasswordField && logins.length) {
      if (logins.length == 1) {
        existingLogin = logins[0];

        if (existingLogin.password == formLogin.password) {
          recordLoginUse(existingLogin);
          lazy.log(
            "Not prompting to save/change since we have no username and the only saved password matches the new password."
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
      existingLogin = this.#findSameLogin(logins, formLogin);
    }

    const promptBrowser = lazy.LoginHelper.getBrowserForPrompt(browser);
    const prompter = this._getPrompter(browser);

    if (!canMatchExistingLogin) {
      prompter.promptToChangePasswordWithUsernames(
        promptBrowser,
        logins,
        formLogin
      );
      return;
    }

    if (existingLogin) {
      lazy.log("Found an existing login matching this form submission.");

      // Change password if needed.
      if (existingLogin.password != formLogin.password) {
        lazy.log("Passwords differ, prompting to change.");
        prompter.promptToChangePassword(
          promptBrowser,
          existingLogin,
          formLogin,
          dismissedPrompt,
          notifySaved,
          autoSavedStorageGUID,
          autoFilledLoginGuid,
          this.possibleValues
        );
      } else if (!existingLogin.username && formLogin.username) {
        lazy.log("Empty username update, prompting to change.");
        prompter.promptToChangePassword(
          promptBrowser,
          existingLogin,
          formLogin,
          dismissedPrompt,
          notifySaved,
          autoSavedStorageGUID,
          autoFilledLoginGuid,
          this.possibleValues
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
      notifySaved,
      autoFilledLoginGuid,
      this.possibleValues
    );
  }

  /**
   * Performs validation of inputs against already-saved logins in order to determine whether and
   * how these inputs can be stored. Depending on validation, will either no-op or show a 'save'
   * or 'update' dialog to the user.
   *
   * This is called after any of the following:
   *   - The user edits a password
   *   - A generated password is filled
   *   - The user edits a username (when a matching password field has already been filled)
   *
   * @param {Element} browser
   * @param {string} formOrigin
   * @param {string} options.formActionOrigin
   * @param {string?} options.autoFilledLoginGuid
   * @param {Object} options.newPasswordField
   * @param {Object?} options.usernameField
   * @param {Element?} options.oldPasswordField
   * @param {boolean} [options.triggeredByFillingGenerated = false]
   */
  /* eslint-disable-next-line complexity */
  async _onPasswordEditedOrGenerated(
    browser,
    formOrigin,
    {
      formActionOrigin,
      autoFilledLoginGuid,
      newPasswordField,
      usernameField = null,
      oldPasswordField,
      triggeredByFillingGenerated = false,
    }
  ) {
    lazy.log(
      `_onPasswordEditedOrGenerated: triggeredByFillingGenerated: ${triggeredByFillingGenerated}.`
    );

    // If password storage is disabled, bail out.
    if (!lazy.LoginHelper.storageEnabled) {
      return;
    }

    if (!Services.logins.getLoginSavingEnabled(formOrigin)) {
      // No UI should be shown to offer generation in this case but a user may
      // disable saving for the site after already filling one and they may then
      // edit it.
      lazy.log(`Saving is disabled for origin: ${formOrigin}.`);
      return;
    }

    if (!newPasswordField.value) {
      lazy.log("The password field is empty.");
      return;
    }

    if (!browser) {
      lazy.log("The browser is gone.");
      return;
    }

    let browsingContext = this.getBrowsingContextToUse();
    if (!browsingContext) {
      return;
    }

    if (!triggeredByFillingGenerated && !Services.logins.isLoggedIn) {
      // Don't show the dismissed doorhanger on "input" or "change" events
      // when the Primary Password is locked
      lazy.log(
        "Edited field is not a generated password field, and Primary Password is locked."
      );
      return;
    }

    let framePrincipalOrigin =
      browsingContext.currentWindowGlobal.documentPrincipal.origin;

    lazy.log("Got framePrincipalOrigin: ", framePrincipalOrigin);

    let formLogin = new LoginInfo(
      formOrigin,
      formActionOrigin,
      null,
      usernameField?.value ?? "",
      newPasswordField.value,
      usernameField?.name ?? "",
      newPasswordField.name
    );
    let existingLogin = null;
    let canMatchExistingLogin = true;
    let shouldAutoSaveLogin = triggeredByFillingGenerated;
    let autoSavedLogin = null;
    let notifySaved = false;

    if (autoFilledLoginGuid) {
      let [matchedLogin] = await Services.logins.searchLoginsAsync({
        guid: autoFilledLoginGuid,
        origin: formOrigin, // Ignored outside of GV.
      });
      if (
        matchedLogin &&
        matchedLogin.password == formLogin.password &&
        (!formLogin.username || // Also cover cases where only the password is requested.
          matchedLogin.username == formLogin.username)
      ) {
        lazy.log(
          "The filled login matches the changed fields. Nothing to change."
        );
        // We may want to update an existing doorhanger
        existingLogin = matchedLogin;
      }
    }

    let generatedPW =
      gGeneratedPasswordsByPrincipalOrigin.get(framePrincipalOrigin);

    // Below here we have one login per hostPort + action + username with the
    // matching scheme being preferred.
    let logins = await LoginManagerParent.searchAndDedupeLogins(formOrigin, {
      formActionOrigin,
    });
    // only used in the generated pw case where we auto-save
    let formLoginWithoutUsername;

    if (triggeredByFillingGenerated && generatedPW) {
      lazy.log("Got cached generatedPW.");
      formLoginWithoutUsername = new LoginInfo(
        formOrigin,
        formActionOrigin,
        null,
        "",
        newPasswordField.value
      );

      if (newPasswordField.value != generatedPW.value) {
        // The user edited the field after generation to a non-empty value.
        lazy.log("The field containing the generated password has changed.");

        // Record telemetry for the first edit
        if (!generatedPW.edited) {
          Services.telemetry.recordEvent(
            "pwmgr",
            "filled_field_edited",
            "generatedpassword"
          );
          lazy.log("filled_field_edited telemetry event recorded.");
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
        lazy.log("autocomplete_field telemetry event recorded.");
        generatedPW.filled = true;
      }

      // We may have already autosaved this login
      // Note that it could have been saved in a totally different tab in the session.
      if (generatedPW.storageGUID) {
        [autoSavedLogin] = await Services.logins.searchLoginsAsync({
          guid: generatedPW.storageGUID,
          origin: formOrigin, // Ignored outside of GV.
        });

        if (autoSavedLogin) {
          lazy.log("login to change is the auto-saved login.");
          existingLogin = autoSavedLogin;
        }
        // The generated password login may have been deleted in the meantime.
        // Proceed to maybe save a new login below.
      }
      generatedPW.value = newPasswordField.value;

      if (!existingLogin) {
        lazy.log("Did not match generated-password login.");

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
            lazy.log("Matching login already saved.");
            existingLogin = matchedLogin;
          }
          lazy.log(
            "_onPasswordEditedOrGenerated: Login with empty username already saved for this site."
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
          lazy.log(
            "Not prompting to save/change since we have no username and the " +
              "only saved password matches the new password."
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
      existingLogin = this.#findSameLogin(logins, formLogin);
      if (existingLogin) {
        lazy.log("Matched saved login.");
      }
    }

    if (shouldAutoSaveLogin) {
      if (
        existingLogin &&
        existingLogin == autoSavedLogin &&
        existingLogin.password !== formLogin.password
      ) {
        lazy.log("Updating auto-saved login.");

        Services.logins.modifyLogin(
          existingLogin,
          lazy.LoginHelper.newPropertyBag({
            password: formLogin.password,
          })
        );
        notifySaved = true;
        // Update `existingLogin` with the new password if modifyLogin didn't
        // throw so that the prompts later uses the new password.
        existingLogin.password = formLogin.password;
      } else if (!autoSavedLogin) {
        lazy.log("Auto-saving new login with empty username.");
        existingLogin = await Services.logins.addLoginAsync(
          formLoginWithoutUsername
        );
        // Remember the GUID where we saved the generated password so we can update
        // the login if the user later edits the generated password.
        generatedPW.storageGUID = existingLogin.guid;
        notifySaved = true;
      }
    } else {
      lazy.log("Not auto-saving this login.");
    }

    const prompter = this._getPrompter(browser);
    const promptBrowser = lazy.LoginHelper.getBrowserForPrompt(browser);

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
        lazy.log(
          `promptToChangePassword with autoSavedStorageGUID: ${autoSavedStorageGUID}`
        );
        prompter.promptToChangePassword(
          promptBrowser,
          existingLogin,
          formLogin,
          true, // dismissed prompt
          notifySaved,
          autoSavedStorageGUID, // autoSavedLoginGuid
          autoFilledLoginGuid,
          this.possibleValues
        );
      } else if (!existingLogin.username && formLogin.username) {
        lazy.log("Empty username update, prompting to change.");
        prompter.promptToChangePassword(
          promptBrowser,
          existingLogin,
          formLogin,
          true, // dismissed prompt
          notifySaved,
          autoSavedStorageGUID, // autoSavedLoginGuid
          autoFilledLoginGuid,
          this.possibleValues
        );
      } else {
        lazy.log("No change to existing login.");
        // is there a doorhanger we should update?
        let popupNotifications = promptBrowser.ownerGlobal.PopupNotifications;
        let notif = popupNotifications.getNotification("password", browser);
        lazy.log(
          `_onPasswordEditedOrGenerated: Has doorhanger? ${
            notif && notif.dismissed
          }`
        );
        if (notif && notif.dismissed) {
          prompter.promptToChangePassword(
            promptBrowser,
            existingLogin,
            formLogin,
            true, // dismissed prompt
            notifySaved,
            autoSavedStorageGUID, // autoSavedLoginGuid
            autoFilledLoginGuid,
            this.possibleValues
          );
        }
      }
      return;
    }
    lazy.log("No matching login to save/update.");
    prompter.promptToSavePassword(
      promptBrowser,
      formLogin,
      true, // dismissed prompt
      notifySaved,
      autoFilledLoginGuid,
      this.possibleValues
    );
  }

  static get recipeParentPromise() {
    if (!gRecipeManager) {
      const { LoginRecipesParent } = ChromeUtils.importESModule(
        "resource://gre/modules/LoginRecipes.sys.mjs"
      );
      gRecipeManager = new LoginRecipesParent({
        defaults: Services.prefs.getStringPref("signon.recipes.path"),
      });
    }

    return gRecipeManager.initializationPromise;
  }
}

LoginManagerParent.SUGGEST_IMPORT_DEBOUNCE_MS = 10000;

XPCOMUtils.defineLazyPreferenceGetter(
  LoginManagerParent,
  "_repromptTimeout",
  "signon.masterPasswordReprompt.timeout_ms",
  900000
); // 15 Minutes
