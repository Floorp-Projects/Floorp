/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["BrowserIDManager"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-common/async.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-common/tokenserverclient.js");
Cu.import("resource://services-crypto/utils.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-common/tokenserverclient.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://services-sync/stages/cluster.js");
Cu.import("resource://gre/modules/FxAccounts.jsm");

// Lazy imports to prevent unnecessary load on startup.
XPCOMUtils.defineLazyModuleGetter(this, "BulkKeyBundle",
                                  "resource://services-sync/keys.js");

XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
                                  "resource://gre/modules/FxAccounts.jsm");

XPCOMUtils.defineLazyGetter(this, 'fxAccountsCommon', function() {
  let ob = {};
  Cu.import("resource://gre/modules/FxAccountsCommon.js", ob);
  return ob;
});

XPCOMUtils.defineLazyGetter(this, 'log', function() {
  let log = Log.repository.getLogger("Sync.BrowserIDManager");
  log.addAppender(new Log.DumpAppender());
  log.level = Log.Level[Svc.Prefs.get("log.logger.identity")] || Log.Level.Error;
  return log;
});

const PREF_SYNC_SHOW_CUSTOMIZATION = "services.sync.ui.showCustomizationDialog";

function deriveKeyBundle(kB) {
  let out = CryptoUtils.hkdf(kB, undefined,
                             "identity.mozilla.com/picl/v1/oldsync", 2*32);
  let bundle = new BulkKeyBundle();
  // [encryptionKey, hmacKey]
  bundle.keyPair = [out.slice(0, 32), out.slice(32, 64)];
  return bundle;
}

/*
  General authentication error for abstracting authentication
  errors from multiple sources (e.g., from FxAccounts, TokenServer)
    'message' is a string with a description of the error
*/
function AuthenticationError(message) {
  this.message = message || "";
}

this.BrowserIDManager = function BrowserIDManager() {
  this._fxaService = fxAccounts;
  this._tokenServerClient = new TokenServerClient();
  // will be a promise that resolves when we are ready to authenticate
  this.whenReadyToAuthenticate = null;
  this._log = log;
};

