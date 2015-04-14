/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["fxAccounts", "FxAccounts"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-crypto/utils.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");

XPCOMUtils.defineLazyModuleGetter(this, "FxAccountsClient",
  "resource://gre/modules/FxAccountsClient.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "jwcrypto",
  "resource://gre/modules/identity/jwcrypto.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FxAccountsOAuthGrantClient",
  "resource://gre/modules/FxAccountsOAuthGrantClient.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FxAccountsProfile",
  "resource://gre/modules/FxAccountsProfile.jsm");

// All properties exposed by the public FxAccounts API.
let publicProperties = [
  "accountStatus",
  "getAccountsClient",
  "getAccountsSignInURI",
  "getAccountsSignUpURI",
  "getAssertion",
  "getKeys",
  "getSignedInUser",
  "getOAuthToken",
  "getSignedInUserProfile",
  "loadAndPoll",
  "localtimeOffsetMsec",
  "now",
  "promiseAccountsForceSigninURI",
  "promiseAccountsChangeProfileURI",
  "removeCachedOAuthToken",
  "resendVerificationEmail",
  "setSignedInUser",
  "signOut",
  "version",
  "whenVerified"
];

// An AccountState object holds all state related to one specific account.
// Only one AccountState is ever "current" in the FxAccountsInternal object -
// whenever a user logs out or logs in, the current AccountState is discarded,
// making it impossible for the wrong state or state data to be accidentally
// used.
// In addition, it has some promise-related helpers to ensure that if an
// attempt is made to resolve a promise on a "stale" state (eg, if an
// operation starts, but a different user logs in before the operation
// completes), the promise will be rejected.
// It is intended to be used thusly:
// somePromiseBasedFunction: function() {
//   let currentState = this.currentAccountState;
//   return someOtherPromiseFunction().then(
//     data => currentState.resolve(data)
//   );
// }
// If the state has changed between the function being called and the promise
// being resolved, the .resolve() call will actually be rejected.
let AccountState = function(fxaInternal, signedInUserStorage, accountData = null) {
  this.fxaInternal = fxaInternal;
  this.signedInUserStorage = signedInUserStorage;
  this.signedInUser = accountData ? {version: DATA_FORMAT_VERSION, accountData} : null;
  this.uid = accountData ? accountData.uid : null;
  this.oauthTokens = {};
};

AccountState.prototype = {
  cert: null,
  keyPair: null,
  signedInUser: null,
  oauthTokens: null,
  whenVerifiedDeferred: null,
  whenKeysReadyDeferred: null,
  profile: null,
  promiseInitialAccountData: null,
  uid: null,

  get isCurrent() this.fxaInternal && this.fxaInternal.currentAccountState === this,

  abort: function() {
    if (this.whenVerifiedDeferred) {
      this.whenVerifiedDeferred.reject(
        new Error("Verification aborted; Another user signing in"));
      this.whenVerifiedDeferred = null;
    }

    if (this.whenKeysReadyDeferred) {
      this.whenKeysReadyDeferred.reject(
        new Error("Verification aborted; Another user signing in"));
      this.whenKeysReadyDeferred = null;
    }

    this.cert = null;
    this.keyPair = null;
    this.signedInUser = null;
    this.uid = null;
    this.fxaInternal = null;
    this.initProfilePromise = null;

    if (this.profile) {
      this.profile.tearDown();
      this.profile = null;
    }
  },

  // Clobber all cached data and write that empty data to storage.
  signOut() {
    this.cert = null;
    this.keyPair = null;
    this.signedInUser = null;
    this.oauthTokens = {};
    this.uid = null;
    return this.persistUserData();
  },

  getUserAccountData() {
    if (!this.isCurrent) {
      return this.reject(new Error("Another user has signed in"));
    }
    if (this.promiseInitialAccountData) {
      // We are still reading the data for the first and only time.
      return this.promiseInitialAccountData;
    }
    // We've previously read it successfully (and possibly updated it since)
    if (this.signedInUser) {
      return this.resolve(this.signedInUser.accountData);
    }

    // We fetch the signedInUser data first, then fetch the token store and
    // ensure the uid in the tokens matches our user.
    let accountData = null;
    let oauthTokens = {};
    return this.promiseInitialAccountData = this.signedInUserStorage.get()
      .then(user => {
        if (logPII) {
          log.debug("getUserAccountData", user);
        }
        // In an ideal world we could cache the data in this.signedInUser, but
        // if we do, the interaction with the login manager breaks when the
        // password is locked as this read may only have obtained partial data.
        // Therefore every read *must* really read incase the login manager is
        // now unlocked. We could fix this with a refactor...
        accountData = user ? user.accountData : null;
      }, err => {
        // Error reading signed in user account data.
        this.promiseInitialAccountData = null;
        if (err instanceof OS.File.Error && err.becauseNoSuchFile) {
          // File hasn't been created yet.  That will be done
          // on the first call to setSignedInUser
          return;
        }
        // something else went wrong - report the error but continue without
        // user data.
        log.error("Failed to read signed in user data", err);
      }).then(() => {
        if (!accountData) {
          return null;
        }
        return this.signedInUserStorage.getOAuthTokens();
      }).then(tokenData => {
        if (tokenData && tokenData.tokens &&
            tokenData.version == DATA_FORMAT_VERSION &&
            tokenData.uid == accountData.uid ) {
          oauthTokens = tokenData.tokens;
        }
      }, err => {
        // Error reading the OAuth tokens file.
        if (err instanceof OS.File.Error && err.becauseNoSuchFile) {
          // File hasn't been created yet, but will be when tokens are saved.
          return;
        }
        log.error("Failed to read oauth tokens", err)
      }).then(() => {
        // We are done - clear our promise and save the data if we are still
        // current.
        this.promiseInitialAccountData = null;
        if (this.isCurrent) {
          // As above, we can not cache the data to this.signedInUser as we
          // may only have partial data due to a locked MP, so the next
          // request must re-read incase it is now unlocked.
          // But we do save the tokens and the uid
          this.oauthTokens = oauthTokens;
          this.uid = accountData ? accountData.uid : null;
        }
        return accountData;
      });
      // phew!
  },

  // XXX - this should really be called "updateCurrentUserData" or similar as
  // it is only ever used to add new fields to the *current* user, not to
  // set a new user as current.
  setUserAccountData: function(accountData) {
    if (!this.isCurrent) {
      return this.reject(new Error("Another user has signed in"));
    }
    if (this.promiseInitialAccountData) {
      throw new Error("Can't set account data before it's been read.");
    }
    if (!accountData) {
      // see above - this should really be called "updateCurrentUserData" or similar.
      throw new Error("Attempt to use setUserAccountData with null user data.");
    }
    if (accountData.uid != this.uid) {
      // see above - this should really be called "updateCurrentUserData" or similar.
      throw new Error("Attempt to use setUserAccountData with a different user.");
    }
    // Set our signedInUser before we start the write, so any updates to the
    // data while the write completes are still captured.
    this.signedInUser = {version: DATA_FORMAT_VERSION, accountData: accountData};
    return this.signedInUserStorage.set(this.signedInUser)
        .then(() => this.resolve(accountData));
  },


  getCertificate: function(data, keyPair, mustBeValidUntil) {
    if (logPII) {
      // don't stringify unless it will be written. We should replace this
      // check with param substitutions added in bug 966674
      log.debug("getCertificate" + JSON.stringify(this.signedInUser));
    }
    // TODO: get the lifetime from the cert's .exp field
    if (this.cert && this.cert.validUntil > mustBeValidUntil) {
      log.debug(" getCertificate already had one");
      return this.resolve(this.cert.cert);
    }

    if (Services.io.offline) {
      return this.reject(new Error(ERROR_OFFLINE));
    }

    let willBeValidUntil = this.fxaInternal.now() + CERT_LIFETIME;
    return this.fxaInternal.getCertificateSigned(data.sessionToken,
                                                 keyPair.serializedPublicKey,
                                                 CERT_LIFETIME).then(
      cert => {
        log.debug("getCertificate got a new one: " + !!cert);
        this.cert = {
          cert: cert,
          validUntil: willBeValidUntil
        };
        return cert;
      }
    ).then(result => this.resolve(result));
  },

  getKeyPair: function(mustBeValidUntil) {
    // If the debugging pref to ignore cached authentication credentials is set for Sync,
    // then don't use any cached key pair, i.e., generate a new one and get it signed.
    // The purpose of this pref is to expedite any auth errors as the result of a
    // expired or revoked FxA session token, e.g., from resetting or changing the FxA
    // password.
    let ignoreCachedAuthCredentials = false;
    try {
      ignoreCachedAuthCredentials = Services.prefs.getBoolPref("services.sync.debug.ignoreCachedAuthCredentials");
    } catch(e) {
      // Pref doesn't exist
    }
    if (!ignoreCachedAuthCredentials && this.keyPair && (this.keyPair.validUntil > mustBeValidUntil)) {
      log.debug("getKeyPair: already have a keyPair");
      return this.resolve(this.keyPair.keyPair);
    }
    // Otherwse, create a keypair and set validity limit.
    let willBeValidUntil = this.fxaInternal.now() + KEY_LIFETIME;
    let d = Promise.defer();
    jwcrypto.generateKeyPair("DS160", (err, kp) => {
      if (err) {
        return this.reject(err);
      }
      this.keyPair = {
        keyPair: kp,
        validUntil: willBeValidUntil
      };
      log.debug("got keyPair");
      delete this.cert;
      d.resolve(this.keyPair.keyPair);
    });
    return d.promise.then(result => this.resolve(result));
  },

  // Get the account's profile image URL from the profile server
  getProfile: function () {
    return this.initProfile()
      .then(() => this.profile.getProfile());
  },

  // Instantiate a FxAccountsProfile with a fresh OAuth token if needed
  initProfile: function () {

    let profileServerUrl = Services.urlFormatter.formatURLPref("identity.fxaccounts.remote.profile.uri");

    let oAuthOptions = {
      scope: "profile"
    };

    if (this.initProfilePromise) {
      return this.initProfilePromise;
    }

    this.initProfilePromise = this.fxaInternal.getOAuthToken(oAuthOptions)
      .then(token => {
        this.profile = new FxAccountsProfile(this, {
          profileServerUrl: profileServerUrl,
          token: token
        });
        this.initProfilePromise = null;
      })
      .then(null, err => {
        this.initProfilePromise = null;
        throw err;
      });

    return this.initProfilePromise;
  },

  resolve: function(result) {
    if (!this.isCurrent) {
      log.info("An accountState promise was resolved, but was actually rejected" +
               " due to a different user being signed in. Originally resolved" +
               " with: " + result);
      return Promise.reject(new Error("A different user signed in"));
    }
    return Promise.resolve(result);
  },

  reject: function(error) {
    // It could be argued that we should just let it reject with the original
    // error - but this runs the risk of the error being (eg) a 401, which
    // might cause the consumer to attempt some remediation and cause other
    // problems.
    if (!this.isCurrent) {
      log.info("An accountState promise was rejected, but we are ignoring that" +
               "reason and rejecting it due to a different user being signed in." +
               "Originally rejected with: " + error);
      return Promise.reject(new Error("A different user signed in"));
    }
    return Promise.reject(error);
  },

  // Abstractions for storage of cached tokens - these are all sync, and don't
  // handle revocation etc - it's just storage.

  // A preamble for the cache helpers...
  _cachePreamble() {
    if (!this.isCurrent) {
      throw new Error("Another user has signed in");
    }
  },

  // Set a cached token. |tokenData| must have a 'token' element, but may also
  // have additional fields (eg, it probably specifies the server to revoke
  // from). The 'get' functions below return the entire |tokenData| value.
  setCachedToken(scopeArray, tokenData) {
    this._cachePreamble();
    if (!tokenData.token) {
      throw new Error("No token");
    }
    let key = getScopeKey(scopeArray);
    this.oauthTokens[key] = tokenData;
    // And a background save...
    this._persistCachedTokens();
  },

  // Return data for a cached token or null (or throws on bad state etc)
  getCachedToken(scopeArray) {
    this._cachePreamble();
    let key = getScopeKey(scopeArray);
    if (this.oauthTokens[key]) {
      // later we might want to check an expiry date - but we currently
      // have no such concept, so just return it.
      log.trace("getCachedToken returning cached token");
      return this.oauthTokens[key];
    }
    return null;
  },

  // Get an array of tokenData for all cached tokens.
  getAllCachedTokens() {
    this._cachePreamble();
    let result = [];
    for (let [key, tokenValue] in Iterator(this.oauthTokens)) {
      result.push(tokenValue);
    }
    return result;
  },

  // Remove a cached token from the cache.  Does *not* revoke it from anywhere.
  // Returns the entire token entry if found, null otherwise.
  removeCachedToken(token) {
    this._cachePreamble();
    let data = this.oauthTokens;
    for (let [key, tokenValue] in Iterator(data)) {
      if (tokenValue.token == token) {
        delete data[key];
        // And a background save...
        this._persistCachedTokens();
        return tokenValue;
      }
    }
    return null;
  },

  // A hook-point for tests.  Returns a promise that's ignored in most cases
  // (notable exceptions are tests and when we explicitly are saving the entire
  // set of user data.)
  _persistCachedTokens() {
    this._cachePreamble();
    let record;
    if (this.uid) {
      record = {
        version: DATA_FORMAT_VERSION,
        uid: this.uid,
        tokens: this.oauthTokens,
      };
    } else {
      record = null;
    }
    return this.signedInUserStorage.setOAuthTokens(record).catch(
      err => {
        log.error("Failed to save account data for token cache", err);
      }
    );
  },

  persistUserData() {
    return this._persistCachedTokens().catch(err => {
      log.error("Failed to persist cached tokens", err);
    }).then(() => {
      return this.signedInUserStorage.set(this.signedInUser);
    }).catch(err => {
      log.error("Failed to persist account data", err);
    });
  },
}

/* Given an array of scopes, make a string key by normalizing. */
function getScopeKey(scopeArray) {
  let normalizedScopes = scopeArray.map(item => item.toLowerCase());
  return normalizedScopes.sort().join("|");
}

/**
 * Copies properties from a given object to another object.
 *
 * @param from (object)
 *        The object we read property descriptors from.
 * @param to (object)
 *        The object that we set property descriptors on.
 * @param options (object) (optional)
 *        {keys: [...]}
 *          Lets the caller pass the names of all properties they want to be
 *          copied. Will copy all properties of the given source object by
 *          default.
 *        {bind: object}
 *          Lets the caller specify the object that will be used to .bind()
 *          all function properties we find to. Will bind to the given target
 *          object by default.
 */
function copyObjectProperties(from, to, opts = {}) {
  let keys = (opts && opts.keys) || Object.keys(from);
  let thisArg = (opts && opts.bind) || to;

  for (let prop of keys) {
    let desc = Object.getOwnPropertyDescriptor(from, prop);

    if (typeof(desc.value) == "function") {
      desc.value = desc.value.bind(thisArg);
    }

    if (desc.get) {
      desc.get = desc.get.bind(thisArg);
    }

    if (desc.set) {
      desc.set = desc.set.bind(thisArg);
    }

    Object.defineProperty(to, prop, desc);
  }
}

/**
 * The public API's constructor.
 */
this.FxAccounts = function (mockInternal) {
  let internal = new FxAccountsInternal();
  let external = {};

  // Copy all public properties to the 'external' object.
  let prototype = FxAccountsInternal.prototype;
  let options = {keys: publicProperties, bind: internal};
  copyObjectProperties(prototype, external, options);

  // Copy all of the mock's properties to the internal object.
  if (mockInternal && !mockInternal.onlySetInternal) {
    copyObjectProperties(mockInternal, internal);
  }

  if (mockInternal) {
    // A little work-around to ensure the initial currentAccountState has
    // the same mock storage the test passed in.
    if (mockInternal.signedInUserStorage) {
      internal.currentAccountState.signedInUserStorage = mockInternal.signedInUserStorage;
    }
    // Exposes the internal object for testing only.
    external.internal = internal;
  }

  return Object.freeze(external);
}

/**
 * The internal API's constructor.
 */
function FxAccountsInternal() {
  this.version = DATA_FORMAT_VERSION;

  // Make a local copy of this constant so we can mock it in testing
  this.POLL_SESSION = POLL_SESSION;

  // The one and only "storage" object.  While this is created here, the
  // FxAccountsInternal object does *not* use it directly, but instead passes
  // it to AccountState objects which has sole responsibility for storage.
  // Ideally we would create it in the AccountState objects, but that makes
  // testing hard as AccountState objects are regularly created and thrown
  // away. Doing it this way means tests can mock/replace this storage object
  // and have it used by all AccountState objects, even those created before
  // and after the mock has been setup.

  // We only want the fancy LoginManagerStorage on desktop.
#if defined(MOZ_B2G)
  this.signedInUserStorage = new JSONStorage({
#else
  this.signedInUserStorage = new LoginManagerStorage({
#endif
    // We don't reference |profileDir| in the top-level module scope
    // as we may be imported before we know where it is.
    filename: DEFAULT_STORAGE_FILENAME,
    oauthTokensFilename: DEFAULT_OAUTH_TOKENS_FILENAME,
    baseDir: OS.Constants.Path.profileDir,
  });

  // We interact with the Firefox Accounts auth server in order to confirm that
  // a user's email has been verified and also to fetch the user's keys from
  // the server.  We manage these processes in possibly long-lived promises
  // that are internal to this object (never exposed to callers).  Because
  // Firefox Accounts allows for only one logged-in user, and because it's
  // conceivable that while we are waiting to verify one identity, a caller
  // could start verification on a second, different identity, we need to be
  // able to abort all work on the first sign-in process.  The currentTimer and
  // currentAccountState are used for this purpose.
  // (XXX - should the timer be directly on the currentAccountState?)
  this.currentTimer = null;
  this.currentAccountState = new AccountState(this, this.signedInUserStorage);
}

/**
 * The internal API's prototype.
 */
FxAccountsInternal.prototype = {

  /**
   * The current data format's version number.
   */
  version: DATA_FORMAT_VERSION,

  // The timeout (in ms) we use to poll for a verified mail for the first 2 mins.
  VERIFICATION_POLL_TIMEOUT_INITIAL: 5000, // 5 seconds
  // And how often we poll after the first 2 mins.
  VERIFICATION_POLL_TIMEOUT_SUBSEQUENT: 15000, // 15 seconds.

  _fxAccountsClient: null,

  get fxAccountsClient() {
    if (!this._fxAccountsClient) {
      this._fxAccountsClient = new FxAccountsClient();
    }
    return this._fxAccountsClient;
  },

  /**
   * Return the current time in milliseconds as an integer.  Allows tests to
   * manipulate the date to simulate certificate expiration.
   */
  now: function() {
    return this.fxAccountsClient.now();
  },

  getAccountsClient: function() {
    return this.fxAccountsClient;
  },

  /**
   * Return clock offset in milliseconds, as reported by the fxAccountsClient.
   * This can be overridden for testing.
   *
   * The offset is the number of milliseconds that must be added to the client
   * clock to make it equal to the server clock.  For example, if the client is
   * five minutes ahead of the server, the localtimeOffsetMsec will be -300000.
   */
  get localtimeOffsetMsec() {
    return this.fxAccountsClient.localtimeOffsetMsec;
  },

  /**
   * Ask the server whether the user's email has been verified
   */
  checkEmailStatus: function checkEmailStatus(sessionToken) {
    return this.fxAccountsClient.recoveryEmailStatus(sessionToken);
  },

  /**
   * Once the user's email is verified, we can request the keys
   */
  fetchKeys: function fetchKeys(keyFetchToken) {
    log.debug("fetchKeys: " + !!keyFetchToken);
    if (logPII) {
      log.debug("fetchKeys - the token is " + keyFetchToken);
    }
    return this.fxAccountsClient.accountKeys(keyFetchToken);
  },

  // set() makes sure that polling is happening, if necessary.
  // get() does not wait for verification, and returns an object even if
  // unverified. The caller of get() must check .verified .
  // The "fxaccounts:onverified" event will fire only when the verified
  // state goes from false to true, so callers must register their observer
  // and then call get(). In particular, it will not fire when the account
  // was found to be verified in a previous boot: if our stored state says
  // the account is verified, the event will never fire. So callers must do:
  //   register notification observer (go)
  //   userdata = get()
  //   if (userdata.verified()) {go()}

  /**
   * Get the user currently signed in to Firefox Accounts.
   *
   * @return Promise
   *        The promise resolves to the credentials object of the signed-in user:
   *        {
   *          email: The user's email address
   *          uid: The user's unique id
   *          sessionToken: Session for the FxA server
   *          kA: An encryption key from the FxA server
   *          kB: An encryption key derived from the user's FxA password
   *          verified: email verification status
   *          authAt: The time (seconds since epoch) that this record was
   *                  authenticated
   *        }
   *        or null if no user is signed in.
   */
  getSignedInUser: function getSignedInUser() {
    let currentState = this.currentAccountState;
    return currentState.getUserAccountData().then(data => {
      if (!data) {
        return null;
      }
      if (!this.isUserEmailVerified(data)) {
        // If the email is not verified, start polling for verification,
        // but return null right away.  We don't want to return a promise
        // that might not be fulfilled for a long time.
        this.startVerifiedCheck(data);
      }
      return data;
    }).then(result => currentState.resolve(result));
  },

  /**
   * Set the current user signed in to Firefox Accounts.
   *
   * @param credentials
   *        The credentials object obtained by logging in or creating
   *        an account on the FxA server:
   *        {
   *          authAt: The time (seconds since epoch) that this record was
   *                  authenticated
   *          email: The users email address
   *          keyFetchToken: a keyFetchToken which has not yet been used
   *          sessionToken: Session for the FxA server
   *          uid: The user's unique id
   *          unwrapBKey: used to unwrap kB, derived locally from the
   *                      password (not revealed to the FxA server)
   *          verified: true/false
   *        }
   * @return Promise
   *         The promise resolves to null when the data is saved
   *         successfully and is rejected on error.
   */
  setSignedInUser: function setSignedInUser(credentials) {
    log.debug("setSignedInUser - aborting any existing flows");
    this.abortExistingFlow();

    let currentAccountState = this.currentAccountState = new AccountState(
      this,
      this.signedInUserStorage,
      JSON.parse(JSON.stringify(credentials)) // Pass a clone of the credentials object.
    );

    // This promise waits for storage, but not for verification.
    // We're telling the caller that this is durable now.
    return currentAccountState.persistUserData().then(() => {
      this.notifyObservers(ONLOGIN_NOTIFICATION);
      if (!this.isUserEmailVerified(credentials)) {
        this.startVerifiedCheck(credentials);
      }
    }).then(() => {
      return currentAccountState.resolve();
    });
  },

  /**
   * returns a promise that fires with the assertion.  If there is no verified
   * signed-in user, fires with null.
   */
  getAssertion: function getAssertion(audience) {
    log.debug("enter getAssertion()");
    let currentState = this.currentAccountState;
    let mustBeValidUntil = this.now() + ASSERTION_USE_PERIOD;
    return currentState.getUserAccountData().then(data => {
      if (!data) {
        // No signed-in user
        return null;
      }
      if (!this.isUserEmailVerified(data)) {
        // Signed-in user has not verified email
        return null;
      }
      return currentState.getKeyPair(mustBeValidUntil).then(keyPair => {
        return currentState.getCertificate(data, keyPair, mustBeValidUntil)
          .then(cert => {
            return this.getAssertionFromCert(data, keyPair, cert, audience);
          });
      });
    }).then(result => currentState.resolve(result));
  },

  /**
   * Resend the verification email fot the currently signed-in user.
   *
   */
  resendVerificationEmail: function resendVerificationEmail() {
    let currentState = this.currentAccountState;
    return this.getSignedInUser().then(data => {
      // If the caller is asking for verification to be re-sent, and there is
      // no signed-in user to begin with, this is probably best regarded as an
      // error.
      if (data) {
        this.pollEmailStatus(currentState, data.sessionToken, "start");
        return this.fxAccountsClient.resendVerificationEmail(data.sessionToken);
      }
      throw new Error("Cannot resend verification email; no signed-in user");
    });
  },

  /*
   * Reset state such that any previous flow is canceled.
   */
  abortExistingFlow: function abortExistingFlow() {
    if (this.currentTimer) {
      log.debug("Polling aborted; Another user signing in");
      clearTimeout(this.currentTimer);
      this.currentTimer = 0;
    }
    this.currentAccountState.abort();
    this.currentAccountState = new AccountState(this, this.signedInUserStorage);
  },

  accountStatus: function accountStatus() {
    return this.currentAccountState.getUserAccountData().then(data => {
      if (!data) {
        return false;
      }
      return this.fxAccountsClient.accountStatus(data.uid);
    });
  },

  _destroyOAuthToken: function(tokenData) {
    let client = new FxAccountsOAuthGrantClient({
      serverURL: tokenData.server,
      client_id: FX_OAUTH_CLIENT_ID
    });
    return client.destroyToken(tokenData.token)
  },

  _destroyAllOAuthTokens: function(tokenInfos) {
    // let's just destroy them all in parallel...
    let promises = [];
    for (let tokenInfo of tokenInfos) {
      promises.push(this._destroyOAuthToken(tokenInfo));
    }
    return Promise.all(promises);
  },

  signOut: function signOut(localOnly) {
    let currentState = this.currentAccountState;
    let sessionToken;
    let tokensToRevoke;
    return currentState.getUserAccountData().then(data => {
      // Save the session token for use in the call to signOut below.
      sessionToken = data && data.sessionToken;
      tokensToRevoke = currentState.getAllCachedTokens();
      return this._signOutLocal();
    }).then(() => {
      // FxAccountsManager calls here, then does its own call
      // to FxAccountsClient.signOut().
      if (!localOnly) {
        // Wrap this in a promise so *any* errors in signOut won't
        // block the local sign out. This is *not* returned.
        Promise.resolve().then(() => {
          // This can happen in the background and shouldn't block
          // the user from signing out. The server must tolerate
          // clients just disappearing, so this call should be best effort.
          return this._signOutServer(sessionToken);
        }).catch(err => {
          log.error("Error during remote sign out of Firefox Accounts", err);
        }).then(() => {
          return this._destroyAllOAuthTokens(tokensToRevoke);
        }).catch(err => {
          log.error("Error during destruction of oauth tokens during signout", err);
        }).then(() => {
          // just for testing - notifications are cheap when no observers.
          this.notifyObservers("testhelper-fxa-signout-complete");
        });
      }
    }).then(() => {
      this.notifyObservers(ONLOGOUT_NOTIFICATION);
    });
  },

  /**
   * This function should be called in conjunction with a server-side
   * signOut via FxAccountsClient.
   */
  _signOutLocal: function signOutLocal() {
    let currentAccountState = this.currentAccountState;
    return currentAccountState.signOut().then(() => {
      this.abortExistingFlow(); // this resets this.currentAccountState.
    });
  },

  _signOutServer: function signOutServer(sessionToken) {
    return this.fxAccountsClient.signOut(sessionToken);
  },

  /**
   * Fetch encryption keys for the signed-in-user from the FxA API server.
   *
   * Not for user consumption.  Exists to cause the keys to be fetch.
   *
   * Returns user data so that it can be chained with other methods.
   *
   * @return Promise
   *        The promise resolves to the credentials object of the signed-in user:
   *        {
   *          email: The user's email address
   *          uid: The user's unique id
   *          sessionToken: Session for the FxA server
   *          kA: An encryption key from the FxA server
   *          kB: An encryption key derived from the user's FxA password
   *          verified: email verification status
   *        }
   *        or null if no user is signed in
   */
  getKeys: function() {
    let currentState = this.currentAccountState;
    return currentState.getUserAccountData().then((userData) => {
      if (!userData) {
        throw new Error("Can't get keys; User is not signed in");
      }
      if (userData.kA && userData.kB) {
        return userData;
      }
      if (!currentState.whenKeysReadyDeferred) {
        currentState.whenKeysReadyDeferred = Promise.defer();
        if (userData.keyFetchToken) {
          this.fetchAndUnwrapKeys(userData.keyFetchToken).then(
            (dataWithKeys) => {
              if (!dataWithKeys.kA || !dataWithKeys.kB) {
                currentState.whenKeysReadyDeferred.reject(
                  new Error("user data missing kA or kB")
                );
                return;
              }
              currentState.whenKeysReadyDeferred.resolve(dataWithKeys);
            },
            (err) => {
              currentState.whenKeysReadyDeferred.reject(err);
            }
          );
        } else {
          currentState.whenKeysReadyDeferred.reject('No keyFetchToken');
        }
      }
      return currentState.whenKeysReadyDeferred.promise;
    }).then(result => currentState.resolve(result));
   },

  fetchAndUnwrapKeys: function(keyFetchToken) {
    if (logPII) {
      log.debug("fetchAndUnwrapKeys: token: " + keyFetchToken);
    }
    let currentState = this.currentAccountState;
    return Task.spawn(function* task() {
      // Sign out if we don't have a key fetch token.
      if (!keyFetchToken) {
        log.warn("improper fetchAndUnwrapKeys() call: token missing");
        yield this.signOut();
        return null;
      }

      let {kA, wrapKB} = yield this.fetchKeys(keyFetchToken);

      let data = yield currentState.getUserAccountData();

      // Sanity check that the user hasn't changed out from under us
      if (data.keyFetchToken !== keyFetchToken) {
        throw new Error("Signed in user changed while fetching keys!");
      }

      // Next statements must be synchronous until we setUserAccountData
      // so that we don't risk getting into a weird state.
      let kB_hex = CryptoUtils.xor(CommonUtils.hexToBytes(data.unwrapBKey),
                                   wrapKB);

      if (logPII) {
        log.debug("kB_hex: " + kB_hex);
      }
      data.kA = CommonUtils.bytesAsHex(kA);
      data.kB = CommonUtils.bytesAsHex(kB_hex);

      delete data.keyFetchToken;
      delete data.unwrapBKey;

      log.debug("Keys Obtained: kA=" + !!data.kA + ", kB=" + !!data.kB);
      if (logPII) {
        log.debug("Keys Obtained: kA=" + data.kA + ", kB=" + data.kB);
      }

      yield currentState.setUserAccountData(data);
      // We are now ready for business. This should only be invoked once
      // per setSignedInUser(), regardless of whether we've rebooted since
      // setSignedInUser() was called.
      this.notifyObservers(ONVERIFIED_NOTIFICATION);
      return data;
    }.bind(this)).then(result => currentState.resolve(result));
  },

  getAssertionFromCert: function(data, keyPair, cert, audience) {
    log.debug("getAssertionFromCert");
    let payload = {};
    let d = Promise.defer();
    let options = {
      duration: ASSERTION_LIFETIME,
      localtimeOffsetMsec: this.localtimeOffsetMsec,
      now: this.now()
    };
    let currentState = this.currentAccountState;
    // "audience" should look like "http://123done.org".
    // The generated assertion will expire in two minutes.
    jwcrypto.generateAssertion(cert, keyPair, audience, options, (err, signed) => {
      if (err) {
        log.error("getAssertionFromCert: " + err);
        d.reject(err);
      } else {
        log.debug("getAssertionFromCert returning signed: " + !!signed);
        if (logPII) {
          log.debug("getAssertionFromCert returning signed: " + signed);
        }
        d.resolve(signed);
      }
    });
    return d.promise.then(result => currentState.resolve(result));
  },

  getCertificateSigned: function(sessionToken, serializedPublicKey, lifetime) {
    log.debug("getCertificateSigned: " + !!sessionToken + " " + !!serializedPublicKey);
    if (logPII) {
      log.debug("getCertificateSigned: " + sessionToken + " " + serializedPublicKey);
    }
    return this.fxAccountsClient.signCertificate(
      sessionToken,
      JSON.parse(serializedPublicKey),
      lifetime
    );
  },

  getUserAccountData: function() {
    return this.currentAccountState.getUserAccountData();
  },

  isUserEmailVerified: function isUserEmailVerified(data) {
    return !!(data && data.verified);
  },

  /**
   * Setup for and if necessary do email verification polling.
   */
  loadAndPoll: function() {
    let currentState = this.currentAccountState;
    return currentState.getUserAccountData()
      .then(data => {
        if (data && !this.isUserEmailVerified(data)) {
          this.pollEmailStatus(currentState, data.sessionToken, "start");
        }
        return data;
      });
  },

  startVerifiedCheck: function(data) {
    log.debug("startVerifiedCheck", data && data.verified);
    if (logPII) {
      log.debug("startVerifiedCheck with user data", data);
    }

    // Get us to the verified state, then get the keys. This returns a promise
    // that will fire when we are completely ready.
    //
    // Login is truly complete once keys have been fetched, so once getKeys()
    // obtains and stores kA and kB, it will fire the onverified observer
    // notification.

    // The callers of startVerifiedCheck never consume a returned promise (ie,
    // this is simply kicking off a background fetch) so we must add a rejection
    // handler to avoid runtime warnings about the rejection not being handled.
    this.whenVerified(data).then(
      () => this.getKeys(),
      err => log.info("startVerifiedCheck promise was rejected: " + err)
    );
  },

  whenVerified: function(data) {
    let currentState = this.currentAccountState;
    if (data.verified) {
      log.debug("already verified");
      return currentState.resolve(data);
    }
    if (!currentState.whenVerifiedDeferred) {
      log.debug("whenVerified promise starts polling for verified email");
      this.pollEmailStatus(currentState, data.sessionToken, "start");
    }
    return currentState.whenVerifiedDeferred.promise.then(
      result => currentState.resolve(result)
    );
  },

  notifyObservers: function(topic, data) {
    log.debug("Notifying observers of " + topic);
    Services.obs.notifyObservers(null, topic, data);
  },

  // XXX - pollEmailStatus should maybe be on the AccountState object?
  pollEmailStatus: function pollEmailStatus(currentState, sessionToken, why) {
    log.debug("entering pollEmailStatus: " + why);
    if (why == "start") {
      // If we were already polling, stop and start again.  This could happen
      // if the user requested the verification email to be resent while we
      // were already polling for receipt of an earlier email.
      this.pollStartDate = Date.now();
      if (!currentState.whenVerifiedDeferred) {
        currentState.whenVerifiedDeferred = Promise.defer();
        // This deferred might not end up with any handlers (eg, if sync
        // is yet to start up.)  This might cause "A promise chain failed to
        // handle a rejection" messages, so add an error handler directly
        // on the promise to log the error.
        currentState.whenVerifiedDeferred.promise.then(null, err => {
          log.info("the wait for user verification was stopped: " + err);
        });
      }
    }

    this.checkEmailStatus(sessionToken)
      .then((response) => {
        log.debug("checkEmailStatus -> " + JSON.stringify(response));
        if (response && response.verified) {
          currentState.getUserAccountData()
            .then((data) => {
              data.verified = true;
              return currentState.setUserAccountData(data);
            })
            .then((data) => {
              // Now that the user is verified, we can proceed to fetch keys
              if (currentState.whenVerifiedDeferred) {
                currentState.whenVerifiedDeferred.resolve(data);
                delete currentState.whenVerifiedDeferred;
              }
              // Tell FxAccountsManager to clear its cache
              this.notifyObservers(ON_FXA_UPDATE_NOTIFICATION, ONVERIFIED_NOTIFICATION);
            });
        } else {
          // Poll email status again after a short delay.
          this.pollEmailStatusAgain(currentState, sessionToken);
        }
      }, error => {
        let timeoutMs = undefined;
        if (error && error.retryAfter) {
          // If the server told us to back off, back off the requested amount.
          timeoutMs = (error.retryAfter + 3) * 1000;
        }
        // The server will return 401 if a request parameter is erroneous or
        // if the session token expired. Let's continue polling otherwise.
        if (!error || !error.code || error.code != 401) {
          this.pollEmailStatusAgain(currentState, sessionToken, timeoutMs);
        }
      });
  },

  // Poll email status using truncated exponential back-off.
  pollEmailStatusAgain: function (currentState, sessionToken, timeoutMs) {
    let ageMs = Date.now() - this.pollStartDate;
    if (ageMs >= this.POLL_SESSION) {
      if (currentState.whenVerifiedDeferred) {
        let error = new Error("User email verification timed out.")
        currentState.whenVerifiedDeferred.reject(error);
        delete currentState.whenVerifiedDeferred;
      }
      log.debug("polling session exceeded, giving up");
      return;
    }
    if (timeoutMs === undefined) {
      let currentMinute = Math.ceil(ageMs / 60000);
      timeoutMs = currentMinute <= 2 ? this.VERIFICATION_POLL_TIMEOUT_INITIAL
                                     : this.VERIFICATION_POLL_TIMEOUT_SUBSEQUENT;
    }
    log.debug("polling with timeout = " + timeoutMs);
    this.currentTimer = setTimeout(() => {
      this.pollEmailStatus(currentState, sessionToken, "timer");
    }, timeoutMs);
  },

  _requireHttps: function() {
    let allowHttp = false;
    try {
      allowHttp = Services.prefs.getBoolPref("identity.fxaccounts.allowHttp");
    } catch(e) {
      // Pref doesn't exist
    }
    return allowHttp !== true;
  },

  // Return the URI of the remote UI flows.
  getAccountsSignUpURI: function() {
    let url = Services.urlFormatter.formatURLPref("identity.fxaccounts.remote.signup.uri");
    if (this._requireHttps() && !/^https:/.test(url)) { // Comment to un-break emacs js-mode highlighting
      throw new Error("Firefox Accounts server must use HTTPS");
    }
    return url;
  },

  // Return the URI of the remote UI flows.
  getAccountsSignInURI: function() {
    let url = Services.urlFormatter.formatURLPref("identity.fxaccounts.remote.signin.uri");
    if (this._requireHttps() && !/^https:/.test(url)) { // Comment to un-break emacs js-mode highlighting
      throw new Error("Firefox Accounts server must use HTTPS");
    }
    return url;
  },

  // Returns a promise that resolves with the URL to use to force a re-signin
  // of the current account.
  promiseAccountsForceSigninURI: function() {
    let url = Services.urlFormatter.formatURLPref("identity.fxaccounts.remote.force_auth.uri");
    if (this._requireHttps() && !/^https:/.test(url)) { // Comment to un-break emacs js-mode highlighting
      throw new Error("Firefox Accounts server must use HTTPS");
    }
    let currentState = this.currentAccountState;
    // but we need to append the email address onto a query string.
    return this.getSignedInUser().then(accountData => {
      if (!accountData) {
        return null;
      }
      let newQueryPortion = url.indexOf("?") == -1 ? "?" : "&";
      newQueryPortion += "email=" + encodeURIComponent(accountData.email);
      return url + newQueryPortion;
    }).then(result => currentState.resolve(result));
  },

  // Returns a promise that resolves with the URL to use to change
  // the current account's profile image.
  // if settingToEdit is set, the profile page should hightlight that setting
  // for the user to edit.
  promiseAccountsChangeProfileURI: function(settingToEdit = null) {
    let url = Services.urlFormatter.formatURLPref("identity.fxaccounts.settings.uri");

    if (settingToEdit) {
      url += (url.indexOf("?") == -1 ? "?" : "&") +
             "setting=" + encodeURIComponent(settingToEdit);
    }

    if (this._requireHttps() && !/^https:/.test(url)) { // Comment to un-break emacs js-mode highlighting
      throw new Error("Firefox Accounts server must use HTTPS");
    }
    let currentState = this.currentAccountState;
    // but we need to append the email address onto a query string.
    return this.getSignedInUser().then(accountData => {
      if (!accountData) {
        return null;
      }
      let newQueryPortion = url.indexOf("?") == -1 ? "?" : "&";
      newQueryPortion += "email=" + encodeURIComponent(accountData.email);
      newQueryPortion += "&uid=" + encodeURIComponent(accountData.uid);
      return url + newQueryPortion;
    }).then(result => currentState.resolve(result));
  },

  /**
   * Get an OAuth token for the user
   *
   * @param options
   *        {
   *          scope: (string/array) the oauth scope(s) being requested. As a
   *                 convenience, you may pass a string if only one scope is
   *                 required, or an array of strings if multiple are needed.
   *        }
   *
   * @return Promise.<string | Error>
   *        The promise resolves the oauth token as a string or rejects with
   *        an error object ({error: ERROR, details: {}}) of the following:
   *          INVALID_PARAMETER
   *          NO_ACCOUNT
   *          UNVERIFIED_ACCOUNT
   *          NETWORK_ERROR
   *          AUTH_ERROR
   *          UNKNOWN_ERROR
   */
  getOAuthToken: Task.async(function* (options = {}) {
    log.debug("getOAuthToken enter");
    let scope = options.scope;
    if (typeof scope === "string") {
      scope = [scope];
    }

    if (!scope || !scope.length) {
      throw this._error(ERROR_INVALID_PARAMETER, "Missing or invalid 'scope' option");
    }

    yield this._getVerifiedAccountOrReject();

    // Early exit for a cached token.
    let currentState = this.currentAccountState;
    let cached = currentState.getCachedToken(scope);
    if (cached) {
      log.debug("getOAuthToken returning a cached token");
      return cached.token;
    }

    // We are going to hit the server - this is the string we pass to it.
    let scopeString = scope.join(" ");
    let client = options.client;

    if (!client) {
      try {
        let defaultURL = Services.urlFormatter.formatURLPref("identity.fxaccounts.remote.oauth.uri");
        client = new FxAccountsOAuthGrantClient({
          serverURL: defaultURL,
          client_id: FX_OAUTH_CLIENT_ID
        });
      } catch (e) {
        throw this._error(ERROR_INVALID_PARAMETER, e);
      }
    }
    let oAuthURL = client.serverURL.href;

    try {
      log.debug("getOAuthToken fetching new token from", oAuthURL);
      let assertion = yield this.getAssertion(oAuthURL);
      let result = yield client.getTokenFromAssertion(assertion, scopeString);
      let token = result.access_token;
      // If we got one, cache it.
      if (token) {
        let entry = {token: token, server: oAuthURL};
        // But before we do, check the cache again - if we find one now, it
        // means someone else concurrently requested the same scope and beat
        // us to the cache write. To be nice to the server, we revoke the one
        // we just got and return the newly cached value.
        let cached = currentState.getCachedToken(scope);
        if (cached) {
          log.debug("Detected a race for this token - revoking the new one.");
          this._destroyOAuthToken(entry);
          return cached.token;
        }
        currentState.setCachedToken(scope, entry);
      }
      return token;
    } catch (err) {
      throw this._errorToErrorClass(err);
    }
  }),

  /**
   * Remove an OAuth token from the token cache.  Callers should call this
   * after they determine a token is invalid, so a new token will be fetched
   * on the next call to getOAuthToken().
   *
   * @param options
   *        {
   *          token: (string) A previously fetched token.
   *        }
   * @return Promise.<undefined> This function will always resolve, even if
   *         an unknown token is passed.
   */
   removeCachedOAuthToken: Task.async(function* (options) {
    if (!options.token || typeof options.token !== "string") {
      throw this._error(ERROR_INVALID_PARAMETER, "Missing or invalid 'token' option");
    }
    let currentState = this.currentAccountState;
    let existing = currentState.removeCachedToken(options.token);
    if (existing) {
      // background destroy.
      this._destroyOAuthToken(existing).catch(err => {
        log.warn("FxA failed to revoke a cached token", err);
      });
    }
   }),

  _getVerifiedAccountOrReject: Task.async(function* () {
    let data = yield this.currentAccountState.getUserAccountData();
    if (!data) {
      // No signed-in user
      throw this._error(ERROR_NO_ACCOUNT);
    }
    if (!this.isUserEmailVerified(data)) {
      // Signed-in user has not verified email
      throw this._error(ERROR_UNVERIFIED_ACCOUNT);
    }
  }),

  /*
   * Coerce an error into one of the general error cases:
   *          NETWORK_ERROR
   *          AUTH_ERROR
   *          UNKNOWN_ERROR
   *
   * These errors will pass through:
   *          INVALID_PARAMETER
   *          NO_ACCOUNT
   *          UNVERIFIED_ACCOUNT
   */
  _errorToErrorClass: function (aError) {
    if (aError.errno) {
      let error = SERVER_ERRNO_TO_ERROR[aError.errno];
      return this._error(ERROR_TO_GENERAL_ERROR_CLASS[error] || ERROR_UNKNOWN, aError);
    } else if (aError.message &&
        (aError.message === "INVALID_PARAMETER" ||
        aError.message === "NO_ACCOUNT" ||
        aError.message === "UNVERIFIED_ACCOUNT")) {
      return aError;
    }
    return this._error(ERROR_UNKNOWN, aError);
  },

  _error: function(aError, aDetails) {
    log.error("FxA rejecting with error ${aError}, details: ${aDetails}", {aError, aDetails});
    let reason = new Error(aError);
    if (aDetails) {
      reason.details = aDetails;
    }
    return reason;
  },

  /**
   * Get the user's account and profile data
   *
   * @param options
   *        {
   *          contentUrl: (string) Used by the FxAccountsProfileChannel.
   *            Defaults to pref identity.fxaccounts.settings.uri
   *          profileServerUrl: (string) Used by the FxAccountsProfileChannel.
   *            Defaults to pref identity.fxaccounts.remote.profile.uri
   *        }
   *
   * @return Promise.<object | Error>
   *        The promise resolves to an accountData object with extra profile
   *        information such as profileImageUrl, or rejects with
   *        an error object ({error: ERROR, details: {}}) of the following:
   *          INVALID_PARAMETER
   *          NO_ACCOUNT
   *          UNVERIFIED_ACCOUNT
   *          NETWORK_ERROR
   *          AUTH_ERROR
   *          UNKNOWN_ERROR
   */
  getSignedInUserProfile: function () {
    let accountState = this.currentAccountState;
    return accountState.getProfile()
      .then((profileData) => {
        let profile = JSON.parse(JSON.stringify(profileData));
        return accountState.resolve(profile);
      },
      (error) => {
        log.error("Could not retrieve profile data", error);
        return accountState.reject(error);
      })
      .then(null, err => Promise.reject(this._errorToErrorClass(err)));
  },
};

/**
 * JSONStorage constructor that creates instances that may set/get
 * to a specified file, in a directory that will be created if it
 * doesn't exist.
 *
 * @param options {
 *                  filename: of the file to write to
 *                  baseDir: directory where the file resides
 *                }
 * @return instance
 */
function JSONStorage(options) {
  this.baseDir = options.baseDir;
  this.path = OS.Path.join(options.baseDir, options.filename);
  this.oauthTokensPath = OS.Path.join(options.baseDir, options.oauthTokensFilename);
};

JSONStorage.prototype = {
  set: function(contents) {
    return OS.File.makeDir(this.baseDir, {ignoreExisting: true})
      .then(CommonUtils.writeJSON.bind(null, contents, this.path));
  },

  get: function() {
    return CommonUtils.readJSON(this.path);
  },

  setOAuthTokens: function(contents) {
    return OS.File.makeDir(this.baseDir, {ignoreExisting: true})
      .then(CommonUtils.writeJSON.bind(null, contents, this.oauthTokensPath));
  },

  getOAuthTokens: function(contents) {
    return CommonUtils.readJSON(this.oauthTokensPath);
  },

};

/**
 * LoginManagerStorage constructor that creates instances that may set/get
 * from a combination of a clear-text JSON file and stored securely in
 * the nsILoginManager.
 *
 * @param options {
 *                  filename: of the plain-text file to write to
 *                  baseDir: directory where the file resides
 *                }
 * @return instance
 */

function LoginManagerStorage(options) {
  // we reuse the JSONStorage for writing the plain-text stuff.
  this.jsonStorage = new JSONStorage(options);
}

LoginManagerStorage.prototype = {
  // The fields in the credentials JSON object that are stored in plain-text
  // in the profile directory.  All other fields are stored in the login manager,
  // and thus are only available when the master-password is unlocked.

  // a hook point for testing.
  get _isLoggedIn() {
    return Services.logins.isLoggedIn;
  },

  // Clear any data from the login manager.  Returns true if the login manager
  // was unlocked (even if no existing logins existed) or false if it was
  // locked (meaning we don't even know if it existed or not.)
  _clearLoginMgrData: Task.async(function* () {
    try { // Services.logins might be third-party and broken...
      yield Services.logins.initializationPromise;
      if (!this._isLoggedIn) {
        return false;
      }
      let logins = Services.logins.findLogins({}, FXA_PWDMGR_HOST, null, FXA_PWDMGR_REALM);
      for (let login of logins) {
        Services.logins.removeLogin(login);
      }
      return true;
    } catch (ex) {
      log.error("Failed to clear login data: ${}", ex);
      return false;
    }
  }),

  set: Task.async(function* (contents) {
    if (!contents) {
      // User is signing out - write the null to the json file.
      yield this.jsonStorage.set(contents);

      // And nuke it from the login manager.
      let cleared = yield this._clearLoginMgrData();
      if (!cleared) {
        // just log a message - we verify that the email address matches when
        // we reload it, so having a stale entry doesn't really hurt.
        log.info("not removing credentials from login manager - not logged in");
      }
      return;
    }

    // We are saving actual data.
    // Split the data into 2 chunks - one to go to the plain-text, and the
    // other to write to the login manager.
    let toWriteJSON = {version: contents.version};
    let accountDataJSON = toWriteJSON.accountData = {};
    let toWriteLoginMgr = {version: contents.version};
    let accountDataLoginMgr = toWriteLoginMgr.accountData = {};
    for (let [name, value] of Iterator(contents.accountData)) {
      if (FXA_PWDMGR_PLAINTEXT_FIELDS.indexOf(name) >= 0) {
        accountDataJSON[name] = value;
      } else {
        accountDataLoginMgr[name] = value;
      }
    }
    yield this.jsonStorage.set(toWriteJSON);

    try { // Services.logins might be third-party and broken...
      // and the stuff into the login manager.
      yield Services.logins.initializationPromise;
      // If MP is locked we silently fail - the user may need to re-auth
      // next startup.
      if (!this._isLoggedIn) {
        log.info("not saving credentials to login manager - not logged in");
        return;
      }
      // write the rest of the data to the login manager.
      let loginInfo = new Components.Constructor(
         "@mozilla.org/login-manager/loginInfo;1", Ci.nsILoginInfo, "init");
      let login = new loginInfo(FXA_PWDMGR_HOST,
                                null, // aFormSubmitURL,
                                FXA_PWDMGR_REALM, // aHttpRealm,
                                contents.accountData.email, // aUsername
                                JSON.stringify(toWriteLoginMgr), // aPassword
                                "", // aUsernameField
                                "");// aPasswordField

      let existingLogins = Services.logins.findLogins({}, FXA_PWDMGR_HOST, null,
                                                      FXA_PWDMGR_REALM);
      if (existingLogins.length) {
        Services.logins.modifyLogin(existingLogins[0], login);
      } else {
        Services.logins.addLogin(login);
      }
    } catch (ex) {
      log.error("Failed to save data to the login manager: ${}", ex);
    }
  }),

  get: Task.async(function* () {
    // we need to suck some data from the .json file in the profile dir and
    // some other from the login manager.
    let data = yield this.jsonStorage.get();
    if (!data) {
      // no user logged in, nuke the storage data incase we couldn't remove
      // it previously and then we are done.
      yield this._clearLoginMgrData();
      return null;
    }

    // if we have encryption keys it must have been saved before we
    // used the login manager, so re-save it.
    if (data.accountData.kA || data.accountData.kB || data.keyFetchToken) {
      // We need to migrate, but the MP might be locked (eg, on the first run
      // with this enabled, we will get here very soon after startup, so will
      // certainly be locked.)  This means we can't actually store the data in
      // the login manager (and thus might lose it if we migrated now)
      // So if the MP is locked, we *don't* migrate, but still just return
      // the subset of data we now store in the JSON.
      // This will cause sync to notice the lack of keys, force an unlock then
      // re-fetch the account data to see if the keys are there.  At *that*
      // point we will end up back here, but because the MP is now unlocked
      // we can actually perform the migration.
      if (!this._isLoggedIn) {
        // return the "safe" subset but leave the storage alone.
        log.info("account data needs migration to the login manager but the MP is locked.");
        let result = {
          version: data.version,
          accountData: {},
        };
        for (let fieldName of FXA_PWDMGR_PLAINTEXT_FIELDS) {
          result.accountData[fieldName] = data.accountData[fieldName];
        }
        return result;
      }
      // actually migrate - just calling .set() will split everything up.
      log.info("account data is being migrated to the login manager.");
      yield this.set(data);
    }

    try { // Services.logins might be third-party and broken...
      // read the data from the login manager and merge it for return.
      yield Services.logins.initializationPromise;

      if (!this._isLoggedIn) {
        log.info("returning partial account data as the login manager is locked.");
        return data;
      }

      let logins = Services.logins.findLogins({}, FXA_PWDMGR_HOST, null, FXA_PWDMGR_REALM);
      if (logins.length == 0) {
        // This could happen if the MP was locked when we wrote the data.
        log.info("Can't find the rest of the credentials in the login manager");
        return data;
      }
      let login = logins[0];
      if (login.username == data.accountData.email) {
        let lmData = JSON.parse(login.password);
        if (lmData.version == data.version) {
          // Merge the login manager data
          copyObjectProperties(lmData.accountData, data.accountData);
        } else {
          log.info("version field in the login manager doesn't match - ignoring it");
          yield this._clearLoginMgrData();
        }
      } else {
        log.info("username in the login manager doesn't match - ignoring it");
        yield this._clearLoginMgrData();
      }
    } catch (ex) {
      log.error("Failed to get data from the login manager: ${}", ex);
    }
    return data;
  }),

  // OAuth tokens are always written to disk, so delegate to our JSON storage.
  // (Bug 1013064 comments 23-25 explain why we save the sessionToken into the
  // plain JSON file, and the same logic applies for oauthTokens being in JSON)
  getOAuthTokens() {
    return this.jsonStorage.getOAuthTokens();
  },

  setOAuthTokens(contents) {
    return this.jsonStorage.setOAuthTokens(contents);
  },
}

// A getter for the instance to export
XPCOMUtils.defineLazyGetter(this, "fxAccounts", function() {
  let a = new FxAccounts();

  // XXX Bug 947061 - We need a strategy for resuming email verification after
  // browser restart
  a.loadAndPoll();

  return a;
});
