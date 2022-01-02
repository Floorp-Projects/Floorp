/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["LoginManagerContextMenu"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "LoginHelper",
  "resource://gre/modules/LoginHelper.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "LoginManagerParent",
  "resource://gre/modules/LoginManagerParent.jsm"
);
XPCOMUtils.defineLazyGetter(this, "log", () => {
  return LoginHelper.createLogger("LoginManagerContextMenu");
});

/**
 * Password manager object for the browser contextual menu.
 */
this.LoginManagerContextMenu = {
  /**
   * Look for login items and add them to the contextual menu.
   *
   * @param {Object} inputElementIdentifier
   *        An identifier generated for the input element via ContentDOMReference.
   * @param {xul:browser} browser
   *        The browser for the document the context menu was open on.
   * @param {string} formOrigin
   *        The origin of the document that the context menu was activated from.
   *        This isn't the same as the browser's top-level document origin
   *        when subframes are involved.
   * @returns {DocumentFragment} a document fragment with all the login items.
   */
  addLoginsToMenu(inputElementIdentifier, browser, formOrigin) {
    let foundLogins = this._findLogins(formOrigin);

    if (!foundLogins.length) {
      return null;
    }

    let fragment = browser.ownerDocument.createDocumentFragment();
    let duplicateUsernames = this._findDuplicates(foundLogins);
    for (let login of foundLogins) {
      let item = fragment.ownerDocument.createXULElement("menuitem");

      let username = login.username;
      // If login is empty or duplicated we want to append a modification date to it.
      if (!username || duplicateUsernames.has(username)) {
        if (!username) {
          username = this._getLocalizedString("noUsername");
        }
        let meta = login.QueryInterface(Ci.nsILoginMetaInfo);
        let time = this.dateAndTimeFormatter.format(
          new Date(meta.timePasswordChanged)
        );
        username = this._getLocalizedString("loginHostAge", [username, time]);
      }
      item.setAttribute("label", username);
      item.setAttribute("class", "context-login-item");

      // login is bound so we can keep the reference to each object.
      item.addEventListener(
        "command",
        function(login, event) {
          this._fillTargetField(
            login,
            inputElementIdentifier,
            browser,
            formOrigin
          );
        }.bind(this, login)
      );

      fragment.appendChild(item);
    }

    return fragment;
  },

  /**
   * Undoes the work of addLoginsToMenu for the same menu.
   *
   * @param {Document}
   *        The context menu owner document.
   */
  clearLoginsFromMenu(document) {
    let loginItems = document.getElementsByClassName("context-login-item");
    while (loginItems.item(0)) {
      loginItems.item(0).remove();
    }
  },

  /**
   * Show the password autocomplete UI with the generation option forced to appear.
   */
  async useGeneratedPassword(inputElementIdentifier, documentURI, browser) {
    let browsingContextId = inputElementIdentifier.browsingContextId;
    let browsingContext = BrowsingContext.get(browsingContextId);
    let actor = browsingContext.currentWindowGlobal.getActor("LoginManager");

    actor.sendAsyncMessage("PasswordManager:useGeneratedPassword", {
      inputElementIdentifier,
    });
  },

  /**
   * Find logins for the specified origin..
   *
   * @param {string} formOrigin
   *        Origin of the logins we want to find that has be sanitized by `getLoginOrigin`.
   *        This isn't the same as the browser's top-level document URI
   *        when subframes are involved.
   *
   * @returns {nsILoginInfo[]} a login list
   */
  _findLogins(formOrigin) {
    let searchParams = {
      origin: formOrigin,
      schemeUpgrades: LoginHelper.schemeUpgrades,
    };
    let logins = LoginHelper.searchLoginsWithObject(searchParams);
    let resolveBy = ["scheme", "timePasswordChanged"];
    logins = LoginHelper.dedupeLogins(
      logins,
      ["username", "password"],
      resolveBy,
      formOrigin
    );

    // Sort logins in alphabetical order and by date.
    logins.sort((loginA, loginB) => {
      // Sort alphabetically
      let result = loginA.username.localeCompare(loginB.username);
      if (result) {
        // Forces empty logins to be at the end
        if (!loginA.username) {
          return 1;
        }
        if (!loginB.username) {
          return -1;
        }
        return result;
      }

      // Same username logins are sorted by last change date
      let metaA = loginA.QueryInterface(Ci.nsILoginMetaInfo);
      let metaB = loginB.QueryInterface(Ci.nsILoginMetaInfo);
      return metaB.timePasswordChanged - metaA.timePasswordChanged;
    });

    return logins;
  },

  /**
   * Find duplicate usernames in a login list.
   *
   * @param {nsILoginInfo[]} loginList
   *        A list of logins we want to look for duplicate usernames.
   *
   * @returns {Set} a set with the duplicate usernames.
   */
  _findDuplicates(loginList) {
    let seen = new Set();
    let duplicates = new Set();
    for (let login of loginList) {
      if (seen.has(login.username)) {
        duplicates.add(login.username);
      }
      seen.add(login.username);
    }
    return duplicates;
  },

  /**
   * @param {nsILoginInfo} login
   *        The login we want to fill the form with.
   * @param {Object} inputElementIdentifier
   *        An identifier generated for the input element via ContentDOMReference.
   * @param {xul:browser} browser
   *        The target tab browser.
   * @param {string} formOrigin
   *        Origin of the document we're filling after sanitization via
   *        `getLoginOrigin`.
   *        This isn't the same as the browser's top-level
   *        origin when subframes are involved.
   */
  _fillTargetField(login, inputElementIdentifier, browser, formOrigin) {
    let browsingContextId = inputElementIdentifier.browsingContextId;
    let browsingContext = BrowsingContext.get(browsingContextId);
    if (!browsingContext) {
      return;
    }

    let actor = browsingContext.currentWindowGlobal.getActor("LoginManager");
    if (!actor) {
      return;
    }

    actor
      .fillForm({
        browser,
        inputElementIdentifier,
        loginFormOrigin: formOrigin,
        login,
      })
      .catch(Cu.reportError);
  },

  /**
   * @param {string} key
   *        The localized string key
   * @param {string[]} formatArgs
   *        An array of formatting argument string
   *
   * @returns {string} the localized string for the specified key,
   *          formatted with arguments if required.
   */
  _getLocalizedString(key, formatArgs) {
    if (formatArgs) {
      return this._stringBundle.formatStringFromName(key, formatArgs);
    }
    return this._stringBundle.GetStringFromName(key);
  },
};

XPCOMUtils.defineLazyGetter(
  LoginManagerContextMenu,
  "_stringBundle",
  function() {
    return Services.strings.createBundle(
      "chrome://passwordmgr/locale/passwordmgr.properties"
    );
  }
);

XPCOMUtils.defineLazyGetter(
  LoginManagerContextMenu,
  "dateAndTimeFormatter",
  function() {
    return new Services.intl.DateTimeFormat(undefined, {
      dateStyle: "medium",
    });
  }
);
