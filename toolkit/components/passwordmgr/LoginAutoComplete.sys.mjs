/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * nsIAutoCompleteResult and nsILoginAutoCompleteSearch implementations for saved logins.
 */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { GenericAutocompleteItem } from "resource://gre/modules/FillHelpers.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  InsecurePasswordUtils: "resource://gre/modules/InsecurePasswordUtils.sys.mjs",
  LoginFormFactory: "resource://gre/modules/LoginFormFactory.sys.mjs",
  LoginHelper: "resource://gre/modules/LoginHelper.sys.mjs",
  LoginManagerChild: "resource://gre/modules/LoginManagerChild.sys.mjs",
  NewPasswordModel: "resource://gre/modules/NewPasswordModel.sys.mjs",
});
XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "formFillController",
  "@mozilla.org/satchel/form-fill-controller;1",
  Ci.nsIFormFillController
);
ChromeUtils.defineLazyGetter(lazy, "log", () => {
  return lazy.LoginHelper.createLogger("LoginAutoComplete");
});
ChromeUtils.defineLazyGetter(lazy, "passwordMgrBundle", () => {
  return Services.strings.createBundle(
    "chrome://passwordmgr/locale/passwordmgr.properties"
  );
});
ChromeUtils.defineLazyGetter(lazy, "dateAndTimeFormatter", () => {
  return new Services.intl.DateTimeFormat(undefined, {
    dateStyle: "medium",
  });
});

function loginSort(formHostPort, a, b) {
  let maybeHostPortA = lazy.LoginHelper.maybeGetHostPortForURL(a.origin);
  let maybeHostPortB = lazy.LoginHelper.maybeGetHostPortForURL(b.origin);
  if (formHostPort == maybeHostPortA && formHostPort != maybeHostPortB) {
    return -1;
  }
  if (formHostPort != maybeHostPortA && formHostPort == maybeHostPortB) {
    return 1;
  }

  if (a.httpRealm !== b.httpRealm) {
    // Sort HTTP auth. logins after form logins for the same origin.
    if (b.httpRealm === null) {
      return 1;
    }
    if (a.httpRealm === null) {
      return -1;
    }
  }

  let userA = a.username.toLowerCase();
  let userB = b.username.toLowerCase();

  if (userA < userB) {
    return -1;
  }

  if (userA > userB) {
    return 1;
  }

  return 0;
}

function findDuplicates(loginList) {
  let seen = new Set();
  let duplicates = new Set();
  for (let login of loginList) {
    if (seen.has(login.username)) {
      duplicates.add(login.username);
    }
    seen.add(login.username);
  }
  return duplicates;
}

function getLocalizedString(key, ...formatArgs) {
  if (formatArgs.length) {
    return lazy.passwordMgrBundle.formatStringFromName(key, formatArgs);
  }
  return lazy.passwordMgrBundle.GetStringFromName(key);
}

class AutocompleteItem {
  constructor(style) {
    this.comment = "";
    this.style = style;
    this.value = "";
  }

  removeFromStorage() {
    /* Do nothing by default */
  }
}

class InsecureLoginFormAutocompleteItem extends AutocompleteItem {
  constructor() {
    super("insecureWarning");

    this.label = getLocalizedString(
      "insecureFieldWarningDescription2",
      getLocalizedString("insecureFieldWarningLearnMore")
    );
  }
}

class LoginAutocompleteItem extends AutocompleteItem {
  login;
  #actor;

  constructor(
    login,
    hasBeenTypePassword,
    duplicateUsernames,
    actor,
    isOriginMatched
  ) {
    super("loginWithOrigin");
    this.login = login.QueryInterface(Ci.nsILoginMetaInfo);
    this.#actor = actor;

    const isDuplicateUsername =
      login.username && duplicateUsernames.has(login.username);

    let username = login.username
      ? login.username
      : getLocalizedString("noUsername");

    // If login is empty or duplicated we want to append a modification date to it.
    if (!login.username || isDuplicateUsername) {
      const time = lazy.dateAndTimeFormatter.format(
        new Date(login.timePasswordChanged)
      );
      username = getLocalizedString("loginHostAge", username, time);
    }

    this.label = username;
    this.value = hasBeenTypePassword ? login.password : login.username;
    this.comment = JSON.stringify({
      guid: login.guid,
      login, // We have to keep login here to satisfy Android
      isDuplicateUsername,
      isOriginMatched,
      comment:
        isOriginMatched && login.httpRealm === null
          ? getLocalizedString("displaySameOrigin")
          : login.displayOrigin,
    });
    this.image = `page-icon:${login.origin}`;
  }