this.BrowserIDManager.prototype = {
  __proto__: IdentityManager.prototype,

  _fxaService: null,
  _tokenServerClient: null,
  // https://docs.services.mozilla.com/token/apis.html
  _token: null,
  _account: null,

  // it takes some time to fetch a sync key bundle, so until this flag is set,
  // we don't consider the lack of a keybundle as a failure state.
  _shouldHaveSyncKeyBundle: false,

  get readyToAuthenticate() {
    // We are finished initializing when we *should* have a sync key bundle,
    // although we might not actually have one due to auth failures etc.
    return this._shouldHaveSyncKeyBundle;
  },

  get needsCustomization() {
    try {
      return Services.prefs.getBoolPref(PREF_SYNC_SHOW_CUSTOMIZATION);
    } catch (e) {
      return false;
    }
  },

  initialize: function() {
    Services.obs.addObserver(this, fxAccountsCommon.ONLOGIN_NOTIFICATION, false);
    Services.obs.addObserver(this, fxAccountsCommon.ONLOGOUT_NOTIFICATION, false);
    Services.obs.addObserver(this, "weave:service:logout:finish", false);
    return this.initializeWithCurrentIdentity();
  },

  initializeWithCurrentIdentity: function(isInitialSync=false) {
    this._log.trace("initializeWithCurrentIdentity");
    Components.utils.import("resource://services-sync/main.js");

    // Reset the world before we do anything async.
    this.whenReadyToAuthenticate = Promise.defer();
    this._shouldHaveSyncKeyBundle = false;

    return fxAccounts.getSignedInUser().then(accountData => {
      if (!accountData) {
        this._log.info("initializeWithCurrentIdentity has no user logged in");
        this._account = null;
        return;
      }

      this._account = accountData.email;
      // The user must be verified before we can do anything at all; we kick
      // this and the rest of initialization off in the background (ie, we
      // don't return the promise)
      this._log.info("Waiting for user to be verified.");
      fxAccounts.whenVerified(accountData).then(accountData => {
        // We do the background keybundle fetch...
        this._log.info("Starting fetch for key bundle.");
        if (this.needsCustomization) {
          // If the user chose to "Customize sync options" when signing
          // up with Firefox Accounts, ask them to choose what to sync.
          const url = "chrome://browser/content/sync/customize.xul";
          const features = "centerscreen,chrome,modal,dialog,resizable=no";
          let win = Services.wm.getMostRecentWindow("navigator:browser");

          let data = {accepted: false};
          win.openDialog(url, "_blank", features, data);

          if (data.accepted) {
            Services.prefs.clearUserPref(PREF_SYNC_SHOW_CUSTOMIZATION);
          } else {
            // Log out if the user canceled the dialog.
            return fxAccounts.signOut();
          }
        }
      }).then(() => {
        return this._fetchSyncKeyBundle();
      }).then(() => {
        this._shouldHaveSyncKeyBundle = true; // and we should actually have one...
        this.whenReadyToAuthenticate.resolve();
        this._log.info("Background fetch for key bundle done");
        if (isInitialSync) {
          this._log.info("Doing initial sync actions");
          Svc.Prefs.set("firstSync", "resetClient");
          Services.obs.notifyObservers(null, "weave:service:setup-complete", null);
          Weave.Utils.nextTick(Weave.Service.sync, Weave.Service);
        }
      }).then(null, err => {
        this._shouldHaveSyncKeyBundle = true; // but we probably don't have one...
        this.whenReadyToAuthenticate.reject(err);
        // report what failed...
        this._log.error("Background fetch for key bundle failed: " + err.message);
      });
      // and we are done - the fetch continues on in the background...
    }).then(null, err => {
      this._log.error("Processing logged in account: " + err.message);
    });
  },

  observe: function (subject, topic, data) {
    this._log.debug("observed " + topic);
    switch (topic) {
    case fxAccountsCommon.ONLOGIN_NOTIFICATION:
      this.initializeWithCurrentIdentity(true);
      break;

    case fxAccountsCommon.ONLOGOUT_NOTIFICATION:
      Components.utils.import("resource://services-sync/main.js");
      // Setting .username calls resetCredentials which drops the key bundle
      // and resets _shouldHaveSyncKeyBundle.
      this.username = "";
      this._account = null;
      Weave.Service.logout();
      break;

    case "weave:service:logout:finish":
      // This signals an auth error with the storage server,
      // or that the user unlinked her account from the browser.
      // Either way, we clear our auth token. In the case of an
      // auth error, this will force the fetch of a new one.
      this._token = null;
      break;
    }
  },

   /**
   * Compute the sha256 of the message bytes.  Return bytes.
   */
  _sha256: function(message) {
    let hasher = Cc["@mozilla.org/security/hash;1"]
                    .createInstance(Ci.nsICryptoHash);
    hasher.init(hasher.SHA256);
    return CryptoUtils.digestBytes(message, hasher);
  },

  /**
   * Compute the X-Client-State header given the byte string kB.
   *
   * Return string: hex(first16Bytes(sha256(kBbytes)))
   */
  _computeXClientState: function(kBbytes) {
    return CommonUtils.bytesAsHex(this._sha256(kBbytes).slice(0, 16), false);
  },

  /**
   * Provide override point for testing token expiration.
   */
  _now: function() {
    return this._fxaService.now()
  },

  get _localtimeOffsetMsec() {
    return this._fxaService.localtimeOffsetMsec;
  },

  get account() {
    return this._account;
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
    throw "account setter should be not used in BrowserIDManager";
  },

  /**
   * Obtains the HTTP Basic auth password.
   *
   * Returns a string if set or null if it is not set.
   */
  get basicPassword() {
    this._log.error("basicPassword getter should be not used in BrowserIDManager");
    return null;
  },

  /**
   * Set the HTTP basic password to use.
   *
   * Changes will not persist unless persistSyncCredentials() is called.
   */
  set basicPassword(value) {
    throw "basicPassword setter should be not used in BrowserIDManager";
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
    if (this.syncKeyBundle) {
      // TODO: This is probably fine because the code shouldn't be
      // using the sync key directly (it should use the sync key
      // bundle), but I don't like it. We should probably refactor
      // code that is inspecting this to not do validation on this
      // field directly and instead call a isSyncKeyValid() function
      // that we can override.
      return "99999999999999999999999999";
    }
    else {
      return null;
    }
  },

  set syncKey(value) {
    throw "syncKey setter should be not used in BrowserIDManager";
  },

  get syncKeyBundle() {
    return this._syncKeyBundle;
  },

  /**
   * Resets/Drops all credentials we hold for the current user.
   */
  resetCredentials: function() {
    // the only credentials we hold are the sync key.
    this.resetSyncKey();
  },

  /**
   * Resets/Drops the sync key we hold for the current user.
   */
  resetSyncKey: function() {
    this._syncKey = null;
    this._syncKeyBundle = null;
    this._syncKeyUpdated = true;
    this._shouldHaveSyncKeyBundle = false;
  },

  /**
   * The current state of the auth credentials.
   *
   * This essentially validates that enough credentials are available to use
   * Sync.
   */
  get currentAuthState() {
    // TODO: need to revisit this. Currently this isn't ready to go until
    // both the username and syncKeyBundle are both configured and having no
    // username seems to make things fail fast so that's good.
    if (!this.username) {
      return LOGIN_FAILED_NO_USERNAME;
    }

    // No need to check this.syncKey as our getter for that attribute
    // uses this.syncKeyBundle
    // If bundle creation started, but failed.
    if (this._shouldHaveSyncKeyBundle && !this.syncKeyBundle) {
      return LOGIN_FAILED_NO_PASSPHRASE;
    }

    return STATUS_OK;
  },

  /**
   * Do we have a non-null, not yet expired token whose email field
   * matches (when normalized) our account field?
   */
  hasValidToken: function() {
    if (!this._token) {
      return false;
    }
    if (this._token.expiration < this._now()) {
      return false;
    }
    let signedInUser = this._getSignedInUser();
    if (!signedInUser) {
      return false;
    }
    // Does the signed in user match the user we retrieved the token for?
    if (signedInUser.email !== this.account) {
      return false;
    }
    return true;
  },

  /**
   * Wrap and synchronize FxAccounts.getSignedInUser().
   *
   * @return credentials per wrapped.
   */
  _getSignedInUser: function() {
    let userData;
    let cb = Async.makeSpinningCallback();

    this._fxaService.getSignedInUser().then(function (result) {
        cb(null, result);
    },
    function (err) {
        cb(err);
    });

    try {
      userData = cb.wait();
    } catch (err) {
      this._log.error("FxAccounts.getSignedInUser() failed with: " + err);
      return null;
    }
    return userData;
  },

  _fetchSyncKeyBundle: function() {
    // Fetch a sync token for the logged in user from the token server.
    return this._fxaService.getKeys().then(userData => {
      // Unlikely, but if the logged in user somehow changed between these
      // calls we better fail. TODO: add tests for these
      if (!userData) {
        throw new AuthenticationError("No userData in _fetchSyncKeyBundle");
      } else if (userData.email !== this.account) {
        throw new AuthenticationError("Unexpected user change in _fetchSyncKeyBundle");
      }
      return this._fetchTokenForUser(userData).then(token => {
        this._token = token;
        // Set the username to be the uid returned by the token server.
        this.username = this._token.uid.toString();
        // both Jelly and FxAccounts give us kA/kB as hex.
        let kB = Utils.hexToBytes(userData.kB);
        this._syncKeyBundle = deriveKeyBundle(kB);
        return;
      });
    });
  },

  // Refresh the sync token for the specified Firefox Accounts user.
  _fetchTokenForUser: function(userData) {
    let tokenServerURI = Svc.Prefs.get("tokenServerURI");
    let log = this._log;
    let client = this._tokenServerClient;

    // Both Jelly and FxAccounts give us kB as hex
    let kBbytes = CommonUtils.hexToBytes(userData.kB);
    let headers = {"X-Client-State": this._computeXClientState(kBbytes)};
    log.info("Fetching assertion and token from: " + tokenServerURI);

    function getToken(tokenServerURI, assertion) {
      log.debug("Getting a token");
      let deferred = Promise.defer();
      let cb = function (err, token) {
        if (err) {
          log.info("TokenServerClient.getTokenFromBrowserIDAssertion() failed with: " + err.message);
          return deferred.reject(new AuthenticationError(err.message));
        } else {
          log.debug("Successfully got a sync token");
          return deferred.resolve(token);
        }
      };

      client.getTokenFromBrowserIDAssertion(tokenServerURI, assertion, cb, headers);
      return deferred.promise;
    }

    function getAssertion() {
      log.debug("Getting an assertion");
      let audience = Services.io.newURI(tokenServerURI, null, null).prePath;
      return fxAccounts.getAssertion(audience).then(null, err => {
        if (err.code === 401) {
          throw new AuthenticationError("Unable to get assertion for user");
        } else {
          throw err;
        }
      });
    };

    // wait until the account email is verified and we know that
    // getAssertion() will return a real assertion (not null).
    return this._fxaService.whenVerified(userData)
      .then(() => getAssertion())
      .then(assertion => getToken(tokenServerURI, assertion))
      .then(token => {
        // TODO: Make it be only 80% of the duration, so refresh the token
        // before it actually expires. This is to avoid sync storage errors
        // otherwise, we get a nasty notification bar briefly. Bug 966568.
        token.expiration = this._now() + (token.duration * 1000) * 0.80;
        return token;
      })
      .then(null, err => {
        // TODO: write tests to make sure that different auth error cases are handled here
        // properly: auth error getting assertion, auth error getting token (invalid generation
        // and client-state error)
        if (err instanceof AuthenticationError) {
          this._log.error("Authentication error in _fetchTokenForUser: " + err.message);
          // Drop the sync key bundle, but still expect to have one.
          // This will arrange for us to be in the right 'currentAuthState'
          // such that UI will show the right error.
          this._shouldHaveSyncKeyBundle = true;
          this._syncKeyBundle = null;
          Weave.Status.login = this.currentAuthState;
          Services.obs.notifyObservers(null, "weave:service:login:error", null);
        }
        throw err;
      });
  },

  _fetchTokenForLoggedInUserSync: function() {
    let cb = Async.makeSpinningCallback();

    this._fxaService.getSignedInUser().then(userData => {
      this._fetchTokenForUser(userData).then(token => {
        cb(null, token);
      }, err => {
        cb(err);
      });
    });
    try {
      return cb.wait();
    } catch (err) {
      this._log.info("_fetchTokenForLoggedInUserSync: " + err.message);
      return null;
    }
  },

  getResourceAuthenticator: function () {
    return this._getAuthenticationHeader.bind(this);
  },

  /**
   * Obtain a function to be used for adding auth to RESTRequest instances.
   */
  getRESTRequestAuthenticator: function() {
    return this._addAuthenticationHeader.bind(this);
  },

  /**
   * @return a Hawk HTTP Authorization Header, lightly wrapped, for the .uri
   * of a RESTRequest or AsyncResponse object.
   */
  _getAuthenticationHeader: function(httpObject, method) {
    if (!this.hasValidToken()) {
      // Refresh token for the currently logged in FxA user
      this._token = this._fetchTokenForLoggedInUserSync();
      if (!this._token) {
        return null;
      }
    }
    let credentials = {algorithm: "sha256",
                       id: this._token.id,
                       key: this._token.key,
                      };
    method = method || httpObject.method;

    // Get the local clock offset from the Firefox Accounts server.  This should
    // be close to the offset from the storage server.
    let options = {
      now: this._now(),
      localtimeOffsetMsec: this._localtimeOffsetMsec,
      credentials: credentials,
    };

    let headerValue = CryptoUtils.computeHAWK(httpObject.uri, method, options);
    return {headers: {authorization: headerValue.field}};
  },

  _addAuthenticationHeader: function(request, method) {
    let header = this._getAuthenticationHeader(request, method);
    if (!header) {
      return null;
    }
    request.setHeader("authorization", header.headers.authorization);
    return request;
  },

  createClusterManager: function(service) {
    return new BrowserIDClusterManager(service);
  }

};

