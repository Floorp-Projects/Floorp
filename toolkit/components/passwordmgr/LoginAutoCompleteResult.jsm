/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * nsIAutoCompleteResult and nsILoginAutoCompleteSearch implementations for saved logins.
 */

"use strict";

var EXPORTED_SYMBOLS = [
  "LoginAutoComplete",
  "LoginAutoCompleteResult",
];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "BrowserUtils",
                               "resource://gre/modules/BrowserUtils.jsm");
ChromeUtils.defineModuleGetter(this, "InsecurePasswordUtils",
                               "resource://gre/modules/InsecurePasswordUtils.jsm");
ChromeUtils.defineModuleGetter(this, "LoginFormFactory",
                               "resource://gre/modules/LoginFormFactory.jsm");
ChromeUtils.defineModuleGetter(this, "LoginHelper",
                               "resource://gre/modules/LoginHelper.jsm");
ChromeUtils.defineModuleGetter(this, "LoginManagerContent",
                               "resource://gre/modules/LoginManagerContent.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "formFillController",
                                   "@mozilla.org/satchel/form-fill-controller;1",
                                   Ci.nsIFormFillController);
XPCOMUtils.defineLazyPreferenceGetter(this, "SHOULD_SHOW_ORIGIN",
                                      "signon.showAutoCompleteOrigins");

XPCOMUtils.defineLazyGetter(this, "log", () => {
  return LoginHelper.createLogger("LoginAutoCompleteResult");
});