  removeFromStorage() {
    if (this.#actor) {
      let vanilla = lazy.LoginHelper.loginToVanillaObject(this.login);
      this.#actor.sendAsyncMessage("PasswordManager:removeLogin", {
        login: vanilla,
      });
    } else {
      Services.logins.removeLogin(this.login);
    }
  }
}

class GeneratedPasswordAutocompleteItem extends AutocompleteItem {
  constructor(generatedPassword, willAutoSaveGeneratedPassword) {
    super("generatedPassword");

    this.label = getLocalizedString("useASecurelyGeneratedPassword");
    this.value = generatedPassword;
    this.comment = JSON.stringify({
      generatedPassword,
      willAutoSaveGeneratedPassword,
    });
    this.image = "chrome://browser/skin/login.svg";
  }
}

class ImportableLearnMoreAutocompleteItem extends AutocompleteItem {
  constructor() {
    super("importableLearnMore");
    this.comment = JSON.stringify({
      fillMessageName: "PasswordManager:OpenImportableLearnMore",
    });
  }
}

class ImportableLoginsAutocompleteItem extends AutocompleteItem {
  #actor;

  constructor(browserId, hostname, actor) {
    super("importableLogins");
    this.label = browserId;
    this.comment = JSON.stringify({
      hostname,
      fillMessageName: "PasswordManager:HandleImportable",
      fillMessageData: {
        browserId,
      },
    });
    this.#actor = actor;

    // This is sent for every item (re)shown, but the parent will debounce to
    // reduce the count by 1 total.
    this.#actor.sendAsyncMessage(
      "PasswordManager:decreaseSuggestImportCount",
      1
    );
  }

  removeFromStorage() {
    this.#actor.sendAsyncMessage(
      "PasswordManager:decreaseSuggestImportCount",
      100
    );
  }
}

class LoginsFooterAutocompleteItem extends AutocompleteItem {
  constructor(formHostname, telemetryEventData) {
    super("loginsFooter");

    this.label = getLocalizedString("managePasswords.label");

    // The comment field of `loginsFooter` results have many additional pieces of
    // information for telemetry purposes. After bug 1555209, this information
    // can be passed to the parent process outside of nsIAutoCompleteResult APIs
    // so we won't need this hack.
    this.comment = JSON.stringify({
      telemetryEventData,
      formHostname,
      fillMessageName: "PasswordManager:OpenPreferences",
      fillMessageData: {
        entryPoint: "autocomplete",
      },
    });
  }
}

// nsIAutoCompleteResult implementation
export class LoginAutoCompleteResult {
  #rows = [];

  constructor(
    aSearchString,
    matchingLogins,
    autocompleteItems,
    formOrigin,
    {
      generatedPassword,
      willAutoSaveGeneratedPassword,
      importable,
      isSecure,
      actor,
      hasBeenTypePassword,
      hostname,
      telemetryEventData,
    }
  ) {
    let hidingFooterOnPWFieldAutoOpened = false;
    const importableBrowsers =
      importable?.state === "import" && importable?.browsers;

    function isFooterEnabled() {
      // We need to check LoginHelper.enabled here since the insecure warning should
      // appear even if pwmgr is disabled but the footer should never appear in that case.
      if (
        !lazy.LoginHelper.showAutoCompleteFooter ||
        !lazy.LoginHelper.enabled
      ) {
        return false;
      }

      // Don't show the footer on non-empty password fields as it's not providing
      // value and only adding noise since a password was already filled.
      if (hasBeenTypePassword && aSearchString && !generatedPassword) {
        lazy.log.debug("Hiding footer: non-empty password field");
        return false;
      }

      if (
        !autocompleteItems?.length &&
        !importableBrowsers &&
        !matchingLogins.length &&
        !generatedPassword &&
        hasBeenTypePassword &&
        lazy.formFillController.passwordPopupAutomaticallyOpened
      ) {
        hidingFooterOnPWFieldAutoOpened = true;
        lazy.log.debug(
          "Hiding footer: no logins and the popup was opened upon focus of the pw. field"
        );
        return false;
      }

      return true;
    }

    this.searchString = aSearchString;

    // Insecure field warning comes first.
    if (!isSecure) {
      this.#rows.push(new InsecureLoginFormAutocompleteItem());
    }

    // Saved login items
    let formHostPort = lazy.LoginHelper.maybeGetHostPortForURL(formOrigin);
    let logins = matchingLogins.sort(loginSort.bind(null, formHostPort));
    let duplicateUsernames = findDuplicates(matchingLogins);

    for (let login of logins) {
      let item = new LoginAutocompleteItem(
        login,
        hasBeenTypePassword,
        duplicateUsernames,
        actor,
        lazy.LoginHelper.isOriginMatching(login.origin, formOrigin, {
          schemeUpgrades: lazy.LoginHelper.schemeUpgrades,
        })
      );
      this.#rows.push(item);
    }

    // The footer comes last if it's enabled
    if (isFooterEnabled()) {
      if (autocompleteItems) {
        this.#rows.push(
          ...autocompleteItems.map(
            item =>
              new GenericAutocompleteItem(
                item.image,
                item.title,
                item.subtitle,
                item.fillMessageName,
                item.fillMessageData
              )
          )
        );
      }

      if (generatedPassword) {
        this.#rows.push(
          new GeneratedPasswordAutocompleteItem(
            generatedPassword,
            willAutoSaveGeneratedPassword
          )
        );
      }

