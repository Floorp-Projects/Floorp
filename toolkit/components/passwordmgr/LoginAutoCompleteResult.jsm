/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * nsIAutoCompleteResult and nsILoginAutoCompleteSearch implementations for saved logins.
 */

"use strict";

const EXPORTED_SYMBOLS = ["LoginAutoComplete", "LoginAutoCompleteResult"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "InsecurePasswordUtils",
  "resource://gre/modules/InsecurePasswordUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "LoginFormFactory",
  "resource://gre/modules/LoginFormFactory.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "LoginHelper",
  "resource://gre/modules/LoginHelper.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "LoginManagerContent",
  "resource://gre/modules/LoginManagerContent.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "formFillController",
  "@mozilla.org/satchel/form-fill-controller;1",
  Ci.nsIFormFillController
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "SHOULD_SHOW_ORIGIN",
  "signon.showAutoCompleteOrigins"
);

XPCOMUtils.defineLazyGetter(this, "log", () => {
  return LoginHelper.createLogger("LoginAutoCompleteResult");
});
XPCOMUtils.defineLazyGetter(this, "passwordMgrBundle", () => {
  return Services.strings.createBundle(
    "chrome://passwordmgr/locale/passwordmgr.properties"
  );
});

function loginSort(formHostPort, a, b) {
  let maybeHostPortA = LoginHelper.maybeGetHostPortForURL(a.origin);
  let maybeHostPortB = LoginHelper.maybeGetHostPortForURL(b.origin);
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

function getLocalizedString(key, formatArgs = null) {
  if (formatArgs) {
    return passwordMgrBundle.formatStringFromName(key, formatArgs);
  }
  return passwordMgrBundle.GetStringFromName(key);
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

    XPCOMUtils.defineLazyGetter(this, "label", () => {
      let learnMoreString = getLocalizedString("insecureFieldWarningLearnMore");
      return getLocalizedString("insecureFieldWarningDescription2", [
        learnMoreString,
      ]);
    });
  }
}

class LoginAutocompleteItem extends AutocompleteItem {
  constructor(
    login,
    isPasswordField,
    dateAndTimeFormatter,
    duplicateUsernames,
    messageManager
  ) {
    super(SHOULD_SHOW_ORIGIN ? "loginWithOrigin" : "login");
    this._login = login.QueryInterface(Ci.nsILoginMetaInfo);
    this._messageManager = messageManager;

    XPCOMUtils.defineLazyGetter(this, "label", () => {
      let username = login.username;
      // If login is empty or duplicated we want to append a modification date to it.
      if (!username || duplicateUsernames.has(username)) {
        if (!username) {
          username = getLocalizedString("noUsername");
        }
        let time = dateAndTimeFormatter.format(
          new Date(login.timePasswordChanged)
        );
        username = getLocalizedString("loginHostAge", [username, time]);
      }

      return username;
    });

    XPCOMUtils.defineLazyGetter(this, "value", () => {
      return isPasswordField ? login.password : login.username;
    });

    XPCOMUtils.defineLazyGetter(this, "comment", () => {
      return JSON.stringify({
        guid: login.guid,
        comment: login.displayOrigin,
      });
    });
  }

  removeFromStorage() {
    if (this._messageManager) {
      let vanilla = LoginHelper.loginToVanillaObject(this._login);
      this._messageManager.sendAsyncMessage("PasswordManager:removeLogin", {
        login: vanilla,
      });
    } else {
      Services.logins.removeLogin(this._login);
    }
  }
}

class GeneratedPasswordAutocompleteItem extends AutocompleteItem {
  constructor(generatedPassword) {
    super("generatedPassword");
    this.comment = generatedPassword;
    this.value = generatedPassword;

    XPCOMUtils.defineLazyGetter(this, "label", () => {
      return getLocalizedString("useASecurelyGeneratedPassword");
    });
  }
}

class LoginsFooterAutocompleteItem extends AutocompleteItem {
  constructor(hostname) {
    super("loginsFooter");
    this.comment = hostname;

    XPCOMUtils.defineLazyGetter(this, "label", () => {
      return getLocalizedString("viewSavedLogins.label");
    });
  }
}

