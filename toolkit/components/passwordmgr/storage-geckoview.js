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

ChromeUtils.defineModuleGetter(
  this,
  "LoginHelper",
  "resource://gre/modules/LoginHelper.jsm"
);

class LoginManagerStorage_geckoview extends LoginManagerStorage_json {
  get classID() {
    return Components.ID("{337f317f-f713-452a-962d-db831c785fec}");
  }
  get QueryInterface() {
    return ChromeUtils.generateQI([Ci.nsILoginManagerStorage]);
  }

  get _xpcom_factory() {
    return XPCOMUtils.generateSingletonFactory(
      this.LoginManagerStorage_geckoview
    );
  }

  get _crypto() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
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
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  }

  removeLogin(login) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  }

  modifyLogin(oldLogin, newLoginData) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  }

  getAllLogins() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  }

  /**
   * Returns an array of all saved logins that can be decrypted.
   *
   * @resolve {nsILoginInfo[]}
   */
  async getAllLoginsAsync() {
    let [logins, ids] = this._searchLogins({});
    if (!logins.length) {
      return [];
    }

    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  }

  async searchLoginsAsync(matchData) {
    this.log("searchLoginsAsync:", matchData);

    await Promise.resolve();

    let realMatchData = {};
    let options = {};
    for (let [name, value] of Object.entries(matchData)) {
      switch (name) {
        // Some property names aren't field names but are special options to affect the search.
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

    // TODO: get from GV
    let candidateLogins = [];
    let [logins, ids] = this._searchLogins(
      realMatchData,
      options,
      candidateLogins
    );
    return logins;
  }

  /**
   * Use `searchLoginsAsync` instead.
   */
  searchLogins(matchData) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  }

  /**
   * Removes all logins from storage.
   */
  removeAllLogins() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
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
   */
  _decryptLogins(logins) {
    return logins;
  }
}

XPCOMUtils.defineLazyGetter(
  LoginManagerStorage_geckoview.prototype,
  "log",
  () => {
    let logger = LoginHelper.createLogger("Login storage");
    return logger.log.bind(logger);
  }
);

const EXPORTED_SYMBOLS = ["LoginManagerStorage_geckoview"];
