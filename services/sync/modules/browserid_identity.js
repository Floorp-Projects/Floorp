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
XPCOMUtils.defineLazyModuleGetter(this, "Weave",
                                  "resource://services-sync/main.js");

XPCOMUtils.defineLazyModuleGetter(this, "BulkKeyBundle",
                                  "resource://services-sync/keys.js");

XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
                                  "resource://gre/modules/FxAccounts.jsm");

XPCOMUtils.defineLazyGetter(this, 'log', function() {
  let log = Log.repository.getLogger("Sync.BrowserIDManager");
  log.level = Log.Level[Svc.Prefs.get("log.logger.identity")] || Log.Level.Error;
  return log;
});

// FxAccountsCommon.js doesn't use a "namespace", so create one here.
let fxAccountsCommon = {};
Cu.import("resource://gre/modules/FxAccountsCommon.js", fxAccountsCommon);

const OBSERVER_TOPICS = [
  fxAccountsCommon.ONLOGIN_NOTIFICATION,
  fxAccountsCommon.ONLOGOUT_NOTIFICATION,
];

const PREF_SYNC_SHOW_CUSTOMIZATION = "services.sync-setup.ui.showCustomizationDialog";

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
  errors from multiple sources (e.g., from FxAccounts, TokenServer).
  details is additional details about the error - it might be a string, or
  some other error object (which should do the right thing when toString() is
  called on it)
*/
function AuthenticationError(details) {
  this.details = details;
}

AuthenticationError.prototype = {
  toString: function() {
    return "AuthenticationError(" + this.details + ")";
  }
}

