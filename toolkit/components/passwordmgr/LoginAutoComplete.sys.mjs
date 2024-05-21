/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * nsIAutoCompleteResult implementations for saved logins.
 */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { GenericAutocompleteItem } from "resource://gre/modules/FillHelpers.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  LoginHelper: "resource://gre/modules/LoginHelper.sys.mjs",
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
      fillMessageName: "PasswordManager:OnFieldAutoComplete",
      fillMessageData: login,
      guid: login.guid,
      isDuplicateUsername,
      isOriginMatched,
      secondary:
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
      fillMessageName: "PasswordManager:FillGeneratedPassword",
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
                item.label,
                item.secondary,
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

  isRemovableAt(_index) {
    return false;
  }

  removeValueAt(_index) {
    return false;
  }
}