      // Suggest importing logins if there are none found.
      if (!logins.length && importableBrowsers) {
        this.#rows.push(
          ...importableBrowsers.map(
            browserId =>
              new ImportableLoginsAutocompleteItem(browserId, hostname, actor)
          )
        );
        this.#rows.push(new ImportableLearnMoreAutocompleteItem());
      }

      // If we have anything in autocomplete, then add "Manage Passwords"
      this.#rows.push(
        new LoginsFooterAutocompleteItem(hostname, telemetryEventData)
      );
    }

    // Determine the result code and default index.
    if (this.matchCount > 0) {
      this.searchResult = Ci.nsIAutoCompleteResult.RESULT_SUCCESS;
      this.defaultIndex = 0;
    } else if (hidingFooterOnPWFieldAutoOpened) {
      // We use a failure result so that the empty results aren't re-used for when
      // the user tries to manually open the popup (we want the footer in that case).
      this.searchResult = Ci.nsIAutoCompleteResult.RESULT_FAILURE;
      this.defaultIndex = -1;
    }
  }

  QueryInterface = ChromeUtils.generateQI([
    "nsIAutoCompleteResult",
    "nsISupportsWeakReference",
  ]);

  /**
   * Accessed via .wrappedJSObject
   * @private
   */
  get logins() {
    return this.#rows
      .filter(item => item instanceof LoginAutocompleteItem)
      .map(item => item.login);
  }

  // Allow autoCompleteSearch to get at the JS object so it can
  // modify some readonly properties for internal use.
  get wrappedJSObject() {
    return this;
  }

  // Interfaces from idl...
  searchString = null;
  searchResult = Ci.nsIAutoCompleteResult.RESULT_NOMATCH;
  defaultIndex = -1;
  errorDescription = "";

  get matchCount() {
    return this.#rows.length;
  }

  #throwOnBadIndex(index) {
    if (index < 0 || index >= this.matchCount) {
      throw new Error("Index out of range.");
    }
  }

  getValueAt(index) {
    this.#throwOnBadIndex(index);
    return this.#rows[index].value;
  }

  getLabelAt(index) {
    this.#throwOnBadIndex(index);
    return this.#rows[index].label;
  }

  getCommentAt(index) {
    this.#throwOnBadIndex(index);
    return this.#rows[index].comment;
  }

  getStyleAt(index) {
    this.#throwOnBadIndex(index);
    return this.#rows[index].style;
  }

  getImageAt(index) {
    this.#throwOnBadIndex(index);
    return this.#rows[index].image ?? "";
  }

  getFinalCompleteValueAt(index) {
    return this.getValueAt(index);
  }

  isRemovableAt(index) {
    this.#throwOnBadIndex(index);
    return true;
  }

  removeValueAt(index) {
    this.#throwOnBadIndex(index);

    let [removedItem] = this.#rows.splice(index, 1);

    if (this.defaultIndex > this.#rows.length) {
      this.defaultIndex--;
    }

    removedItem.removeFromStorage();
  }
}

export class LoginAutoComplete {
  // HTMLInputElement to number, the element's new-password heuristic confidence score
  #cachedNewPasswordScore = new WeakMap();
  #autoCompleteLookupPromise = null;
  classID = Components.ID("{2bdac17c-53f1-4896-a521-682ccdeef3a8}");
  QueryInterface = ChromeUtils.generateQI(["nsILoginAutoCompleteSearch"]);