// nsIAutoCompleteResult implementation
function LoginAutoCompleteResult(aSearchString, matchingLogins, {isSecure, messageManager, isPasswordField, hostname}) {
  function loginSort(a, b) {
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

    if (!matchingLogins.length && isPasswordField && formFillController.passwordPopupAutomaticallyOpened) {
      hidingFooterOnPWFieldAutoOpened = true;
      log.debug("Hiding footer: no logins and the popup was opened upon focus of the pw. field");
      return false;
    }

    return true;
  }

  this._showInsecureFieldWarning = (!isSecure && LoginHelper.showInsecureFieldWarning) ? 1 : 0;
  this._showAutoCompleteFooter = isFooterEnabled() ? 1 : 0;
  this._showOrigin = SHOULD_SHOW_ORIGIN ? 1 : 0;
  this.searchString = aSearchString;
  this.logins = matchingLogins.sort(loginSort);
  this.matchCount = matchingLogins.length + this._showInsecureFieldWarning + this._showAutoCompleteFooter;
  this._messageManager = messageManager;
  this._stringBundle = Services.strings.createBundle("chrome://passwordmgr/locale/passwordmgr.properties");
  this._dateAndTimeFormatter = new Services.intl.DateTimeFormat(undefined, { dateStyle: "medium" });

  this._isPasswordField = isPasswordField;
  this._hostname = hostname;

  this._duplicateUsernames = findDuplicates(matchingLogins);

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
  QueryInterface: ChromeUtils.generateQI([Ci.nsIAutoCompleteResult,
                                          Ci.nsISupportsWeakReference]),

  // private
  logins: null,

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
  matchCount: 0,

  getValueAt(index) {
    if (index < 0 || index >= this.matchCount) {
      throw new Error("Index out of range.");
    }

    if (this._showInsecureFieldWarning && index === 0) {
      return "";
    }

    if (this._showAutoCompleteFooter && index === this.matchCount - 1) {
      return "";
    }

    let selectedLogin = this.logins[index - this._showInsecureFieldWarning];

    return this._isPasswordField ? selectedLogin.password : selectedLogin.username;
  },

  getLabelAt(index) {
    if (index < 0 || index >= this.matchCount) {
      throw new Error("Index out of range.");
    }

    let getLocalizedString = (key, formatArgs = null) => {
      if (formatArgs) {
        return this._stringBundle.formatStringFromName(key, formatArgs, formatArgs.length);
      }
      return this._stringBundle.GetStringFromName(key);
    };

    if (this._showInsecureFieldWarning && index === 0) {
      let learnMoreString = getLocalizedString("insecureFieldWarningLearnMore");
      return getLocalizedString("insecureFieldWarningDescription2", [learnMoreString]);
    } else if (this._showAutoCompleteFooter && index === this.matchCount - 1) {
      return JSON.stringify({
        label: getLocalizedString("viewSavedLogins.label"),
        hostname: this._hostname,
      });
    }

    let login = this.logins[index - this._showInsecureFieldWarning];
    let username = login.username;
    // If login is empty or duplicated we want to append a modification date to it.
    if (!username || this._duplicateUsernames.has(username)) {
      if (!username) {
        username = getLocalizedString("noUsername");
      }
      let meta = login.QueryInterface(Ci.nsILoginMetaInfo);
      let time = this._dateAndTimeFormatter.format(new Date(meta.timePasswordChanged));
      username = getLocalizedString("loginHostAge", [username, time]);
    }

    return username;
  },

  getCommentAt(index) {
    if (this._showInsecureFieldWarning && index === 0) {
      return "";
    }

    if (this._showAutoCompleteFooter && index === this.matchCount - 1) {
      return "";
    }

    let login = this.logins[index - this._showInsecureFieldWarning];
    return JSON.stringify({
      loginOrigin: login.hostname,
    });
  },

  getStyleAt(index) {
    if (index == 0 && this._showInsecureFieldWarning) {
      return "insecureWarning";
    } else if (this._showAutoCompleteFooter && index == this.matchCount - 1) {
      return "loginsFooter";
    } else if (this._showOrigin) {
      return "loginWithOrigin";
    }

    return "login";
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

    if (this._showInsecureFieldWarning && index === 0) {
      // Ignore the warning message item.
      return;
    }

    if (this._showInsecureFieldWarning) {
      index--;
    }

    // The user cannot delete the autocomplete footer.
    if (this._showAutoCompleteFooter && index === this.matchCount - 1) {
      return;
    }

    let [removedLogin] = this.logins.splice(index, 1);

    this.matchCount--;
    if (this.defaultIndex > this.logins.length) {
      this.defaultIndex--;
    }

    if (removeFromDB) {
      if (this._messageManager) {
        let vanilla = LoginHelper.loginToVanillaObject(removedLogin);
        this._messageManager.sendAsyncMessage("PasswordManager:removeLogin",
                                              { login: vanilla });
      } else {
        Services.logins.removeLogin(removedLogin);
      }
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
    let {isNullPrincipal} = aElement.nodePrincipal;
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

    let completeSearch = (autoCompleteLookupPromise, { logins, messageManager }) => {
      // If the search was canceled before we got our
      // results, don't bother reporting them.
      if (this._autoCompleteLookupPromise !== autoCompleteLookupPromise) {
        return;
      }

      this._autoCompleteLookupPromise = null;
      let results = new LoginAutoCompleteResult(aSearchString, logins, {
        messageManager,
        isSecure,
        isPasswordField,
        hostname,
      });
      aCallback.onSearchCompletion(results);
    };

    if (isNullPrincipal) {
      // Don't search login storage when the field has a null principal as we don't want to fill
      // logins for the `location` in this case.
      let acLookupPromise = this._autoCompleteLookupPromise = Promise.resolve({ logins: [] });
      acLookupPromise.then(completeSearch.bind(this, acLookupPromise));
      return;
    }

    if (isPasswordField && aSearchString) {
      // Return empty result on password fields with password already filled.
      let acLookupPromise = this._autoCompleteLookupPromise = Promise.resolve({ logins: [] });
      acLookupPromise.then(completeSearch.bind(this, acLookupPromise));
      return;
    }

    if (!LoginHelper.enabled) {
      let acLookupPromise = this._autoCompleteLookupPromise = Promise.resolve({ logins: [] });
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

    let rect = BrowserUtils.getElementBoundingScreenRect(aElement);
    let acLookupPromise = this._autoCompleteLookupPromise =
      LoginManagerContent._autoCompleteSearchAsync(aSearchString, previousResult,
                                                   aElement, rect);
    acLookupPromise.then(completeSearch.bind(this, acLookupPromise)).catch(log.error);
  },

  stopSearch() {
    this._autoCompleteLookupPromise = null;
  },
};
