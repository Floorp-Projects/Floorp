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
   * @param {string} formActionOrigin
   *        The origin of the LoginForm's action.
   * @returns {DocumentFragment} a document fragment with all the login items.
   */
  addLoginsToMenu(
    inputElementIdentifier,
    browser,
    formOrigin,
    formActionOrigin
  ) {
    let foundLogins = this._findLogins(formOrigin, formActionOrigin);

    if (!foundLogins.length) {
      return null;
    }

    let fragment = browser.ownerDocument.createDocumentFragment();
    let duplicateUsernames = this._findDuplicates(foundLogins);
    // Default `lastDisplayOrigin` to the hostPort of the form so that we don't
    // show a menucaption above logins that are direct matches for this document.
    let lastDisplayOrigin = LoginHelper.maybeGetHostPortForURL(formOrigin);
    let lastMenuCaption = null;
    for (let login of foundLogins) {
      // Add a section header containing the displayOrigin above logins that
      // aren't matches for the form's origin.
      if (lastDisplayOrigin != login.displayOrigin) {
        if (fragment.children.length) {
          let menuSeparator = fragment.ownerDocument.createXULElement(
            "menuseparator"
          );
          menuSeparator.className = "context-login-item";
          fragment.appendChild(menuSeparator);
        }

        lastMenuCaption = fragment.ownerDocument.createXULElement(
          "menucaption"
        );
        lastMenuCaption.setAttribute("role", "group");
        lastMenuCaption.label = login.displayOrigin;
        lastMenuCaption.className = "context-login-item";

        fragment.appendChild(lastMenuCaption);
      }

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
      item.id = "login-" + login.guid;
      item.setAttribute("label", username);
      item.setAttribute("class", "context-login-item");
      if (lastMenuCaption) {
        item.setAttribute("aria-level", "2");
        lastMenuCaption.setAttribute(
          "aria-owns",
          lastMenuCaption.getAttribute("aria-owns") + item.id + " "
        );
      }

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
      lastDisplayOrigin = login.displayOrigin;
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

  loginSort(formHostPort, a, b) {
    let maybeHostPortA = LoginHelper.maybeGetHostPortForURL(a.origin);
    let maybeHostPortB = LoginHelper.maybeGetHostPortForURL(b.origin);

    // Exact hostPort matches should appear first.
    if (formHostPort == maybeHostPortA && formHostPort != maybeHostPortB) {
      return -1;
    }
    if (formHostPort != maybeHostPortA && formHostPort == maybeHostPortB) {
      return 1;
    }

    // Next sort by displayOrigin (which contains the httpRealm)
    if (a.displayOrigin !== b.displayOrigin) {
      return a.displayOrigin.localeCompare(b.displayOrigin);
    }

    // Finally sort by username within the displayOrigin.
    return a.username.localeCompare(b.username);
  },

  /**
   * Find logins for the specified origin.
   *
   * @param {string} formOrigin
   *        Origin of the logins we want to find that has be sanitized by `getLoginOrigin`.
   *        This isn't the same as the browser's top-level document URI
   *        when subframes are involved.
   * @param {string} formActionOrigin
   *
   * @returns {nsILoginInfo[]} a login list
   */
  _findLogins(formOrigin, formActionOrigin) {
    let searchParams = {
      acceptDifferentSubdomains: LoginHelper.includeOtherSubdomainsInLookup,
      formActionOrigin,
      ignoreActionAndRealm: true,
    };

    let logins = LoginManagerParent.searchAndDedupeLogins(
      formOrigin,
      searchParams
    );

    let formHostPort = LoginHelper.maybeGetHostPortForURL(formOrigin);
    logins.sort(this.loginSort.bind(null, formHostPort));
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
