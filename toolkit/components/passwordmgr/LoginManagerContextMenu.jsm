/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["LoginManagerContextMenu"];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LoginHelper",
                                  "resource://gre/modules/LoginHelper.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LoginManagerParent",
                                  "resource://gre/modules/LoginManagerParent.jsm");

/*
 * Password manager object for the browser contextual menu.
 */
var LoginManagerContextMenu = {
  /**
   * Look for login items and add them to the contextual menu.
   *
   * @param {HTMLInputElement} inputElement
   *        The target input element of the context menu click.
   * @param {xul:browser} browser
   *        The browser for the document the context menu was open on.
   * @param {nsIURI} documentURI
   *        The URI of the document that the context menu was activated from.
   *        This isn't the same as the browser's top-level document URI
   *        when subframes are involved.
   * @returns {DocumentFragment} a document fragment with all the login items.
   */
  addLoginsToMenu(inputElement, browser, documentURI) {
    let foundLogins = this._findLogins(documentURI);

    if (!foundLogins.length) {
      return null;
    }

    let fragment = browser.ownerDocument.createDocumentFragment();
    let duplicateUsernames = this._findDuplicates(foundLogins);
    for (let login of foundLogins) {
        let item = fragment.ownerDocument.createElement("menuitem");

        let username = login.username;
        // If login is empty or duplicated we want to append a modification date to it.
        if (!username || duplicateUsernames.has(username)) {
          if (!username) {
            username = this._getLocalizedString("noUsername");
          }
          let meta = login.QueryInterface(Ci.nsILoginMetaInfo);
          let time = this.dateAndTimeFormatter.format(new Date(meta.timePasswordChanged));
          username = this._getLocalizedString("loginHostAge", [username, time]);
        }
        item.setAttribute("label", username);
        item.setAttribute("class", "context-login-item");

        // login is bound so we can keep the reference to each object.
        item.addEventListener("command", function(login, event) {
          this._fillTargetField(login, inputElement, browser, documentURI);
        }.bind(this, login));

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
   * Find logins for the current URI.
   *
   * @param {nsIURI} documentURI
   *        URI object with the hostname of the logins we want to find.
   *        This isn't the same as the browser's top-level document URI
   *        when subframes are involved.
   *
   * @returns {nsILoginInfo[]} a login list
   */
  _findLogins(documentURI) {
    let searchParams = {
      hostname: documentURI.displayPrePath,
      schemeUpgrades: LoginHelper.schemeUpgrades,
    };
    let logins = LoginHelper.searchLoginsWithObject(searchParams);
    let resolveBy = [
      "scheme",
      "timePasswordChanged",
    ];
    logins = LoginHelper.dedupeLogins(logins, ["username", "password"], resolveBy, documentURI.displayPrePath);

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
   * @param {Element} inputElement
   *        The target input element we want to fill.
   * @param {xul:browser} browser
   *        The target tab browser.
   * @param {nsIURI} documentURI
   *        URI of the document owning the form we want to fill.
   *        This isn't the same as the browser's top-level
   *        document URI when subframes are involved.
   */
  _fillTargetField(login, inputElement, browser, documentURI) {
    LoginManagerParent.fillForm({
      browser,
      loginFormOrigin: documentURI.displayPrePath,
      login,
      inputElement,
    }).catch(Cu.reportError);
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
      return this._stringBundle.formatStringFromName(key, formatArgs, formatArgs.length);
    }
    return this._stringBundle.GetStringFromName(key);
  },
};

XPCOMUtils.defineLazyGetter(LoginManagerContextMenu, "_stringBundle", function() {
  return Services.strings.
         createBundle("chrome://passwordmgr/locale/passwordmgr.properties");
});

XPCOMUtils.defineLazyGetter(LoginManagerContextMenu, "dateAndTimeFormatter", function() {
  return new Services.intl.DateTimeFormat(undefined, {
    dateStyle: "medium"
  });
});