/* An implementation of the ClusterManager for this identity
 */

function BrowserIDClusterManager(service) {
  ClusterManager.call(this, service);
}

BrowserIDClusterManager.prototype = {
  __proto__: ClusterManager.prototype,

  _findCluster: function() {
    let promiseClusterURL = function() {
      return fxAccounts.getSignedInUser().then(userData => {
        return this.identity._fetchTokenForUser(userData).then(token => {
          let endpoint = token.endpoint;
          // For Sync 1.5 storage endpoints, we use the base endpoint verbatim.
          // However, it should end in "/" because we will extend it with
          // well known path components. So we add a "/" if it's missing.
          if (!endpoint.endsWith("/")) {
            endpoint += "/";
          }
          return endpoint;
        });
      });
    }.bind(this);

    let cb = Async.makeSpinningCallback();
    promiseClusterURL().then(function (clusterURL) {
        cb(null, clusterURL);
    },
    function (err) {
        cb(err);
    });
    return cb.wait();
  },

  getUserBaseURL: function() {
    // Legacy Sync and FxA Sync construct the userBaseURL differently. Legacy
    // Sync appends path components onto an empty path, and in FxA Sync the
    // token server constructs this for us in an opaque manner. Since the
    // cluster manager already sets the clusterURL on Service and also has
    // access to the current identity, we added this functionality here.
    return this.service.clusterURL;
  }
}
