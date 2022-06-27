/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * nsILoginManagerStorage implementation for GeckoView
 */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { LoginManagerStorage_json } = ChromeUtils.import(
  "resource://gre/modules/storage-json.js"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  GeckoViewAutocomplete: "resource://gre/modules/GeckoViewAutocomplete.jsm",
  LoginHelper: "resource://gre/modules/LoginHelper.jsm",
  LoginEntry: "resource://gre/modules/GeckoViewAutocomplete.jsm",
});

class LoginManagerStorage_geckoview extends LoginManagerStorage_json {
  get classID() {
    return Components.ID("{337f317f-f713-452a-962d-db831c785fec}");
  }
  get QueryInterface() {
    return ChromeUtils.generateQI(["nsILoginManagerStorage"]);
  }

  get _crypto() {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  initialize() {
    try {
      return Promise.resolve();
    } catch (e) {
      this.log("Initialization failed:", e);
      throw new Error("Initialization failed");
    }
  }

  /**
   * Internal method used by regression tests only.  It is called before
   * replacing this storage module with a new instance.
   */
  terminate() {}

  addLogin(
    login,
    preEncrypted = false,
    plaintextUsername = null,
    plaintextPassword = null
  ) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  removeLogin(login) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  modifyLogin(oldLogin, newLoginData) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  recordPasswordUse(login) {
    lazy.GeckoViewAutocomplete.onLoginPasswordUsed(
      lazy.LoginEntry.fromLoginInfo(login)
    );
  }

  getAllLogins() {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  /**
   * Returns an array of all saved logins that can be decrypted.
   *
   * @resolve {nsILoginInfo[]}
   */
  async getAllLoginsAsync() {
    return this._getLoginsAsync({});
  }

  async searchLoginsAsync(matchData) {
    this.log("searchLoginsAsync:", matchData);
    return this._getLoginsAsync(matchData);
  }

  _baseHostnameFromOrigin(origin) {
    if (!origin) {
      return null;
    }

    let originURI = Services.io.newURI(origin);
    try {
      return Services.eTLD.getBaseDomain(originURI);
    } catch (ex) {
      if (ex.result == Cr.NS_ERROR_HOST_IS_IP_ADDRESS) {
        // `getBaseDomain` cannot handle IP addresses and `nsIURI` cannot return
        // IPv6 hostnames with the square brackets so use `URL.hostname`.
        return new URL(origin).hostname;
      } else if (ex.result == Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
        return originURI.asciiHost;
      }
      throw ex;
    }
  }

  async _getLoginsAsync(matchData) {
    let baseHostname = this._baseHostnameFromOrigin(matchData.origin);

    // Query all logins for the eTLD+1 and then filter the logins in _searchLogins
    // so that we can handle the logic for scheme upgrades, subdomains, etc.
    // Convert from the new shape to one which supports the legacy getters used
    // by _searchLogins.
    let candidateLogins = await lazy.GeckoViewAutocomplete.fetchLogins(
      baseHostname
    ).catch(_ => {
      // No GV delegate is attached.
    });

    if (!candidateLogins) {
      // May be undefined if there is no delegate attached to handle the request.
      // Ignore the request.
      return [];
    }

    let realMatchData = {};
    let options = {};

    if (matchData.guid) {
      // Enforce GUID-based filtering when available, since the origin of the
      // login may not match the origin of the form in the case of scheme
      // upgrades.
      realMatchData = { guid: matchData.guid };
    } else {
      for (let [name, value] of Object.entries(matchData)) {
        switch (name) {
          // Some property names aren't field names but are special options to
          // affect the search.
          case "acceptDifferentSubdomains":
          case "schemeUpgrades": {
            options[name] = value;
            break;
          }
          default: {
            realMatchData[name] = value;
            break;
          }
        }
      }
    }

    const [logins] = this._searchLogins(
      realMatchData,
      options,
      candidateLogins.map(this._vanillaLoginToStorageLogin)
    );
    return logins;
  }

  /**
   * Convert a modern decrypted vanilla login object to one expected from logins.json.
   *
   * The storage login is usually encrypted but not in this case, this aligns
   * with the `_decryptLogins` method being a no-op.
   *
   * @param {object} vanillaLogin using `origin`/`formActionOrigin`/`username` properties.
   * @returns {object} a vanilla login for logins.json using
   *                   `hostname`/`formSubmitURL`/`encryptedUsername`.
   */
  _vanillaLoginToStorageLogin(vanillaLogin) {
    return {
      ...vanillaLogin,
      hostname: vanillaLogin.origin,
      formSubmitURL: vanillaLogin.formActionOrigin,
      encryptedUsername: vanillaLogin.username,
      encryptedPassword: vanillaLogin.password,
    };
  }

  /**
   * Use `searchLoginsAsync` instead.
   */
  searchLogins(matchData) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  /**
   * Removes all logins from storage.
   */
  removeAllLogins() {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  countLogins(origin, formActionOrigin, httpRealm) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  get uiBusy() {
    return false;
  }

  get isLoggedIn() {
    return true;
  }

  /**
   * GeckoView will encrypt the login itself.
   */
  _encryptLogin(login) {
    return login;
  }

  /**
   * GeckoView logins are already decrypted before this component receives them
   * so this method is a no-op for this backend.
   * @see _vanillaLoginToStorageLogin
   */
  _decryptLogins(logins) {
    return logins;
  }

  /**
   * Sync metadata, which isn't supported by GeckoView.
   */
  async getSyncID() {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  async setSyncID(syncID) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  async getLastSync() {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  async setLastSync(timestamp) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }
}

XPCOMUtils.defineLazyGetter(
  LoginManagerStorage_geckoview.prototype,
  "log",
  () => {
    let logger = lazy.LoginHelper.createLogger("Login storage");
    return logger.log.bind(logger);
  }
);

const EXPORTED_SYMBOLS = ["LoginManagerStorage_geckoview"];
