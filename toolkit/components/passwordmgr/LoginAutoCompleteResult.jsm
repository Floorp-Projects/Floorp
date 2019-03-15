/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * nsIAutoCompleteResult implementation for saved logins.
 */

"use strict";

var EXPORTED_SYMBOLS = ["LoginAutoCompleteResult"];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "LoginHelper",
                               "resource://gre/modules/LoginHelper.jsm");

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let logger = LoginHelper.createLogger("LoginAutoCompleteResult");
  return logger.log.bind(logger);
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

  this._showInsecureFieldWarning = (!isSecure && LoginHelper.showInsecureFieldWarning) ? 1 : 0;
  this._showAutoCompleteFooter = 0;
  // We need to check LoginHelper.enabled here since the insecure warning should
  // appear even if pwmgr is disabled but the footer should never appear in that case.
  if (LoginHelper.showAutoCompleteFooter && LoginHelper.enabled) {
    // Don't show the footer on non-empty password fields as it's not providing
    // value and only adding noise since a password was already filled.
    this._showAutoCompleteFooter = (!isPasswordField || !aSearchString) ? 1 : 0;
  }
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
    return "";
  },

  getStyleAt(index) {
    if (index == 0 && this._showInsecureFieldWarning) {
      return "insecureWarning";
    } else if (this._showAutoCompleteFooter && index == this.matchCount - 1) {
      return "loginsFooter";
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
