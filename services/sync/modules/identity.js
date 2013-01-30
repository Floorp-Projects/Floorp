/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["IdentityManager"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-sync/util.js");

// Lazy import to prevent unnecessary load on startup.
for (let symbol of ["BulkKeyBundle", "SyncKeyBundle"]) {
  XPCOMUtils.defineLazyModuleGetter(this, symbol,
                                    "resource://services-sync/keys.js",
                                    symbol);
}

/**
 * Manages identity and authentication for Sync.
 *
 * The following entities are managed:
 *
 *   account - The main Sync/services account. This is typically an email
 *     address.
 *   username - A normalized version of your account. This is what's
 *     transmitted to the server.
 *   basic password - UTF-8 password used for authenticating when using HTTP
 *     basic authentication.
 *   sync key - The main encryption key used by Sync.
 *   sync key bundle - A representation of your sync key.
 *
 * When changes are made to entities that are stored in the password manager
 * (basic password, sync key), those changes are merely staged. To commit them
 * to the password manager, you'll need to call persistCredentials().
 *
 * This type also manages authenticating Sync's network requests. Sync's
 * network code calls into getRESTRequestAuthenticator and
 * getResourceAuthenticator (depending on the network layer being used). Each
 * returns a function which can be used to add authentication information to an
 * outgoing request.
 *
 * In theory, this type supports arbitrary identity and authentication
 * mechanisms. You can add support for them by monkeypatching the global
 * instance of this type. Specifically, you'll need to redefine the
 * aforementioned network code functions to do whatever your authentication
 * mechanism needs them to do. In addition, you may wish to install custom
 * functions to support your API. Although, that is certainly not required.
 * If you do monkeypatch, please be advised that Sync expects the core
 * attributes to have values. You will need to carry at least account and
 * username forward. If you do not wish to support one of the built-in
 * authentication mechanisms, you'll probably want to redefine currentAuthState
 * and any other function that involves the built-in functionality.
 */
this.IdentityManager = function IdentityManager() {
  this._log = Log4Moz.repository.getLogger("Sync.Identity");
  this._log.Level = Log4Moz.Level[Svc.Prefs.get("log.logger.identity")];

  this._basicPassword = null;
  this._basicPasswordAllowLookup = true;
  this._basicPasswordUpdated = false;
  this._syncKey = null;
  this._syncKeyAllowLookup = true;
  this._syncKeySet = false;
  this._syncKeyBundle = null;
}
IdentityManager.prototype = {
  _log: null,

  _basicPassword: null,
  _basicPasswordAllowLookup: true,
  _basicPasswordUpdated: false,

  _syncKey: null,
  _syncKeyAllowLookup: true,
  _syncKeySet: false,

  _syncKeyBundle: null,

  get account() {
    return Svc.Prefs.get("account", this.username);
  },

  /**
   * Sets the active account name.
   *
   * This should almost always be called in favor of setting username, as
   * username is derived from account.
   *
   * Changing the account name has the side-effect of wiping out stored
   * credentials. Keep in mind that persistCredentials() will need to be called
   * to flush the changes to disk.
   *
   * Set this value to null to clear out identity information.
   */
  set account(value) {
    if (value) {
      value = value.toLowerCase();
      Svc.Prefs.set("account", value);
    } else {
      Svc.Prefs.reset("account");
    }

    this.username = this.usernameFromAccount(value);
  },

  get username() {
    return Svc.Prefs.get("username", null);
  },

  /**
   * Set the username value.
   *
   * Changing the username has the side-effect of wiping credentials.
   */
  set username(value) {
    if (value) {
      value = value.toLowerCase();

      if (value == this.username) {
        return;
      }

      Svc.Prefs.set("username", value);
    } else {
      Svc.Prefs.reset("username");
    }

    // If we change the username, we interpret this as a major change event
    // and wipe out the credentials.
    this._log.info("Username changed. Removing stored credentials.");
    this.basicPassword = null;
    this.syncKey = null;
    // syncKeyBundle cleared as a result of setting syncKey.
  },

  /**
   * Obtains the HTTP Basic auth password.
   *
   * Returns a string if set or null if it is not set.
   */
  get basicPassword() {
    if (this._basicPasswordAllowLookup) {
      // We need a username to find the credentials.
      let username = this.username;
      if (!username) {
        return null;
      }

      for each (let login in this._getLogins(PWDMGR_PASSWORD_REALM)) {
        if (login.username.toLowerCase() == username) {
          // It should already be UTF-8 encoded, but we don't take any chances.
          this._basicPassword = Utils.encodeUTF8(login.password);
        }
      }

      this._basicPasswordAllowLookup = false;
    }

    return this._basicPassword;
  },

  /**
   * Set the HTTP basic password to use.
   *
   * Changes will not persist unless persistSyncCredentials() is called.
   */
  set basicPassword(value) {
    // Wiping out value.
    if (!value) {
      this._log.info("Basic password has no value. Removing.");
      this._basicPassword = null;
      this._basicPasswordUpdated = true;
      this._basicPasswordAllowLookup = false;
      return;
    }

    let username = this.username;
    if (!username) {
      throw new Error("basicPassword cannot be set before username.");
    }

    this._log.info("Basic password being updated.");
    this._basicPassword = Utils.encodeUTF8(value);
    this._basicPasswordUpdated = true;
  },

  /**
   * Obtain the Sync Key.
   *
   * This returns a 26 character "friendly" Base32 encoded string on success or
   * null if no Sync Key could be found.
   *
   * If the Sync Key hasn't been set in this session, this will look in the
   * password manager for the sync key.
   */
  get syncKey() {
    if (this._syncKeyAllowLookup) {
      let username = this.username;
      if (!username) {
        return null;
      }

      for each (let login in this._getLogins(PWDMGR_PASSPHRASE_REALM)) {
        if (login.username.toLowerCase() == username) {
          this._syncKey = login.password;
        }
      }

      this._syncKeyAllowLookup = false;
    }

    return this._syncKey;
  },

  /**
   * Set the active Sync Key.
   *
   * If being set to null, the Sync Key and its derived SyncKeyBundle are
   * removed. However, the Sync Key won't be deleted from the password manager
   * until persistSyncCredentials() is called.
   *
   * If a value is provided, it should be a 26 or 32 character "friendly"
   * Base32 string for which Utils.isPassphrase() returns true.
   *
   * A side-effect of setting the Sync Key is that a SyncKeyBundle is
   * generated. For historical reasons, this will silently error out if the
   * value is not a proper Sync Key (!Utils.isPassphrase()). This should be
   * fixed in the future (once service.js is more sane) to throw if the passed
   * value is not valid.
   */
  set syncKey(value) {
    if (!value) {
      this._log.info("Sync Key has no value. Deleting.");
      this._syncKey = null;
      this._syncKeyBundle = null;
      this._syncKeyUpdated = true;
      return;
    }

    if (!this.username) {
      throw new Error("syncKey cannot be set before username.");
    }

    this._log.info("Sync Key being updated.");
    this._syncKey = value;

    // Clear any cached Sync Key Bundle and regenerate it.
    this._syncKeyBundle = null;
    let bundle = this.syncKeyBundle;

    this._syncKeyUpdated = true;
  },

  /**
   * Obtain the active SyncKeyBundle.
   *
   * This returns a SyncKeyBundle representing a key pair derived from the
   * Sync Key on success. If no Sync Key is present or if the Sync Key is not
   * valid, this returns null.
   *
   * The SyncKeyBundle should be treated as immutable.
   */
  get syncKeyBundle() {
    // We can't obtain a bundle without a username set.
    if (!this.username) {
      this._log.warn("Attempted to obtain Sync Key Bundle with no username set!");
      return null;
    }

    if (!this.syncKey) {
      this._log.warn("Attempted to obtain Sync Key Bundle with no Sync Key " +
                     "set!");
      return null;
    }

    if (!this._syncKeyBundle) {
      try {
        this._syncKeyBundle = new SyncKeyBundle(this.username, this.syncKey);
      } catch (ex) {
        this._log.warn(Utils.exceptionStr(ex));
        return null;
      }
    }

    return this._syncKeyBundle;
  },

  /**
   * The current state of the auth credentials.
   *
   * This essentially validates that enough credentials are available to use
   * Sync.
   */
  get currentAuthState() {
    if (!this.username) {
      return LOGIN_FAILED_NO_USERNAME;
    }

    if (Utils.mpLocked()) {
      return STATUS_OK;
    }

    if (!this.basicPassword) {
      return LOGIN_FAILED_NO_PASSWORD;
    }

    if (!this.syncKey) {
      return LOGIN_FAILED_NO_PASSPHRASE;
    }

    // If we have a Sync Key but no bundle, bundle creation failed, which
    // implies a bad Sync Key.
    if (!this.syncKeyBundle) {
      return LOGIN_FAILED_INVALID_PASSPHRASE;
    }

    return STATUS_OK;
  },

  /**
   * Persist credentials to password store.
   *
   * When credentials are updated, they are changed in memory only. This will
   * need to be called to save them to the underlying password store.
   *
   * If the password store is locked (e.g. if the master password hasn't been
   * entered), this could throw an exception.
   */
  persistCredentials: function persistCredentials(force) {
    if (this._basicPasswordUpdated || force) {
      if (this._basicPassword) {
        this._setLogin(PWDMGR_PASSWORD_REALM, this.username,
                       this._basicPassword);
      } else {
        for each (let login in this._getLogins(PWDMGR_PASSWORD_REALM)) {
          Services.logins.removeLogin(login);
        }
      }

      this._basicPasswordUpdated = false;
    }

    if (this._syncKeyUpdated || force) {
      if (this._syncKey) {
        this._setLogin(PWDMGR_PASSPHRASE_REALM, this.username, this._syncKey);
      } else {
        for each (let login in this._getLogins(PWDMGR_PASSPHRASE_REALM)) {
          Services.logins.removeLogin(login);
        }
      }

      this._syncKeyUpdated = false;
    }

  },

  /**
   * Deletes the Sync Key from the system.
   */
  deleteSyncKey: function deleteSyncKey() {
    this.syncKey = null;
    this.persistCredentials();
  },

  hasBasicCredentials: function hasBasicCredentials() {
    // Because JavaScript.
    return this.username && this.basicPassword && true;
  },

  /**
   * Obtains the array of basic logins from nsiPasswordManager.
   */
  _getLogins: function _getLogins(realm) {
    return Services.logins.findLogins({}, PWDMGR_HOST, null, realm);
  },

  /**
   * Set a login in the password manager.
   *
   * This has the side-effect of deleting any other logins for the specified
   * realm.
   */
  _setLogin: function _setLogin(realm, username, password) {
    let exists = false;
    for each (let login in this._getLogins(realm)) {
      if (login.username == username && login.password == password) {
        exists = true;
      } else {
        this._log.debug("Pruning old login for " + username + " from " + realm);
        Services.logins.removeLogin(login);
      }
    }

    if (exists) {
      return;
    }

    this._log.debug("Updating saved password for " + username + " in " +
                    realm);

    let loginInfo = new Components.Constructor(
      "@mozilla.org/login-manager/loginInfo;1", Ci.nsILoginInfo, "init");
    let login = new loginInfo(PWDMGR_HOST, null, realm, username,
                                password, "", "");
    Services.logins.addLogin(login);
  },

  /**
   * Deletes Sync credentials from the password manager.
   */
  deleteSyncCredentials: function deleteSyncCredentials() {
    let logins = Services.logins.findLogins({}, PWDMGR_HOST, "", "");
    for each (let login in logins) {
      Services.logins.removeLogin(login);
    }

    // Wait until after store is updated in case it fails.
    this._basicPassword = null;
    this._basicPasswordAllowLookup = true;
    this._basicPasswordUpdated = false;

    this._syncKey = null;
    // this._syncKeyBundle is nullified as part of _syncKey setter.
    this._syncKeyAllowLookup = true;
    this._syncKeyUpdated = false;
  },

  usernameFromAccount: function usernameFromAccount(value) {
    // If we encounter characters not allowed by the API (as found for
    // instance in an email address), hash the value.
    if (value && value.match(/[^A-Z0-9._-]/i)) {
      return Utils.sha1Base32(value.toLowerCase()).toLowerCase();
    }

    return value ? value.toLowerCase() : value;
  },

  /**
   * Obtain a function to be used for adding auth to Resource HTTP requests.
   */
  getResourceAuthenticator: function getResourceAuthenticator() {
    if (this.hasBasicCredentials()) {
      return this._onResourceRequestBasic.bind(this);
    }

    return null;
  },

  /**
   * Helper method to return an authenticator for basic Resource requests.
   */
  getBasicResourceAuthenticator:
    function getBasicResourceAuthenticator(username, password) {

    return function basicAuthenticator(resource) {
      let value = "Basic " + btoa(username + ":" + password);
      return {headers: {authorization: value}};
    };
  },

  _onResourceRequestBasic: function _onResourceRequestBasic(resource) {
    let value = "Basic " + btoa(this.username + ":" + this.basicPassword);
    return {headers: {authorization: value}};
  },

  _onResourceRequestMAC: function _onResourceRequestMAC(resource, method) {
    // TODO Get identifier and key from somewhere.
    let identifier;
    let key;
    let result = Utils.computeHTTPMACSHA1(identifier, key, method, resource.uri);

    return {headers: {authorization: result.header}};
  },

  /**
   * Obtain a function to be used for adding auth to RESTRequest instances.
   */
  getRESTRequestAuthenticator: function getRESTRequestAuthenticator() {
    if (this.hasBasicCredentials()) {
      return this.onRESTRequestBasic.bind(this);
    }

    return null;
  },

  onRESTRequestBasic: function onRESTRequestBasic(request) {
    let up = this.username + ":" + this.basicPassword;
    request.setHeader("authorization", "Basic " + btoa(up));
  }
};