  /**
   * Yuck. This is called directly by satchel:
   * nsFormFillController::StartSearch()
   * [toolkit/components/satchel/nsFormFillController.cpp]
   *
   * We really ought to have a simple way for code to register an
   * auto-complete provider, and not have satchel calling pwmgr directly.
   *
   * @param {string} aSearchString The value typed in the field.
   * @param {nsIAutoCompleteResult} aPreviousResult
   * @param {HTMLInputElement} aElement
   * @param {nsIFormAutoCompleteObserver} aCallback
   */
  startSearch(aSearchString, aPreviousResult, aElement, aCallback) {
    let { isNullPrincipal } = aElement.nodePrincipal;
    if (
      aElement.nodePrincipal.schemeIs("about") ||
      aElement.nodePrincipal.isSystemPrincipal
    ) {
      // Don't show autocomplete results for about: pages.
      // XXX: Don't we need to call the callback here?
      return;
    }

    let searchStartTimeMS = Services.telemetry.msSystemNow();

    // Show the insecure login warning in the passwords field on null principal documents.
    // Avoid loading InsecurePasswordUtils.jsm in a sandboxed document (e.g. an ad. frame) if we
    // already know it has a null principal and will therefore get the insecure autocomplete
    // treatment.
    // InsecurePasswordUtils doesn't handle the null principal case as not secure because we don't
    // want the same treatment:
    // * The web console warnings will be confusing (as they're primarily about http:) and not very
    //   useful if the developer intentionally sandboxed the document.
    // * The site identity insecure field warning would require LoginManagerChild being loaded and
    //   listening to some of the DOM events we're ignoring in null principal documents. For memory
    //   reasons it's better to not load LMC at all for these sandboxed frames. Also, if the top-
    //   document is sandboxing a document, it probably doesn't want that sandboxed document to be
    //   able to affect the identity icon in the address bar by adding a password field.
    let form = lazy.LoginFormFactory.createFromField(aElement);
    let isSecure =
      !isNullPrincipal && lazy.InsecurePasswordUtils.isFormSecure(form);
    let { hasBeenTypePassword } = aElement;
    let hostname = aElement.ownerDocument.documentURIObject.host;
    let formOrigin = lazy.LoginHelper.getLoginOrigin(
      aElement.ownerDocument.documentURI
    );
    let loginManagerActor = lazy.LoginManagerChild.forWindow(
      aElement.ownerGlobal
    );
    let completeSearch = async autoCompleteLookupPromise => {
      // Assign to the member synchronously before awaiting the Promise.
      this.#autoCompleteLookupPromise = autoCompleteLookupPromise;

      let {
        generatedPassword,
        importable,
        logins,
        autocompleteItems,
        willAutoSaveGeneratedPassword,
      } = await autoCompleteLookupPromise;

      // If the search was canceled before we got our
      // results, don't bother reporting them.
      // N.B. This check must occur after the `await` above for it to be
      // effective.
      if (this.#autoCompleteLookupPromise !== autoCompleteLookupPromise) {
        lazy.log.debug("Ignoring result from previous search.");
        return;
      }

      let telemetryEventData = {
        acFieldName: aElement.getAutocompleteInfo().fieldName,
        hadPrevious: !!aPreviousResult,
        typeWasPassword: aElement.hasBeenTypePassword,
        fieldType: aElement.type,
        searchStartTimeMS,
        stringLength: aSearchString.length,
      };

      this.#autoCompleteLookupPromise = null;
      let results = new LoginAutoCompleteResult(
        aSearchString,
        logins,
        autocompleteItems,
        formOrigin,
        {
          generatedPassword,
          willAutoSaveGeneratedPassword,
          importable,
          actor: loginManagerActor,
          isSecure,
          hasBeenTypePassword,
          hostname,
          telemetryEventData,
        }
      );
      aCallback.onSearchCompletion(results);
    };

    if (isNullPrincipal) {
      // Don't search login storage when the field has a null principal as we don't want to fill
      // logins for the `location` in this case.
      completeSearch(Promise.resolve({ logins: [] }));
      return;
    }

    if (
      hasBeenTypePassword &&
      aSearchString &&
      !loginManagerActor.isPasswordGenerationForcedOn(aElement)
    ) {
      // Return empty result on password fields with password already filled,
      // unless password generation was forced.
      completeSearch(Promise.resolve({ logins: [] }));
      return;
    }

    if (!lazy.LoginHelper.enabled) {
      completeSearch(Promise.resolve({ logins: [] }));
      return;
    }

    let previousResult;
    if (aPreviousResult) {
      previousResult = {
        searchString: aPreviousResult.searchString,
        logins: lazy.LoginHelper.loginsToVanillaObjects(
          aPreviousResult.wrappedJSObject.logins
        ),
      };
    } else {
      previousResult = null;
    }

    let acLookupPromise = this.#requestAutoCompleteResultsFromParent({
      searchString: aSearchString,
      previousResult,
      inputElement: aElement,
      form,
      hasBeenTypePassword,
    });
    completeSearch(acLookupPromise).catch(lazy.log.error.bind(lazy.log));
  }

  stopSearch() {
    this.#autoCompleteLookupPromise = null;
  }

  async #requestAutoCompleteResultsFromParent({
    searchString,
    previousResult,
    inputElement,
    form,
    hasBeenTypePassword,
  }) {
    let actionOrigin = lazy.LoginHelper.getFormActionOrigin(form);
    let autocompleteInfo = inputElement.getAutocompleteInfo();

    let loginManagerActor = lazy.LoginManagerChild.forWindow(
      inputElement.ownerGlobal
    );
    let forcePasswordGeneration = false;
    let isProbablyANewPasswordField = false;
    if (hasBeenTypePassword) {
      forcePasswordGeneration =
        loginManagerActor.isPasswordGenerationForcedOn(inputElement);
      // Run the Fathom model only if the password field does not have the
      // autocomplete="new-password" attribute.
      isProbablyANewPasswordField =
        autocompleteInfo.fieldName == "new-password" ||
        this.isProbablyANewPasswordField(inputElement);
    }
    const scenario = loginManagerActor.getScenario(inputElement);

    if (lazy.LoginHelper.showAutoCompleteFooter) {
      gAutoCompleteListener.init();
    }

    lazy.log.debug("LoginAutoComplete search:", {
      forcePasswordGeneration,
      hasBeenTypePassword,
      isProbablyANewPasswordField,
      searchStringLength: searchString.length,
    });

    const result = await loginManagerActor.sendQuery(
      "PasswordManager:autoCompleteLogins",
      {
        actionOrigin,
        searchString,
        previousResult,
        forcePasswordGeneration,
        hasBeenTypePassword,
        isProbablyANewPasswordField,
        scenarioName: scenario?.constructor.name,
        inputMaxLength: inputElement.maxLength,
      }
    );

    return {
      generatedPassword: result.generatedPassword,
      importable: result.importable,
      autocompleteItems: result.autocompleteItems,
      logins: lazy.LoginHelper.vanillaObjectsToLogins(result.logins),
      willAutoSaveGeneratedPassword: result.willAutoSaveGeneratedPassword,
    };
  }

  isProbablyANewPasswordField(inputElement) {
    const threshold = lazy.LoginHelper.generationConfidenceThreshold;
    if (threshold == -1) {
      // Fathom is disabled
      return false;
    }

    let score = this.#cachedNewPasswordScore.get(inputElement);
    if (score) {
      return score >= threshold;
    }

    const { rules, type } = lazy.NewPasswordModel;
    const results = rules.against(inputElement);
    score = results.get(inputElement).scoreFor(type);
    this.#cachedNewPasswordScore.set(inputElement, score);
    return score >= threshold;
  }
}

