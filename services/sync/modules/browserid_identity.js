/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["BrowserIDManager", "AuthenticationError"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Log.jsm");
ChromeUtils.import("resource://gre/modules/FxAccounts.jsm");
ChromeUtils.import("resource://services-common/async.js");
ChromeUtils.import("resource://services-common/utils.js");
ChromeUtils.import("resource://services-common/tokenserverclient.js");
ChromeUtils.import("resource://services-crypto/utils.js");
ChromeUtils.import("resource://services-sync/util.js");
ChromeUtils.import("resource://services-sync/constants.js");

// Lazy imports to prevent unnecessary load on startup.
ChromeUtils.defineModuleGetter(this, "Weave",
                               "resource://services-sync/main.js");

ChromeUtils.defineModuleGetter(this, "BulkKeyBundle",
                               "resource://services-sync/keys.js");

ChromeUtils.defineModuleGetter(this, "fxAccounts",
                               "resource://gre/modules/FxAccounts.jsm");

XPCOMUtils.defineLazyGetter(this, "log", function() {
  let log = Log.repository.getLogger("Sync.BrowserIDManager");
  log.manageLevelFromPref("services.sync.log.logger.identity");
  return log;
});

XPCOMUtils.defineLazyPreferenceGetter(this, "IGNORE_CACHED_AUTH_CREDENTIALS",
                                      "services.sync.debug.ignoreCachedAuthCredentials");

// FxAccountsCommon.js doesn't use a "namespace", so create one here.
var fxAccountsCommon = {};
ChromeUtils.import("resource://gre/modules/FxAccountsCommon.js", fxAccountsCommon);

const OBSERVER_TOPICS = [
  fxAccountsCommon.ONLOGIN_NOTIFICATION,
  fxAccountsCommon.ONVERIFIED_NOTIFICATION,
  fxAccountsCommon.ONLOGOUT_NOTIFICATION,
  fxAccountsCommon.ON_ACCOUNT_STATE_CHANGE_NOTIFICATION,
];

// A telemetry helper that records how long a user was in a "bad" state.
// It is recorded in the *main* ping, *not* the Sync ping.
// These bad states may persist across browser restarts, and may never change
// (eg, users may *never* validate)
var telemetryHelper = {
  // These are both the "status" values passed to maybeRecordLoginState and
  // the key we use for our keyed scalar.
  STATES: {
    SUCCESS: "SUCCESS",
    NOTVERIFIED: "NOTVERIFIED",
    REJECTED: "REJECTED",
  },

  PREFS: {
    REJECTED_AT: "identity.telemetry.loginRejectedAt",
    APPEARS_PERMANENTLY_REJECTED: "identity.telemetry.loginAppearsPermanentlyRejected",
    LAST_RECORDED_STATE: "identity.telemetry.lastRecordedState",
  },

  // How long, in minutes, that we continue to wait for a user to transition
  // from a "bad" state to a success state. After this has expired, we record
  // the "how long were they rejected for?" histogram.
  NUM_MINUTES_TO_RECORD_REJECTED_TELEMETRY: 20160, // 14 days in minutes.

  SCALAR: "services.sync.sync_login_state_transitions", // The scalar we use to report

  maybeRecordLoginState(status) {
    try {
      this._maybeRecordLoginState(status);
    } catch (ex) {
      log.error("Failed to record login telemetry", ex);
    }
  },

  _maybeRecordLoginState(status) {
    let key = this.STATES[status];
    if (!key) {
      throw new Error(`invalid state ${status}`);
    }

    let when = Svc.Prefs.get(this.PREFS.REJECTED_AT);
    let howLong = when ? this.nowInMinutes() - when : 0; // minutes.
    let isNewState = Svc.Prefs.get(this.PREFS.LAST_RECORDED_STATE) != status;

    if (status == this.STATES.SUCCESS) {
      if (isNewState) {
        Services.telemetry.keyedScalarSet(this.SCALAR, key, true);
        Svc.Prefs.set(this.PREFS.LAST_RECORDED_STATE, status);
      }
      // If we previously recorded an error state, report how long they were
      // in the bad state for (in minutes)
      if (when) {
        // If we are "permanently rejected" we've already recorded for how
        // long, so don't do it again.
        if (!Svc.Prefs.get(this.PREFS.APPEARS_PERMANENTLY_REJECTED)) {
          Services.telemetry.getHistogramById("WEAVE_LOGIN_FAILED_FOR").add(howLong);
        }
      }
      Svc.Prefs.reset(this.PREFS.REJECTED_AT);
      Svc.Prefs.reset(this.PREFS.APPEARS_PERMANENTLY_REJECTED);
    } else {
      // We are in a failure state.
      if (Svc.Prefs.get(this.PREFS.APPEARS_PERMANENTLY_REJECTED)) {
        return; // we've given up, so don't record errors.
      }
      if (isNewState) {
        Services.telemetry.keyedScalarSet(this.SCALAR, key, true);
        Svc.Prefs.set(this.PREFS.LAST_RECORDED_STATE, status);
      }
      if (howLong > this.NUM_MINUTES_TO_RECORD_REJECTED_TELEMETRY) {
        // We are giving up for this user, so report this "max time" as how
        // long they were in this state for.
        Services.telemetry.getHistogramById("WEAVE_LOGIN_FAILED_FOR").add(howLong);
        Svc.Prefs.set(this.PREFS.APPEARS_PERMANENTLY_REJECTED, true);
      }
      if (!Svc.Prefs.has(this.PREFS.REJECTED_AT)) {
        Svc.Prefs.set(this.PREFS.REJECTED_AT, this.nowInMinutes());
      }
    }
  },

  // hookable by tests.
  nowInMinutes() {
    return Math.floor(Date.now() / 1000 / 60);
  },
};

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
  }
};