// nsIAutoCompleteResult implementation
function LoginAutoCompleteResult(
  aSearchString,
  matchingLogins,
  formHostPort,
  { generatedPassword, isSecure, messageManager, isPasswordField, hostname }
) {
  let hidingFooterOnPWFieldAutoOpened = false;
  function isFooterEnabled() {
    // We need to check LoginHelper.enabled here since the insecure warning should
    // appear even if pwmgr is disabled but the footer should never appear in that case.
    if (!LoginHelper.showAutoCompleteFooter || !LoginHelper.enabled) {
      return false;
    }

    // Don't show the footer on non-empty password fields as it's not providing
    // value and only adding noise since a password was already filled.
    if (isPasswordField && aSearchString) {
      log.debug("Hiding footer: non-empty password field");
      return false;
    }

    if (
      !matchingLogins.length &&
      !generatedPassword &&
      isPasswordField &&
      formFillController.passwordPopupAutomaticallyOpened
    ) {
      hidingFooterOnPWFieldAutoOpened = true;
      log.debug(
        "Hiding footer: no logins and the popup was opened upon focus of the pw. field"
      );
      return false;
    }

    return true;
  }

  this.searchString = aSearchString;

  // Build up the array of autocomplete rows to display.
  this._rows = [];

  // Insecure field warning comes first if it applies and is enabled.
  if (!isSecure && LoginHelper.showInsecureFieldWarning) {
    this._rows.push(new InsecureLoginFormAutocompleteItem());
  }

  // Saved login items
  let logins = matchingLogins.sort(loginSort.bind(null, formHostPort));
  let dateAndTimeFormatter = new Services.intl.DateTimeFormat(undefined, {
    dateStyle: "medium",
  });
  let duplicateUsernames = findDuplicates(matchingLogins);
  for (let login of logins) {
    let item = new LoginAutocompleteItem(
      login,
      isPasswordField,
      dateAndTimeFormatter,
      duplicateUsernames,
      messageManager
    );
    this._rows.push(item);
  }

  // The footer comes last if it's enabled
  if (isFooterEnabled()) {
    if (generatedPassword) {
      this._rows.push(new GeneratedPasswordAutocompleteItem(generatedPassword));
    }
    this._rows.push(new LoginsFooterAutocompleteItem(hostname));
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

LoginAutoCompleteResult.prototype = {
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIAutoCompleteResult,
    Ci.nsISupportsWeakReference,
  ]),

  /**
   * Accessed via .wrappedJSObject
   * @private
   */
  get logins() {
    return this._rows
      .filter(item => {
        return item.constructor === LoginAutocompleteItem;
      })
      .map(item => item._login);
  },

  // Allow autoCompleteSearch to get at the JS object so it can
  // modify some readonly properties for internal use.
  get wrappedJSObject() {
    return this;
  },

  // Interfaces from idl...
  searchString: null,
  searchResult: Ci.nsIAutoCompleteResult.RESULT_NOMATCH,
  defaultIndex: -1,
  errorDescription: "",
  get matchCount() {
    return this._rows.length;
  },

  getValueAt(index) {
    if (index < 0 || index >= this.matchCount) {
      throw new Error("Index out of range.");
    }
    return this._rows[index].value;
  },

  getLabelAt(index) {
    if (index < 0 || index >= this.matchCount) {
      throw new Error("Index out of range.");
    }
    return this._rows[index].label;
  },

  getCommentAt(index) {
    if (index < 0 || index >= this.matchCount) {
      throw new Error("Index out of range.");
    }
    return this._rows[index].comment;
  },

  getStyleAt(index) {
    return this._rows[index].style;
  },

  getImageAt(index) {
    return "";
  },

  getFinalCompleteValueAt(index) {
    return this.getValueAt(index);
  },

  removeValueAt(index, removeFromDB) {
    if (index < 0 || index >= this.matchCount) {
      throw new Error("Index out of range.");
    }

    let [removedItem] = this._rows.splice(index, 1);

    if (this.defaultIndex > this._rows.length) {
      this.defaultIndex--;
    }

    if (removeFromDB) {
      removedItem.removeFromStorage();
    }
  },
};