let gAutoCompleteListener = {
  added: false,
  fillRequestId: 0,

  init() {
    if (!this.added) {
      Services.obs.addObserver(this, "autocomplete-will-enter-text");
      this.added = true;
    }
  },

  async observe(subject, topic, data) {
    switch (topic) {
      case "autocomplete-will-enter-text": {
        await this.sendFillRequestToLoginManagerParent(subject, data);
        break;
      }
    }
  },

  async sendFillRequestToLoginManagerParent(input, comment) {
    if (!comment) {
      return;
    }

    if (input != lazy.formFillController.controller.input) {
      return;
    }

    const { fillMessageName, fillMessageData } = JSON.parse(comment ?? "{}");
    if (!fillMessageName) {
      return;
    }

    this.fillRequestId++;
    const fillRequestId = this.fillRequestId;
    const child = lazy.LoginManagerChild.forWindow(
      input.focusedInput.ownerGlobal
    );
    const value = await child.sendQuery(fillMessageName, fillMessageData ?? {});

    // skip fill if another fill operation started during await
    if (fillRequestId != this.fillRequestId) {
      return;
    }

    if (typeof value !== "string") {
      return;
    }

    // If LoginManagerParent returned a string to fill, we must do it here because
    // nsAutoCompleteController.cpp already finished it's work before we finished await.
    input.textValue = value;
    input.selectTextRange(value.length, value.length);
  },
};
