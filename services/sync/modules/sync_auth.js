/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["SyncAuthManager", "AuthenticationError"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
const { Async } = ChromeUtils.import("resource://services-common/async.js");
const { TokenServerClient } = ChromeUtils.import(
  "resource://services-common/tokenserverclient.js"
);
const { CryptoUtils } = ChromeUtils.import(
  "resource://services-crypto/utils.js"
);
const { Svc, Utils } = ChromeUtils.import("resource://services-sync/util.js");
const {
  LOGIN_FAILED_LOGIN_REJECTED,
  LOGIN_FAILED_NETWORK_ERROR,
  LOGIN_FAILED_NO_USERNAME,
  LOGIN_SUCCEEDED,
  MASTER_PASSWORD_LOCKED,
  STATUS_OK,
} = ChromeUtils.import("resource://services-sync/constants.js");

const lazy = {};

// Lazy imports to prevent unnecessary load on startup.
ChromeUtils.defineModuleGetter(
  lazy,
  "Weave",
  "resource://services-sync/main.js"
);

ChromeUtils.defineModuleGetter(
  lazy,
  "BulkKeyBundle",
  "resource://services-sync/keys.js"
);

XPCOMUtils.defineLazyGetter(lazy, "fxAccounts", () => {
  return ChromeUtils.import(
    "resource://gre/modules/FxAccounts.jsm"
  ).getFxAccountsSingleton();
});

XPCOMUtils.defineLazyGetter(lazy, "log", function() {
  let log = Log.repository.getLogger("Sync.SyncAuthManager");
  log.manageLevelFromPref("services.sync.log.logger.identity");
  return log;
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "IGNORE_CACHED_AUTH_CREDENTIALS",
  "services.sync.debug.ignoreCachedAuthCredentials"
);

// FxAccountsCommon.js doesn't use a "namespace", so create one here.
var fxAccountsCommon = ChromeUtils.import(
  "resource://gre/modules/FxAccountsCommon.js"
);

const SCOPE_OLD_SYNC = fxAccountsCommon.SCOPE_OLD_SYNC;

const OBSERVER_TOPICS = [
  fxAccountsCommon.ONLOGIN_NOTIFICATION,
  fxAccountsCommon.ONVERIFIED_NOTIFICATION,
  fxAccountsCommon.ONLOGOUT_NOTIFICATION,
  fxAccountsCommon.ON_ACCOUNT_STATE_CHANGE_NOTIFICATION,
  "weave:connected",
];

/*
  General authentication error for abstracting authentication
  errors from multiple sources (e.g., from FxAccounts, TokenServer).
  details is additional details about the error - it might be a string, or
  some other error object (which should do the right thing when toString() is
  called on it)
*/
function AuthenticationError(details, source) {
  this.details = details;
  this.source = source;
}

AuthenticationError.prototype = {
  toString() {
    return "AuthenticationError(" + this.details + ")";
  },
};

// The `SyncAuthManager` coordinates access authorization to the Sync server.
// Its job is essentially to get us from having a signed-in Firefox Accounts user,
// to knowing the user's sync storage node and having the necessary short-lived
// credentials in order to access it.
//

function SyncAuthManager() {
  // NOTE: _fxaService and _tokenServerClient are replaced with mocks by
  // the test suite.
  this._fxaService = lazy.fxAccounts;
  this._tokenServerClient = new TokenServerClient();
  this._tokenServerClient.observerPrefix = "weave:service";
  this._log = lazy.log;
  XPCOMUtils.defineLazyPreferenceGetter(
    this,
    "_username",
    "services.sync.username"
  );

  this.asyncObserver = Async.asyncObserver(this, lazy.log);
  for (let topic of OBSERVER_TOPICS) {
    Services.obs.addObserver(this.asyncObserver, topic);
  }
}

SyncAuthManager.prototype = {
  _fxaService: null,
  _tokenServerClient: null,
  // https://docs.services.mozilla.com/token/apis.html
  _token: null,
  // protection against the user changing underneath us - the uid
  // of the current user.
  _userUid: null,

  hashedUID() {
    const id = this._fxaService.telemetry.getSanitizedUID();
    if (!id) {
      throw new Error("hashedUID: Don't seem to have previously seen a token");
    }
    return id;
  },

  // Return a hashed version of a deviceID, suitable for telemetry.
  hashedDeviceID(deviceID) {
    const id = this._fxaService.telemetry.sanitizeDeviceId(deviceID);
    if (!id) {
      throw new Error("hashedUID: Don't seem to have previously seen a token");
    }
    return id;
  },

  // The "node type" reported to telemetry or null if not specified.
  get telemetryNodeType() {
    return this._token && this._token.node_type ? this._token.node_type : null;
  },

  finalize() {
    // After this is called, we can expect Service.identity != this.
    for (let topic of OBSERVER_TOPICS) {
      Services.obs.removeObserver(this.asyncObserver, topic);
    }
    this.resetCredentials();
    this._userUid = null;
  },

  async getSignedInUser() {
    let data = await this._fxaService.getSignedInUser();
    if (!data) {
      this._userUid = null;
      return null;
    }
    if (this._userUid == null) {
      this._userUid = data.uid;
    } else if (this._userUid != data.uid) {
      throw new Error("The signed in user has changed");
    }
    return data;
  },

  logout() {
    // This will be called when sync fails (or when the account is being
    // unlinked etc).  It may have failed because we got a 401 from a sync
    // server, so we nuke the token.  Next time sync runs and wants an
    // authentication header, we will notice the lack of the token and fetch a
    // new one.
    this._token = null;
  },

  async observe(subject, topic, data) {
    this._log.debug("observed " + topic);
    if (!this.username) {
      this._log.info("Sync is not configured, so ignoring the notification");
      return;
    }
    switch (topic) {
      case "weave:connected":
      case fxAccountsCommon.ONLOGIN_NOTIFICATION: {
        this._log.info("Sync has been connected to a logged in user");
        this.resetCredentials();
        let accountData = await this.getSignedInUser();

        if (!accountData.verified) {
          // wait for a verified notification before we kick sync off.
          this._log.info("The user is not verified");
          break;
        }
      }
      // We've been configured with an already verified user, so fall-through.
      // intentional fall-through - the user is verified.
      case fxAccountsCommon.ONVERIFIED_NOTIFICATION: {
        this._log.info("The user became verified");
        lazy.Weave.Status.login = LOGIN_SUCCEEDED;

        // And actually sync. If we've never synced before, we force a full sync.
        // If we have, then we are probably just reauthenticating so it's a normal sync.
        // We can use any pref that must be set if we've synced before, and check
        // the sync lock state because we might already be doing that first sync.
        let isFirstSync =
          !lazy.Weave.Service.locked && !Svc.Prefs.get("client.syncID", null);
        if (isFirstSync) {
          this._log.info("Doing initial sync actions");
          Svc.Prefs.set("firstSync", "resetClient");
          Services.obs.notifyObservers(null, "weave:service:setup-complete");
        }
        // There's no need to wait for sync to complete and it would deadlock
        // our AsyncObserver.
        if (!Svc.Prefs.get("testing.tps", false)) {
          lazy.Weave.Service.sync({ why: "login" });
        }
        break;
      }

      case fxAccountsCommon.ONLOGOUT_NOTIFICATION:
        lazy.Weave.Service.startOver()
          .then(() => {
            this._log.trace("startOver completed");
          })
          .catch(err => {
            this._log.warn("Failed to reset sync", err);
          });
        // startOver will cause this instance to be thrown away, so there's
        // nothing else to do.
        break;

      case fxAccountsCommon.ON_ACCOUNT_STATE_CHANGE_NOTIFICATION:
        // throw away token forcing us to fetch a new one later.
        this.resetCredentials();
        break;
    }
  },

  /**
   * Provide override point for testing token expiration.
   */
  _now() {
    return this._fxaService._internal.now();
  },

  get _localtimeOffsetMsec() {
    return this._fxaService._internal.localtimeOffsetMsec;
  },

  get syncKeyBundle() {
    return this._syncKeyBundle;
  },

  get username() {
    return this._username;
  },

  /**
   * Set the username value.
   *
   * Changing the username has the side-effect of wiping credentials.
   */
  set username(value) {
    // setting .username is an old throwback, but it should no longer happen.
    throw new Error("don't set the username");
  },

  /**
   * Resets all calculated credentials we hold for the current user. This will
   * *not* force the user to reauthenticate, but instead will force us to
   * calculate a new key bundle, fetch a new token, etc.
   */
  resetCredentials() {
    this._syncKeyBundle = null;
    this._token = null;
    // The cluster URL comes from the token, so resetting it to empty will
    // force Sync to not accidentally use a value from an earlier token.
    lazy.Weave.Service.clusterURL = null;
  },

  /**
   * Pre-fetches any information that might help with migration away from this
   * identity.  Called after every sync and is really just an optimization that
   * allows us to avoid a network request for when we actually need the
   * migration info.
   */
  prefetchMigrationSentinel(service) {
    // nothing to do here until we decide to migrate away from FxA.
  },

  /**
   * Verify the current auth state, unlocking the master-password if necessary.
   *
   * Returns a promise that resolves with the current auth state after
   * attempting to unlock.
   */
  async unlockAndVerifyAuthState() {
    let data = await this.getSignedInUser();
    const fxa = this._fxaService;
    if (!data) {
      lazy.log.debug("unlockAndVerifyAuthState has no FxA user");
      return LOGIN_FAILED_NO_USERNAME;
    }
    if (!this.username) {
      lazy.log.debug(
        "unlockAndVerifyAuthState finds that sync isn't configured"
      );
      return LOGIN_FAILED_NO_USERNAME;
    }
    if (!data.verified) {
      // Treat not verified as if the user needs to re-auth, so the browser
      // UI reflects the state.
      lazy.log.debug("unlockAndVerifyAuthState has an unverified user");
      return LOGIN_FAILED_LOGIN_REJECTED;
    }
    if (await fxa.keys.canGetKeyForScope(SCOPE_OLD_SYNC)) {
      lazy.log.debug(
        "unlockAndVerifyAuthState already has (or can fetch) sync keys"
      );
      return STATUS_OK;
    }
    // so no keys - ensure MP unlocked.
    if (!Utils.ensureMPUnlocked()) {
      // user declined to unlock, so we don't know if they are stored there.
      lazy.log.debug(
        "unlockAndVerifyAuthState: user declined to unlock master-password"
      );
      return MASTER_PASSWORD_LOCKED;
    }
    // If we still can't get keys it probably means the user authenticated
    // without unlocking the MP or cleared the saved logins, so we've now
    // lost them - the user will need to reauth before continuing.
    let result;
    if (await fxa.keys.canGetKeyForScope(SCOPE_OLD_SYNC)) {
      result = STATUS_OK;
    } else {
      result = LOGIN_FAILED_LOGIN_REJECTED;
    }
    lazy.log.debug(
      "unlockAndVerifyAuthState re-fetched credentials and is returning",
      result
    );
    return result;
  },

  /**
   * Do we have a non-null, not yet expired token for the user currently
   * signed in?
   */
  _hasValidToken() {
    // If pref is set to ignore cached authentication credentials for debugging,
    // then return false to force the fetching of a new token.
    if (lazy.IGNORE_CACHED_AUTH_CREDENTIALS) {
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

  // Get our tokenServerURL - a private helper. Returns a string.
  get _tokenServerUrl() {
    // We used to support services.sync.tokenServerURI but this was a
    // pain-point for people using non-default servers as Sync may auto-reset
    // all services.sync prefs. So if that still exists, it wins.
    let url = Svc.Prefs.get("tokenServerURI"); // Svc.Prefs "root" is services.sync
    if (!url) {
      url = Services.prefs.getCharPref("identity.sync.tokenserver.uri");
    }
    while (url.endsWith("/")) {
      // trailing slashes cause problems...
      url = url.slice(0, -1);
    }
    return url;
  },

  // Refresh the sync token for our user. Returns a promise that resolves
  // with a token, or rejects with an error.
  async _fetchTokenForUser() {
    const fxa = this._fxaService;
    // We need keys for things to work.  If we don't have them, just
    // return null for the token - sync calling unlockAndVerifyAuthState()
    // before actually syncing will setup the error states if necessary.
    if (!(await fxa.keys.canGetKeyForScope(SCOPE_OLD_SYNC))) {
      this._log.info(
        "Unable to fetch keys (master-password locked?), so aborting token fetch"
      );
      throw new Error("Can't fetch a token as we can't get keys");
    }

    // Do the token dance, with a retry in case of transient auth failure.
    // We need to prove that we know the sync key in order to get a token
    // from the tokenserver.
    let getToken = async key => {
      this._log.info("Getting a sync token from", this._tokenServerUrl);
      let token = await this._fetchTokenUsingOAuth(key);
      this._log.trace("Successfully got a token");
      return token;
    };

    try {
      let token, key;
      try {
        this._log.info("Getting sync key");
        key = await fxa.keys.getKeyForScope(SCOPE_OLD_SYNC);
        if (!key) {
          throw new Error("browser does not have the sync key, cannot sync");
        }
        token = await getToken(key);
      } catch (err) {
        // If we get a 401 fetching the token it may be that our auth tokens needed
        // to be regenerated; retry exactly once.
        if (!err.response || err.response.status !== 401) {
          throw err;
        }
        this._log.warn(
          "Token server returned 401, retrying token fetch with fresh credentials"
        );
        key = await fxa.keys.getKeyForScope(SCOPE_OLD_SYNC);
        token = await getToken(key);
      }
      // TODO: Make it be only 80% of the duration, so refresh the token
      // before it actually expires. This is to avoid sync storage errors
      // otherwise, we may briefly enter a "needs reauthentication" state.
      // (XXX - the above may no longer be true - someone should check ;)
      token.expiration = this._now() + token.duration * 1000 * 0.8;
      if (!this._syncKeyBundle) {
        this._syncKeyBundle = lazy.BulkKeyBundle.fromJWK(key);
      }
      lazy.Weave.Status.login = LOGIN_SUCCEEDED;
      this._token = token;
      return token;
    } catch (caughtErr) {
      let err = caughtErr; // The error we will rethrow.

      // TODO: unify these errors - we need to handle errors thrown by
      // both tokenserverclient and hawkclient.
      // A tokenserver error thrown based on a bad response.
      if (err.response && err.response.status === 401) {
        err = new AuthenticationError(err, "tokenserver");
        // A hawkclient error.
      } else if (err.code && err.code === 401) {
        err = new AuthenticationError(err, "hawkclient");
        // An FxAccounts.jsm error.
      } else if (err.message == fxAccountsCommon.ERROR_AUTH_ERROR) {
        err = new AuthenticationError(err, "fxaccounts");
      }

      // TODO: write tests to make sure that different auth error cases are handled here
      // properly: auth error getting oauth token, auth error getting sync token (invalid
      // generation or client-state error)
      if (err instanceof AuthenticationError) {
        this._log.error("Authentication error in _fetchTokenForUser", err);
        // set it to the "fatal" LOGIN_FAILED_LOGIN_REJECTED reason.
        lazy.Weave.Status.login = LOGIN_FAILED_LOGIN_REJECTED;
      } else {
        this._log.error("Non-authentication error in _fetchTokenForUser", err);
        // for now assume it is just a transient network related problem
        // (although sadly, it might also be a regular unhandled exception)
        lazy.Weave.Status.login = LOGIN_FAILED_NETWORK_ERROR;
      }
      throw err;
    }
  },

  /**
   * Generates an OAuth access_token using the OLD_SYNC scope and exchanges it
   * for a TokenServer token.
   *
   * @returns {Promise}
   * @private
   */
  async _fetchTokenUsingOAuth(key) {
    this._log.debug("Getting a token using OAuth");
    const fxa = this._fxaService;
    const ttl = fxAccountsCommon.OAUTH_TOKEN_FOR_SYNC_LIFETIME_SECONDS;
    const accessToken = await fxa.getOAuthToken({ scope: SCOPE_OLD_SYNC, ttl });
    const headers = {
      "X-KeyId": key.kid,
    };

    return this._tokenServerClient
      .getTokenUsingOAuth(this._tokenServerUrl, accessToken, headers)
      .catch(async err => {
        if (err.response && err.response.status === 401) {
          // remove the cached token if we cannot authorize with it.
          // we have to do this here because we know which `token` to remove
          // from cache.
          await fxa.removeCachedOAuthToken({ token: accessToken });
        }

        // continue the error chain, so other handlers can deal with the error.
        throw err;
      });
  },

  // Returns a promise that is resolved with a valid token for the current
  // user, or rejects if one can't be obtained.
  // NOTE: This does all the authentication for Sync - it both sets the
  // key bundle (ie, decryption keys) and does the token fetch. These 2
  // concepts could be decoupled, but there doesn't seem any value in that
  // currently.
  async _ensureValidToken(forceNewToken = false) {
    let signedInUser = await this.getSignedInUser();
    if (!signedInUser) {
      throw new Error("no user is logged in");
    }
    if (!signedInUser.verified) {
      throw new Error("user is not verified");
    }

    await this.asyncObserver.promiseObserversComplete();

    if (!forceNewToken && this._hasValidToken()) {
      this._log.trace("_ensureValidToken already has one");
      return this._token;
    }

    // We are going to grab a new token - re-use the same promise if we are
    // already fetching one.
    if (!this._ensureValidTokenPromise) {
      this._ensureValidTokenPromise = this.__ensureValidToken().finally(() => {
        this._ensureValidTokenPromise = null;
      });
    }
    return this._ensureValidTokenPromise;
  },

  async __ensureValidToken() {
    // reset this._token as a safety net to reduce the possibility of us
    // repeatedly attempting to use an invalid token if _fetchTokenForUser throws.
    this._token = null;
    try {
      let token = await this._fetchTokenForUser();
      this._token = token;
      // This is a little bit of a hack. The tokenserver tells us a HMACed version
      // of the FxA uid which we can use for metrics purposes without revealing the
      // user's true uid. It conceptually belongs to FxA but we get it from tokenserver
      // for legacy reasons. Hand it back to the FxA client code to deal with.
      this._fxaService.telemetry._setHashedUID(token.hashed_fxa_uid);
      return token;
    } finally {
      Services.obs.notifyObservers(null, "weave:service:login:got-hashed-id");
    }
  },

  getResourceAuthenticator() {
    return this._getAuthenticationHeader.bind(this);
  },

  /**
   * @return a Hawk HTTP Authorization Header, lightly wrapped, for the .uri
   * of a RESTRequest or AsyncResponse object.
   */
  async _getAuthenticationHeader(httpObject, method) {
    // Note that in failure states we return null, causing the request to be
    // made without authorization headers, thereby presumably causing a 401,
    // which causes Sync to log out. If we throw, this may not happen as
    // expected.
    try {
      await this._ensureValidToken();
    } catch (ex) {
      this._log.error("Failed to fetch a token for authentication", ex);
      return null;
    }
    if (!this._token) {
      return null;
    }
    let credentials = { id: this._token.id, key: this._token.key };
    method = method || httpObject.method;

    // Get the local clock offset from the Firefox Accounts server.  This should
    // be close to the offset from the storage server.
    let options = {
      now: this._now(),
      localtimeOffsetMsec: this._localtimeOffsetMsec,
      credentials,
    };

    let headerValue = await CryptoUtils.computeHAWK(
      httpObject.uri,
      method,
      options
    );
    return { headers: { authorization: headerValue.field } };
  },

  /**
   * Determine the cluster for the current user and update state.
   * Returns true if a new cluster URL was found and it is different from
   * the existing cluster URL, false otherwise.
   */
  async setCluster() {
    // Make sure we didn't get some unexpected response for the cluster.
    let cluster = await this._findCluster();
    this._log.debug("Cluster value = " + cluster);
    if (cluster == null) {
      return false;
    }

    // Convert from the funky "String object with additional properties" that
    // resource.js returns to a plain-old string.
    cluster = cluster.toString();
    // Don't update stuff if we already have the right cluster
    if (cluster == lazy.Weave.Service.clusterURL) {
      return false;
    }

    this._log.debug("Setting cluster to " + cluster);
    lazy.Weave.Service.clusterURL = cluster;

    return true;
  },

  async _findCluster() {
    try {
      // Ensure we are ready to authenticate and have a valid token.
      // We need to handle node reassignment here.  If we are being asked
      // for a clusterURL while the service already has a clusterURL, then
      // it's likely a 401 was received using the existing token - in which
      // case we just discard the existing token and fetch a new one.
      let forceNewToken = false;
      if (lazy.Weave.Service.clusterURL) {
        this._log.debug(
          "_findCluster has a pre-existing clusterURL, so fetching a new token token"
        );
        forceNewToken = true;
      }
      let token = await this._ensureValidToken(forceNewToken);
      let endpoint = token.endpoint;
      // For Sync 1.5 storage endpoints, we use the base endpoint verbatim.
      // However, it should end in "/" because we will extend it with
      // well known path components. So we add a "/" if it's missing.
      if (!endpoint.endsWith("/")) {
        endpoint += "/";
      }
      this._log.debug("_findCluster returning " + endpoint);
      return endpoint;
    } catch (err) {
      this._log.info("Failed to fetch the cluster URL", err);
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
        return null;
      }
      throw err;
    }
  },
};