function LoginAutoComplete() {}
LoginAutoComplete.prototype = {
  classID: Components.ID("{2bdac17c-53f1-4896-a521-682ccdeef3a8}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsILoginAutoCompleteSearch]),

  _autoCompleteLookupPromise: null,

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
    // Show the insecure login warning in the passwords field on null principal documents.
    let isSecure = !isNullPrincipal;
    // Avoid loading InsecurePasswordUtils.jsm in a sandboxed document (e.g. an ad. frame) if we
    // already know it has a null principal and will therefore get the insecure autocomplete
    // treatment.
    // InsecurePasswordUtils doesn't handle the null principal case as not secure because we don't
    // want the same treatment:
    // * The web console warnings will be confusing (as they're primarily about http:) and not very
    //   useful if the developer intentionally sandboxed the document.
    // * The site identity insecure field warning would require LoginManagerContent being loaded and
    //   listening to some of the DOM events we're ignoring in null principal documents. For memory
    //   reasons it's better to not load LMC at all for these sandboxed frames. Also, if the top-
    //   document is sandboxing a document, it probably doesn't want that sandboxed document to be
    //   able to affect the identity icon in the address bar by adding a password field.
    if (isSecure) {
      let form = LoginFormFactory.createFromField(aElement);
      isSecure = InsecurePasswordUtils.isFormSecure(form);
    }
    let isPasswordField = aElement.type == "password";
    let hostname = aElement.ownerDocument.documentURIObject.host;

    let completeSearch = (
      autoCompleteLookupPromise,
      { generatedPassword, logins, messageManager }
    ) => {
      // If the search was canceled before we got our
      // results, don't bother reporting them.
      if (this._autoCompleteLookupPromise !== autoCompleteLookupPromise) {
        return;
      }
      let formHostPort = LoginHelper.maybeGetHostPortForURL(
        aElement.ownerDocument.documentURI
      );
      this._autoCompleteLookupPromise = null;
      let results = new LoginAutoCompleteResult(
        aSearchString,
        logins,
        formHostPort,
        {
          generatedPassword,
          messageManager,
          isSecure,
          isPasswordField,
          hostname,
        }
      );
      aCallback.onSearchCompletion(results);
    };

    if (isNullPrincipal) {
      // Don't search login storage when the field has a null principal as we don't want to fill
      // logins for the `location` in this case.
      let acLookupPromise = (this._autoCompleteLookupPromise = Promise.resolve({
        logins: [],
      }));
      acLookupPromise.then(completeSearch.bind(this, acLookupPromise));
      return;
    }

    if (isPasswordField && aSearchString) {
      // Return empty result on password fields with password already filled.
      let acLookupPromise = (this._autoCompleteLookupPromise = Promise.resolve({
        logins: [],
      }));
      acLookupPromise.then(completeSearch.bind(this, acLookupPromise));
      return;
    }

    if (!LoginHelper.enabled) {
      let acLookupPromise = (this._autoCompleteLookupPromise = Promise.resolve({
        logins: [],
      }));
      acLookupPromise.then(completeSearch.bind(this, acLookupPromise));
      return;
    }

    log.debug("AutoCompleteSearch invoked. Search is:", aSearchString);

    let previousResult;
    if (aPreviousResult) {
      previousResult = {
        searchString: aPreviousResult.searchString,
        logins: aPreviousResult.wrappedJSObject.logins,
      };
    } else {
      previousResult = null;
    }

    let acLookupPromise = (this._autoCompleteLookupPromise = LoginManagerContent._autoCompleteSearchAsync(
      aSearchString,
      previousResult,
      aElement
    ));
    acLookupPromise
      .then(completeSearch.bind(this, acLookupPromise))
      .catch(log.error);
  },

  stopSearch() {
    this._autoCompleteLookupPromise = null;
  },
};