this.BrowserIDManager = function BrowserIDManager() {
  // NOTE: _fxaService and _tokenServerClient are replaced with mocks by
  // the test suite.
  this._fxaService = fxAccounts;
  this._tokenServerClient = new TokenServerClient();
  this._tokenServerClient.observerPrefix = "weave:service";
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
  _signedInUser: null, // the signedinuser we got from FxAccounts.

  // null if no error, otherwise a LOGIN_FAILED_* value that indicates why
  // we failed to authenticate (but note it might not be an actual
  // authentication problem, just a transient network error or similar)
  _authFailureReason: null,

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
    for (let topic of OBSERVER_TOPICS) {
      Services.obs.addObserver(this, topic, false);
    }
    return this.initializeWithCurrentIdentity();
  },

  /**
   * Ensure the user is logged in.  Returns a promise that resolves when
   * the user is logged in, or is rejected if the login attempt has failed.
   */
  ensureLoggedIn: function() {
    if (!this._shouldHaveSyncKeyBundle) {
      // We are already in the process of logging in.
      return this.whenReadyToAuthenticate.promise;
    }

    // If we are already happy then there is nothing more to do.
    if (this._syncKeyBundle) {
      return Promise.resolve();
    }

    // Similarly, if we have a previous failure that implies an explicit
    // re-entering of credentials by the user is necessary we don't take any
    // further action - an observer will fire when the user does that.
    if (Weave.Status.login == LOGIN_FAILED_LOGIN_REJECTED) {
      return Promise.reject();
    }

    // So - we've a previous auth problem and aren't currently attempting to
    // log in - so fire that off.
    this.initializeWithCurrentIdentity();
    return this.whenReadyToAuthenticate.promise;
  },

  finalize: function() {
    // After this is called, we can expect Service.identity != this.
    for (let topic of OBSERVER_TOPICS) {
      Services.obs.removeObserver(this, topic);
    }
    this.resetCredentials();
    this._signedInUser = null;
    return Promise.resolve();
  },

  offerSyncOptions: function () {
    // If the user chose to "Customize sync options" when signing
    // up with Firefox Accounts, ask them to choose what to sync.
    const url = "chrome://browser/content/sync/customize.xul";
    const features = "centerscreen,chrome,modal,dialog,resizable=no";
    let win = Services.wm.getMostRecentWindow("navigator:browser");

    let data = {accepted: false};
    win.openDialog(url, "_blank", features, data);

    return data;
  },

  initializeWithCurrentIdentity: function(isInitialSync=false) {
    // While this function returns a promise that resolves once we've started
    // the auth process, that process is complete when
    // this.whenReadyToAuthenticate.promise resolves.
    this._log.trace("initializeWithCurrentIdentity");

    // Reset the world before we do anything async.
    this.whenReadyToAuthenticate = Promise.defer();
    this.whenReadyToAuthenticate.promise.then(null, (err) => {
      this._log.error("Could not authenticate", err);
    });

    // initializeWithCurrentIdentity() can be called after the
    // identity module was first initialized, e.g., after the
    // user completes a force authentication, so we should make
    // sure all credentials are reset before proceeding.
    this.resetCredentials();
    this._authFailureReason = null;

    return this._fxaService.getSignedInUser().then(accountData => {
      if (!accountData) {
        this._log.info("initializeWithCurrentIdentity has no user logged in");
        this.account = null;
        // and we are as ready as we can ever be for auth.
        this._shouldHaveSyncKeyBundle = true;
        this.whenReadyToAuthenticate.reject("no user is logged in");
        return;
      }

      this.account = accountData.email;
      this._updateSignedInUser(accountData);
      // The user must be verified before we can do anything at all; we kick
      // this and the rest of initialization off in the background (ie, we
      // don't return the promise)
      this._log.info("Waiting for user to be verified.");
      this._fxaService.whenVerified(accountData).then(accountData => {
        this._updateSignedInUser(accountData);
        this._log.info("Starting fetch for key bundle.");
        if (this.needsCustomization) {
          let data = this.offerSyncOptions();
          if (data.accepted) {
            Services.prefs.clearUserPref(PREF_SYNC_SHOW_CUSTOMIZATION);

            // Mark any non-selected engines as declined.
            Weave.Service.engineManager.declineDisabled();
          } else {
            // Log out if the user canceled the dialog.
            return this._fxaService.signOut();
          }
        }
      }).then(() => {
        return this._fetchTokenForUser();
      }).then(token => {
        this._token = token;
        this._shouldHaveSyncKeyBundle = true; // and we should actually have one...
        this.whenReadyToAuthenticate.resolve();
        this._log.info("Background fetch for key bundle done");
        Weave.Status.login = LOGIN_SUCCEEDED;
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
        this._log.error("Background fetch for key bundle failed", err);
      });
      // and we are done - the fetch continues on in the background...
    }).then(null, err => {
      this._log.error("Processing logged in account", err);
    });
  },

  _updateSignedInUser: function(userData) {
    // This object should only ever be used for a single user.  It is an
    // error to update the data if the user changes (but updates are still
    // necessary, as each call may add more attributes to the user).
    // We start with no user, so an initial update is always ok.
    if (this._signedInUser && this._signedInUser.email != userData.email) {
      throw new Error("Attempting to update to a different user.")
    }
    this._signedInUser = userData;
  },

  logout: function() {
    // This will be called when sync fails (or when the account is being
    // unlinked etc).  It may have failed because we got a 401 from a sync
    // server, so we nuke the token.  Next time sync runs and wants an
    // authentication header, we will notice the lack of the token and fetch a
    // new one.
    this._token = null;
  },

  observe: function (subject, topic, data) {
    this._log.debug("observed " + topic);
    switch (topic) {
    case fxAccountsCommon.ONLOGIN_NOTIFICATION:
      // This should only happen if we've been initialized without a current
      // user - otherwise we'd have seen the LOGOUT notification and been
      // thrown away.
      // The exception is when we've initialized with a user that needs to
      // reauth with the server - in that case we will also get here, but
      // should have the same identity.
      // initializeWithCurrentIdentity will throw and log if these contraints
      // aren't met, so just go ahead and do the init.
      this.initializeWithCurrentIdentity(true);
      break;

    case fxAccountsCommon.ONLOGOUT_NOTIFICATION:
      Weave.Service.startOver();
      // startOver will cause this instance to be thrown away, so there's
      // nothing else to do.
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

  usernameFromAccount: function(val) {
    // we don't differentiate between "username" and "account"
    return val;
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
    this.resetSyncKey();
    this._token = null;
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
   * Pre-fetches any information that might help with migration away from this
   * identity.  Called after every sync and is really just an optimization that
   * allows us to avoid a network request for when we actually need the
   * migration info.
   */
  prefetchMigrationSentinel: function(service) {
    // nothing to do here until we decide to migrate away from FxA.
  },

  /**
    * Return credentials hosts for this identity only.
    */
  _getSyncCredentialsHosts: function() {
    return Utils.getSyncCredentialsHostsFxA();
  },

  /**
   * The current state of the auth credentials.
   *
   * This essentially validates that enough credentials are available to use
   * Sync, although it effectively ignores the state of the master-password -
   * if that's locked and that's the only problem we can see, say everything
   * is OK - unlockAndVerifyAuthState will be used to perform the unlock
   * and re-verification if necessary.
   */
  get currentAuthState() {
    if (this._authFailureReason) {
      this._log.info("currentAuthState returning " + this._authFailureReason +
                     " due to previous failure");
      return this._authFailureReason;
    }
    // TODO: need to revisit this. Currently this isn't ready to go until
    // both the username and syncKeyBundle are both configured and having no
    // username seems to make things fail fast so that's good.
    if (!this.username) {
      return LOGIN_FAILED_NO_USERNAME;
    }

    // No need to check this.syncKey as our getter for that attribute
    // uses this.syncKeyBundle
    // If bundle creation started, but failed due to any reason other than
    // the MP being locked...
    if (this._shouldHaveSyncKeyBundle && !this.syncKeyBundle && !Utils.mpLocked()) {
      // Return a state that says a re-auth is necessary so we can get keys.
      return LOGIN_FAILED_LOGIN_REJECTED;
    }

    return STATUS_OK;
  },

  // Do we currently have keys, or do we have enough that we should be able
  // to successfully fetch them?
  _canFetchKeys: function() {
    let userData = this._signedInUser;
    // a keyFetchToken means we can almost certainly grab them.
    // kA and kB means we already have them.
    return userData && (userData.keyFetchToken || (userData.kA && userData.kB));
  },

  /**
   * Verify the current auth state, unlocking the master-password if necessary.
   *
   * Returns a promise that resolves with the current auth state after
   * attempting to unlock.
   */
  unlockAndVerifyAuthState: function() {
    if (this._canFetchKeys()) {
      return Promise.resolve(STATUS_OK);
    }
    // so no keys - ensure MP unlocked.
    if (!Utils.ensureMPUnlocked()) {
      // user declined to unlock, so we don't know if they are stored there.
      return Promise.resolve(MASTER_PASSWORD_LOCKED);
    }
    // now we are unlocked we must re-fetch the user data as we may now have
    // the details that were previously locked away.
    return this._fxaService.getSignedInUser().then(
      accountData => {
        this._updateSignedInUser(accountData);
        // If we still can't get keys it probably means the user authenticated
        // without unlocking the MP or cleared the saved logins, so we've now
        // lost them - the user will need to reauth before continuing.
        return this._canFetchKeys() ? STATUS_OK : LOGIN_FAILED_LOGIN_REJECTED;
      }
    );
  },

  /**
   * Do we have a non-null, not yet expired token for the user currently
   * signed in?
   */
  hasValidToken: function() {
    // If pref is set to ignore cached authentication credentials for debugging,
    // then return false to force the fetching of a new token.
    let ignoreCachedAuthCredentials = false;
    try {
      ignoreCachedAuthCredentials = Svc.Prefs.get("debug.ignoreCachedAuthCredentials");
    } catch(e) {
      // Pref doesn't exist
    }
    if (ignoreCachedAuthCredentials) {
      return false;
    }
    if (!this._token) {
      return false;
    }
    if (this._token.expiration < this._now()) {
      return false;
    }
    return true;
  },

  // Refresh the sync token for our user. Returns a promise that resolves
  // with a token (which may be null in one sad edge-case), or rejects with an
  // error.
  _fetchTokenForUser: function() {
    let tokenServerURI = Svc.Prefs.get("tokenServerURI");
    if (tokenServerURI.endsWith("/")) { // trailing slashes cause problems...
      tokenServerURI = tokenServerURI.slice(0, -1);
    }
    let log = this._log;
    let client = this._tokenServerClient;
    let fxa = this._fxaService;
    let userData = this._signedInUser;

    // We need kA and kB for things to work.  If we don't have them, just
    // return null for the token - sync calling unlockAndVerifyAuthState()
    // before actually syncing will setup the error states if necessary.
    if (!this._canFetchKeys()) {
      return Promise.resolve(null);
    }

    let maybeFetchKeys = () => {
      // This is called at login time and every time we need a new token - in
      // the latter case we already have kA and kB, so optimise that case.
      if (userData.kA && userData.kB) {
        return;
      }
      log.info("Fetching new keys");
      return this._fxaService.getKeys().then(
        newUserData => {
          userData = newUserData;
          this._updateSignedInUser(userData); // throws if the user changed.
        }
      );
    }

    let getToken = (tokenServerURI, assertion) => {
      log.debug("Getting a token");
      let deferred = Promise.defer();
      let cb = function (err, token) {
        if (err) {
          return deferred.reject(err);
        }
        log.debug("Successfully got a sync token");
        return deferred.resolve(token);
      };

      let kBbytes = CommonUtils.hexToBytes(userData.kB);
      let headers = {"X-Client-State": this._computeXClientState(kBbytes)};
      client.getTokenFromBrowserIDAssertion(tokenServerURI, assertion, cb, headers);
      return deferred.promise;
    }

    let getAssertion = () => {
      log.info("Getting an assertion from", tokenServerURI);
      let audience = Services.io.newURI(tokenServerURI, null, null).prePath;
      return fxa.getAssertion(audience);
    };

    // wait until the account email is verified and we know that
    // getAssertion() will return a real assertion (not null).
    return fxa.whenVerified(this._signedInUser)
      .then(() => maybeFetchKeys())
      .then(() => getAssertion())
      .then(assertion => getToken(tokenServerURI, assertion))
      .then(token => {
        // TODO: Make it be only 80% of the duration, so refresh the token
        // before it actually expires. This is to avoid sync storage errors
        // otherwise, we get a nasty notification bar briefly. Bug 966568.
        token.expiration = this._now() + (token.duration * 1000) * 0.80;
        if (!this._syncKeyBundle) {
          // We are given kA/kB as hex.
          this._syncKeyBundle = deriveKeyBundle(Utils.hexToBytes(userData.kB));
        }
        return token;
      })
      .then(null, err => {
        // TODO: unify these errors - we need to handle errors thrown by
        // both tokenserverclient and hawkclient.
        // A tokenserver error thrown based on a bad response.
        if (err.response && err.response.status === 401) {
          err = new AuthenticationError(err);
        // A hawkclient error.
        } else if (err.code && err.code === 401) {
          err = new AuthenticationError(err);
        }

        // TODO: write tests to make sure that different auth error cases are handled here
        // properly: auth error getting assertion, auth error getting token (invalid generation
        // and client-state error)
        if (err instanceof AuthenticationError) {
          this._log.error("Authentication error in _fetchTokenForUser", err);
          // set it to the "fatal" LOGIN_FAILED_LOGIN_REJECTED reason.
          this._authFailureReason = LOGIN_FAILED_LOGIN_REJECTED;
        } else {
          this._log.error("Non-authentication error in _fetchTokenForUser", err);
          // for now assume it is just a transient network related problem
          // (although sadly, it might also be a regular unhandled exception)
          this._authFailureReason = LOGIN_FAILED_NETWORK_ERROR;
        }
        // this._authFailureReason being set to be non-null in the above if clause
        // ensures we are in the correct currentAuthState, and
        // this._shouldHaveSyncKeyBundle being true ensures everything that cares knows
        // that there is no authentication dance still under way.
        this._shouldHaveSyncKeyBundle = true;
        Weave.Status.login = this._authFailureReason;
        Services.obs.notifyObservers(null, "weave:service:login:error", null);
        throw err;
      });
  },

  // Returns a promise that is resolved when we have a valid token for the
  // current user stored in this._token.  When resolved, this._token is valid.
  _ensureValidToken: function() {
    if (this.hasValidToken()) {
      this._log.debug("_ensureValidToken already has one");
      return Promise.resolve();
    }
    // reset this._token as a safety net to reduce the possibility of us
    // repeatedly attempting to use an invalid token if _fetchTokenForUser throws.
    this._token = null;
    return this._fetchTokenForUser().then(
      token => {
        this._token = token;
      }
    );
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
    let cb = Async.makeSpinningCallback();
    this._ensureValidToken().then(cb, cb);
    try {
      cb.wait();
    } catch (ex) {
      this._log.error("Failed to fetch a token for authentication", ex);
      return null;
    }
    if (!this._token) {
      return null;
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
    let endPointFromIdentityToken = function() {
      // The only reason (in theory ;) that we can end up with a null token
      // is when this.identity._canFetchKeys() returned false.  In turn, this
      // should only happen if the master-password is locked or the credentials
      // storage is screwed, and in those cases we shouldn't have started
      // syncing so shouldn't get here anyway.
      // But better safe than sorry! To keep things clearer, throw an explicit
      // exception - the message will appear in the logs and the error will be
      // treated as transient.
      if (!this.identity._token) {
        throw new Error("Can't get a cluster URL as we can't fetch keys.");
      }
      let endpoint = this.identity._token.endpoint;
      // For Sync 1.5 storage endpoints, we use the base endpoint verbatim.
      // However, it should end in "/" because we will extend it with
      // well known path components. So we add a "/" if it's missing.
      if (!endpoint.endsWith("/")) {
        endpoint += "/";
      }
      log.debug("_findCluster returning " + endpoint);
      return endpoint;
    }.bind(this);

    // Spinningly ensure we are ready to authenticate and have a valid token.
    let promiseClusterURL = function() {
      return this.identity.whenReadyToAuthenticate.promise.then(
        () => {
          // We need to handle node reassignment here.  If we are being asked
          // for a clusterURL while the service already has a clusterURL, then
          // it's likely a 401 was received using the existing token - in which
          // case we just discard the existing token and fetch a new one.
          if (this.service.clusterURL) {
            log.debug("_findCluster found existing clusterURL, so discarding the current token");
            this.identity._token = null;
          }
          return this.identity._ensureValidToken();
        }
      ).then(endPointFromIdentityToken
      );
    }.bind(this);

    let cb = Async.makeSpinningCallback();
    promiseClusterURL().then(function (clusterURL) {
      cb(null, clusterURL);
    }).then(
      null, err => {
      log.info("Failed to fetch the cluster URL", err);
      // service.js's verifyLogin() method will attempt to fetch a cluster
      // URL when it sees a 401.  If it gets null, it treats it as a "real"
      // auth error and sets Status.login to LOGIN_FAILED_LOGIN_REJECTED, which
      // in turn causes a notification bar to appear informing the user they
      // need to re-authenticate.
      // On the other hand, if fetching the cluster URL fails with an exception,
      // verifyLogin() assumes it is a transient error, and thus doesn't show
      // the notification bar under the assumption the issue will resolve
      // itself.
      // Thus:
      // * On a real 401, we must return null.
      // * On any other problem we must let an exception bubble up.
      if (err instanceof AuthenticationError) {
        // callback with no error and a null result - cb.wait() returns null.
        cb(null, null);
      } else {
        // callback with an error - cb.wait() completes by raising an exception.
        cb(err);
      }
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