function BrowserIDManager() {
  // NOTE: _fxaService and _tokenServerClient are replaced with mocks by
  // the test suite.
  this._fxaService = fxAccounts;
  this._tokenServerClient = new TokenServerClient();
  this._tokenServerClient.observerPrefix = "weave:service";
  this._log = log;
  XPCOMUtils.defineLazyPreferenceGetter(this, "_username", "services.sync.username");

  this.asyncObserver = Async.asyncObserver(this, log);
  for (let topic of OBSERVER_TOPICS) {
    Services.obs.addObserver(this.asyncObserver, topic);
  }
}

this.BrowserIDManager.prototype = {
  _fxaService: null,
  _tokenServerClient: null,
  // https://docs.services.mozilla.com/token/apis.html
  _token: null,
  _signedInUser: null, // the signedinuser we got from FxAccounts.

  hashedUID() {
    if (!this._hashedUID) {
      throw new Error("hashedUID: Don't seem to have previously seen a token");
    }
    return this._hashedUID;
  },

  // Return a hashed version of a deviceID, suitable for telemetry.
  hashedDeviceID(deviceID) {
    let uid = this.hashedUID();
    // Combine the raw device id with the metrics uid to create a stable
    // unique identifier that can't be mapped back to the user's FxA
    // identity without knowing the metrics HMAC key.
    return Utils.sha256(deviceID + uid);
  },

  finalize() {
    // After this is called, we can expect Service.identity != this.
    for (let topic of OBSERVER_TOPICS) {
      Services.obs.removeObserver(this.asyncObserver, topic);
    }
    this.resetCredentials();
    this._signedInUser = null;
  },

  _updateSignedInUser(userData) {
    // This object should only ever be used for a single user.  It is an
    // error to update the data if the user changes (but updates are still
    // necessary, as each call may add more attributes to the user).
    // We start with no user, so an initial update is always ok.
    if (this._signedInUser && this._signedInUser.uid != userData.uid) {
      throw new Error("Attempting to update to a different user.");
    }
    this._signedInUser = userData;
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
    switch (topic) {
    case fxAccountsCommon.ONLOGIN_NOTIFICATION: {
      this._log.info("A user has logged in");
      this.resetCredentials();
      let accountData = await this._fxaService.getSignedInUser();
      this._updateSignedInUser(accountData);

      if (!accountData.verified) {
        // wait for a verified notification before we kick sync off.
        this._log.info("The user is not verified");
        break;
      }
      // intentional fall-through - the user is verified.
    }
    // We've been configured with an already verified user, so fall-through.
    case fxAccountsCommon.ONVERIFIED_NOTIFICATION: {
      this._log.info("The user became verified");

      // Set the username now - that will cause Sync to know it is configured
      let accountData = await this._fxaService.getSignedInUser();
      this.username = accountData.email;
      Weave.Status.login = LOGIN_SUCCEEDED;

      // And actually sync. If we've never synced before, we force a full sync.
      // If we have, then we are probably just reauthenticating so it's a normal sync.
      // We can use any pref that must be set if we've synced before, and check
      // the sync lock state because we might already be doing that first sync.
      let isFirstSync = !Weave.Service.locked && !Svc.Prefs.get("client.syncID", null);
      if (isFirstSync) {
        this._log.info("Doing initial sync actions");
        Svc.Prefs.set("firstSync", "resetClient");
        Services.obs.notifyObservers(null, "weave:service:setup-complete");
      }
      // There's no need to wait for sync to complete and it would deadlock
      // our AsyncObserver.
      Weave.Service.sync({why: "login"});
      break;
    }

    case fxAccountsCommon.ONLOGOUT_NOTIFICATION:
      Weave.Service.startOver().then(() => {
        this._log.trace("startOver completed");
      }).catch(err => {
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
    return this._fxaService.now();
  },

  get _localtimeOffsetMsec() {
    return this._fxaService.localtimeOffsetMsec;
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
    this.resetCredentials();
  },

  /**
   * Resets/Drops all credentials we hold for the current user.
   */
  resetCredentials() {
    this._syncKeyBundle = null;
    this._token = null;
    this._hashedUID = null;
    // The cluster URL comes from the token, so resetting it to empty will
    // force Sync to not accidentally use a value from an earlier token.
    Weave.Service.clusterURL = null;
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
   * Deletes Sync credentials from the password manager.
   */
  deleteSyncCredentials() {
    for (let host of Utils.getSyncCredentialsHosts()) {
      let logins = Services.logins.findLogins({}, host, "", "");
      for (let login of logins) {
        Services.logins.removeLogin(login);
      }
    }
  },

  /**
   * Verify the current auth state, unlocking the master-password if necessary.
   *
   * Returns a promise that resolves with the current auth state after
   * attempting to unlock.
   */
  async unlockAndVerifyAuthState() {
    let data = await this._fxaService.getSignedInUser();
    if (!data) {
      log.debug("unlockAndVerifyAuthState has no user");
      return LOGIN_FAILED_NO_USERNAME;
    }
    if (!data.verified) {
      // Treat not verified as if the user needs to re-auth, so the browser
      // UI reflects the state.
      log.debug("unlockAndVerifyAuthState has an unverified user");
      telemetryHelper.maybeRecordLoginState(telemetryHelper.STATES.NOTVERIFIED);
      return LOGIN_FAILED_LOGIN_REJECTED;
    }
    this._updateSignedInUser(data);
    if ((await this._fxaService.canGetKeys())) {
      log.debug("unlockAndVerifyAuthState already has (or can fetch) sync keys");
      telemetryHelper.maybeRecordLoginState(telemetryHelper.STATES.SUCCESS);
      return STATUS_OK;
    }
    // so no keys - ensure MP unlocked.
    if (!Utils.ensureMPUnlocked()) {
      // user declined to unlock, so we don't know if they are stored there.
      log.debug("unlockAndVerifyAuthState: user declined to unlock master-password");
      return MASTER_PASSWORD_LOCKED;
    }
    // now we are unlocked we must re-fetch the user data as we may now have
    // the details that were previously locked away.
    this._updateSignedInUser(await this._fxaService.getSignedInUser());
    // If we still can't get keys it probably means the user authenticated
    // without unlocking the MP or cleared the saved logins, so we've now
    // lost them - the user will need to reauth before continuing.
    let result;
    if ((await this._fxaService.canGetKeys())) {
      telemetryHelper.maybeRecordLoginState(telemetryHelper.STATES.SUCCESS);
      result = STATUS_OK;
    } else {
      telemetryHelper.maybeRecordLoginState(telemetryHelper.STATES.REJECTED);
      result = LOGIN_FAILED_LOGIN_REJECTED;
    }
    log.debug("unlockAndVerifyAuthState re-fetched credentials and is returning", result);
    return result;
  },

  /**
   * Do we have a non-null, not yet expired token for the user currently
   * signed in?
   */
  _hasValidToken() {
    // If pref is set to ignore cached authentication credentials for debugging,
    // then return false to force the fetching of a new token.
    if (IGNORE_CACHED_AUTH_CREDENTIALS) {
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
    while (url.endsWith("/")) { // trailing slashes cause problems...
      url = url.slice(0, -1);
    }
    return url;
  },

  // Refresh the sync token for our user. Returns a promise that resolves
  // with a token, or rejects with an error.
  async _fetchTokenForUser() {
    // gotta be verified to fetch a token.
    if (!this._signedInUser.verified) {
      throw new Error("User is not verified");
    }

    // We need keys for things to work.  If we don't have them, just
    // return null for the token - sync calling unlockAndVerifyAuthState()
    // before actually syncing will setup the error states if necessary.
    if (!(await this._fxaService.canGetKeys())) {
      this._log.info("Unable to fetch keys (master-password locked?), so aborting token fetch");
      throw new Error("Can't fetch a token as we can't get keys");
    }

    // Do the assertion/certificate/token dance, with a retry.
    let getToken = async () => {
      this._log.info("Getting an assertion from", this._tokenServerUrl);
      const audience = Services.io.newURI(this._tokenServerUrl).prePath;
      const assertion = await this._fxaService.getAssertion(audience);

      this._log.debug("Getting a token");
      const headers = {"X-Client-State": this._signedInUser.kXCS};
      const token = await this._tokenServerClient.getTokenFromBrowserIDAssertion(
                            this._tokenServerUrl, assertion, headers);
      this._log.trace("Successfully got a token");
      return token;
    };

    let token;
    try {
      try {
        this._log.info("Getting keys");
        this._updateSignedInUser(await this._fxaService.getKeys()); // throws if the user changed.

        token = await getToken();
      } catch (err) {
        // If we get a 401 fetching the token it may be that our certificate
        // needs to be regenerated.
        if (!err.response || err.response.status !== 401) {
          throw err;
        }
        this._log.warn("Token server returned 401, refreshing certificate and retrying token fetch");
        await this._fxaService.invalidateCertificate();
        token = await getToken();
      }
      // TODO: Make it be only 80% of the duration, so refresh the token
      // before it actually expires. This is to avoid sync storage errors
      // otherwise, we may briefly enter a "needs reauthentication" state.
      // (XXX - the above may no longer be true - someone should check ;)
      token.expiration = this._now() + (token.duration * 1000) * 0.80;
      if (!this._syncKeyBundle) {
        this._syncKeyBundle = BulkKeyBundle.fromHexKey(this._signedInUser.kSync);
      }
      telemetryHelper.maybeRecordLoginState(telemetryHelper.STATES.SUCCESS);
      Weave.Status.login = LOGIN_SUCCEEDED;
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
      // properly: auth error getting assertion, auth error getting token (invalid generation
      // and client-state error)
      if (err instanceof AuthenticationError) {
        this._log.error("Authentication error in _fetchTokenForUser", err);
        // set it to the "fatal" LOGIN_FAILED_LOGIN_REJECTED reason.
        Weave.Status.login = LOGIN_FAILED_LOGIN_REJECTED;
        telemetryHelper.maybeRecordLoginState(telemetryHelper.STATES.REJECTED);
      } else {
        this._log.error("Non-authentication error in _fetchTokenForUser", err);
        // for now assume it is just a transient network related problem
        // (although sadly, it might also be a regular unhandled exception)
        Weave.Status.login = LOGIN_FAILED_NETWORK_ERROR;
      }
      throw err;
    }
  },

  // Returns a promise that is resolved with a valid token for the current
  // user, or rejects if one can't be obtained.
  // NOTE: This does all the authentication for Sync - it both sets the
  // key bundle (ie, decryption keys) and does the token fetch. These 2
  // concepts could be decoupled, but there doesn't seem any value in that
  // currently.
  async _ensureValidToken(forceNewToken = false) {
    if (!this._signedInUser) {
      throw new Error("no user is logged in");
    }
    if (!this._signedInUser.verified) {
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
      // we store the hashed UID from the token so that if we see a transient
      // error fetching a new token we still know the "most recent" hashed
      // UID for telemetry.
      this._hashedUID = token.hashed_fxa_uid;
      return token;
    } finally {
      Services.obs.notifyObservers(null, "weave:service:login:change");
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
      credentials,
    };

    let headerValue = CryptoUtils.computeHAWK(httpObject.uri, method, options);
    return {headers: {authorization: headerValue.field}};
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
    if (cluster == Weave.Service.clusterURL) {
      return false;
    }

    this._log.debug("Setting cluster to " + cluster);
    Weave.Service.clusterURL = cluster;

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
      if (Weave.Service.clusterURL) {
        this._log.debug("_findCluster has a pre-existing clusterURL, so fetching a new token token");
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
