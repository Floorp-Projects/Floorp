/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {PromiseUtils} = ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm");
const {CommonUtils} = ChromeUtils.import("resource://services-common/utils.js");
const {CryptoUtils} = ChromeUtils.import("resource://services-crypto/utils.js");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {clearTimeout, setTimeout} = ChromeUtils.import("resource://gre/modules/Timer.jsm");
const {FxAccountsStorageManager} = ChromeUtils.import("resource://gre/modules/FxAccountsStorage.jsm");
const {ASSERTION_LIFETIME, ASSERTION_USE_PERIOD, CERT_LIFETIME, COMMAND_SENDTAB, DERIVED_KEYS_NAMES, ERRNO_DEVICE_SESSION_CONFLICT, ERRNO_INVALID_AUTH_TOKEN, ERRNO_UNKNOWN_DEVICE, ERROR_AUTH_ERROR, ERROR_INVALID_PARAMETER, ERROR_NO_ACCOUNT, ERROR_OFFLINE, ERROR_TO_GENERAL_ERROR_CLASS, ERROR_UNKNOWN, ERROR_UNVERIFIED_ACCOUNT, FXA_PWDMGR_MEMORY_FIELDS, FXA_PWDMGR_PLAINTEXT_FIELDS, FXA_PWDMGR_REAUTH_WHITELIST, FXA_PWDMGR_SECURE_FIELDS, FX_OAUTH_CLIENT_ID, KEY_LIFETIME, ONLOGIN_NOTIFICATION, ONLOGOUT_NOTIFICATION, ONVERIFIED_NOTIFICATION, ON_DEVICE_DISCONNECTED_NOTIFICATION, ON_NEW_DEVICE_ID, POLL_SESSION, PREF_LAST_FXA_USER, SERVER_ERRNO_TO_ERROR, SCOPE_OLD_SYNC, log, logPII} = ChromeUtils.import("resource://gre/modules/FxAccountsCommon.js");

ChromeUtils.defineModuleGetter(this, "FxAccountsClient",
  "resource://gre/modules/FxAccountsClient.jsm");

ChromeUtils.defineModuleGetter(this, "FxAccountsConfig",
  "resource://gre/modules/FxAccountsConfig.jsm");

ChromeUtils.defineModuleGetter(this, "jwcrypto",
  "resource://services-crypto/jwcrypto.jsm");

ChromeUtils.defineModuleGetter(this, "FxAccountsOAuthGrantClient",
  "resource://gre/modules/FxAccountsOAuthGrantClient.jsm");

ChromeUtils.defineModuleGetter(this, "FxAccountsCommands",
  "resource://gre/modules/FxAccountsCommands.js");

ChromeUtils.defineModuleGetter(this, "FxAccountsProfile",
  "resource://gre/modules/FxAccountsProfile.jsm");

ChromeUtils.defineModuleGetter(this, "Utils",
  "resource://services-sync/util.js");

XPCOMUtils.defineLazyPreferenceGetter(this, "FXA_ENABLED",
    "identity.fxaccounts.enabled", true);

// All properties exposed by the public FxAccounts API.
var publicProperties = [
  "_withCurrentAccountState", // fxaccounts package only!
  "accountStatus",
  "canGetKeys",
  "checkVerificationStatus",
  "commands",
  "getAccountsClient",
  "getAssertion",
  "getDeviceId",
  "getDeviceList",
  "getKeys",
  "authorizeOAuthCode",
  "getOAuthToken",
  "getProfileCache",
  "getPushSubscription",
  "getScopedKeys",
  "getSignedInUser",
  "getSignedInUserProfile",
  "handleAccountDestroyed",
  "handleDeviceDisconnection",
  "handleEmailUpdated",
  "hasLocalSession",
  "invalidateCertificate",
  "loadAndPoll",
  "localtimeOffsetMsec",
  "notifyDevices",
  "now",
  "removeCachedOAuthToken",
  "resendVerificationEmail",
  "resetCredentials",
  "sessionStatus",
  "setProfileCache",
  "setSignedInUser",
  "signOut",
  "updateDeviceRegistration",
  "updateUserAccountData",
  "whenVerified",
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
var AccountState = this.AccountState = function(storageManager) {
  this.storageManager = storageManager;
  this.promiseInitialized = this.storageManager.getAccountData().then(data => {
    this.oauthTokens = data && data.oauthTokens ? data.oauthTokens : {};
  }).catch(err => {
    log.error("Failed to initialize the storage manager", err);
    // Things are going to fall apart, but not much we can do about it here.
  });
};

AccountState.prototype = {
  oauthTokens: null,
  whenVerifiedDeferred: null,
  whenKeysReadyDeferred: null,

  // If the storage manager has been nuked then we are no longer current.
  get isCurrent() {
    return this.storageManager != null;
  },

  abort() {
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
    return this.signOut();
  },

  // Clobber all cached data and write that empty data to storage.
  async signOut() {
    this.cert = null;
    this.keyPair = null;
    this.oauthTokens = null;

    // Avoid finalizing the storageManager multiple times (ie, .signOut()
    // followed by .abort())
    if (!this.storageManager) {
      return;
    }
    const storageManager = this.storageManager;
    this.storageManager = null;

    await storageManager.deleteAccountData();
    await storageManager.finalize();
  },

  // Get user account data. Optionally specify explicit field names to fetch
  // (and note that if you require an in-memory field you *must* specify the
  // field name(s).)
  getUserAccountData(fieldNames = null) {
    if (!this.isCurrent) {
      return Promise.reject(new Error("Another user has signed in"));
    }
    return this.storageManager.getAccountData(fieldNames).then(result => {
      return this.resolve(result);
    });
  },

  updateUserAccountData(updatedFields) {
    if (!this.isCurrent) {
      return Promise.reject(new Error("Another user has signed in"));
    }
    return this.storageManager.updateAccountData(updatedFields);
  },

  resolve(result) {
    if (!this.isCurrent) {
      log.info("An accountState promise was resolved, but was actually rejected" +
               " due to a different user being signed in. Originally resolved" +
               " with", result);
      return Promise.reject(new Error("A different user signed in"));
    }
    return Promise.resolve(result);
  },

  reject(error) {
    // It could be argued that we should just let it reject with the original
    // error - but this runs the risk of the error being (eg) a 401, which
    // might cause the consumer to attempt some remediation and cause other
    // problems.
    if (!this.isCurrent) {
      log.info("An accountState promise was rejected, but we are ignoring that" +
               "reason and rejecting it due to a different user being signed in." +
               "Originally rejected with", error);
      return Promise.reject(new Error("A different user signed in"));
    }
    return Promise.reject(error);
  },

  // Abstractions for storage of cached tokens - these are all sync, and don't
  // handle revocation etc - it's just storage (and the storage itself is async,
  // but we don't return the storage promises, so it *looks* sync)
  // These functions are sync simply so we can handle "token races" - when there
  // are multiple in-flight requests for the same scope, we can detect this
  // and revoke the redundant token.

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
    let result = this.oauthTokens[key];
    if (result) {
      // later we might want to check an expiry date - but we currently
      // have no such concept, so just return it.
      log.trace("getCachedToken returning cached token");
      return result;
    }
    return null;
  },

  // Remove a cached token from the cache.  Does *not* revoke it from anywhere.
  // Returns the entire token entry if found, null otherwise.
  removeCachedToken(token) {
    this._cachePreamble();
    let data = this.oauthTokens;
    for (let [key, tokenValue] of Object.entries(data)) {
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
    return this.updateUserAccountData({ oauthTokens: this.oauthTokens }).catch(err => {
      log.error("Failed to update cached tokens", err);
    });
  },
};

/* Given an array of scopes, make a string key by normalizing. */
function getScopeKey(scopeArray) {
  let normalizedScopes = scopeArray.map(item => item.toLowerCase());
  return normalizedScopes.sort().join("|");
}

function getPropertyDescriptor(obj, prop) {
  return Object.getOwnPropertyDescriptor(obj, prop) ||
         getPropertyDescriptor(Object.getPrototypeOf(obj), prop);
}

/**
 * Copies properties from a given object to another object.
 *
 * @param from (object)
 *        The object we read property descriptors from.
 * @param to (object)
 *        The object that we set property descriptors on.
 * @param thisObj (object)
 *        The object that will be used to .bind() all function properties we find to.
 * @param keys ([...])
 *        The names of all properties to be copied.
 */
function copyObjectProperties(from, to, thisObj, keys) {
  for (let prop of keys) {
    // Look for the prop in the prototype chain.
    let desc = getPropertyDescriptor(from, prop);

    if (typeof(desc.value) == "function") {
      desc.value = desc.value.bind(thisObj);
    }

    if (desc.get) {
      desc.get = desc.get.bind(thisObj);
    }

    if (desc.set) {
      desc.set = desc.set.bind(thisObj);
    }

    Object.defineProperty(to, prop, desc);
  }
}

function urlsafeBase64Encode(key) {
  return ChromeUtils.base64URLEncode(new Uint8Array(key), { pad: false });
}

/**
 * The public API's constructor.
 */
var FxAccounts = function(mockInternal) {
  let external = {};
  let internal;

  if (!mockInternal) {
    internal = new FxAccountsInternal();
    copyObjectProperties(FxAccountsInternal.prototype, external, internal, publicProperties);
  } else {
    internal = Object.create(FxAccountsInternal.prototype, Object.getOwnPropertyDescriptors(mockInternal));
    copyObjectProperties(internal, external, internal, publicProperties);
    // Exposes the internal object for testing only.
    external.internal = internal;
  }

  if (!internal.fxaPushService) {
    // internal.fxaPushService option is used in testing.
    // Otherwise we load the service lazily.
    XPCOMUtils.defineLazyGetter(internal, "fxaPushService", function() {
      return Cc["@mozilla.org/fxaccounts/push;1"]
        .getService(Ci.nsISupports)
        .wrappedJSObject;
    });
  }

  if (!internal.observerPreloads) {
    // A registry of promise-returning functions that `notifyObservers` should
    // call before sending notifications. Primarily used so parts of Firefox
    // which have yet to load for performance reasons can be force-loaded, and
    // thus not miss notifications.
    internal.observerPreloads = [
      // Sync
      () => {
        let scope = {};
        ChromeUtils.import("resource://services-sync/main.js", scope);
        return scope.Weave.Service.promiseInitialized;
      },
    ];
  }

  // wait until after the mocks are setup before initializing.
  internal.initialize();

  return Object.freeze(external);
};
this.FxAccounts.config = FxAccountsConfig;

/**
 * The internal API's constructor.
 */
function FxAccountsInternal() {
  // Make a local copy of this constant so we can mock it in testing
  this.POLL_SESSION = POLL_SESSION;

  // All significant initialization should be done in the initialize() method
  // below as it helps with testing.
}

/**
 * The internal API's prototype.
 */
FxAccountsInternal.prototype = {
  // The timeout (in ms) we use to poll for a verified mail for the first
  // VERIFICATION_POLL_START_SLOWDOWN_THRESHOLD minutes if the user has
  // logged-in in this session.
  VERIFICATION_POLL_TIMEOUT_INITIAL: 60000, // 1 minute.
  // All the other cases (> 5 min, on restart etc).
  VERIFICATION_POLL_TIMEOUT_SUBSEQUENT: 5 * 60000, // 5 minutes.
  // After X minutes, the polling will slow down to _SUBSEQUENT if we have
  // logged-in in this session.
  VERIFICATION_POLL_START_SLOWDOWN_THRESHOLD: 5,
  // The current version of the device registration, we use this to re-register
  // devices after we update what we send on device registration.
  DEVICE_REGISTRATION_VERSION: 2,

  _fxAccountsClient: null,

  // All significant initialization should be done in this initialize() method,
  // as it's called after this object has been mocked for tests.
  initialize() {
    this.currentTimer = null;
    this.currentAccountState = this.newAccountState();
  },

  get fxAccountsClient() {
    if (!this._fxAccountsClient) {
      this._fxAccountsClient = new FxAccountsClient();
    }
    return this._fxAccountsClient;
  },

  // The profile object used to fetch the actual user profile.
  _profile: null,
  get profile() {
    if (!this._profile) {
      let profileServerUrl = Services.urlFormatter.formatURLPref("identity.fxaccounts.remote.profile.uri");
      this._profile = new FxAccountsProfile({
        fxa: this,
        profileServerUrl,
      });
    }
    return this._profile;
  },

  _commands: null,
  get commands() {
    if (!this._commands) {
      this._commands = new FxAccountsCommands(this);
    }
    return this._commands;
  },

  _oauthClient: null,
  get oauthClient() {
    if (!this._oauthClient) {
      const serverURL = Services.urlFormatter.formatURLPref("identity.fxaccounts.remote.oauth.uri");
      this._oauthClient = new FxAccountsOAuthGrantClient({
        serverURL,
        client_id: FX_OAUTH_CLIENT_ID,
      });
    }
    return this._oauthClient;
  },

  // A hook-point for tests who may want a mocked AccountState or mocked storage.
  newAccountState(credentials) {
    let storage = new FxAccountsStorageManager();
    storage.initialize(credentials);
    return new AccountState(storage);
  },

  // "Friend" classes of FxAccounts (e.g. FxAccountsCommands) know about the
  // "current account state" system. This method allows them to read and write
  // safely in it.
  // Example of usage:
  // fxAccounts._withCurrentAccountState(async (getUserData, updateUserData) => {
  //   const userData = await getUserData(['device']);
  //   ...
  //   await updateUserData({device: null});
  // });
  _withCurrentAccountState(func) {
    const state = this.currentAccountState;
    const getUserData = (fields) => state.getUserAccountData(fields);
    const updateUserData = (data) => state.updateUserAccountData(data);
    return func(getUserData, updateUserData);
  },

  /**
   * Send a message to a set of devices in the same account
   *
   * @return Promise
   */
  notifyDevices(deviceIds, excludedIds, payload, TTL) {
    if (typeof deviceIds == "string") {
      deviceIds = [deviceIds];
    }
    return this.currentAccountState.getUserAccountData()
      .then(data => {
        if (!data) {
          throw this._error(ERROR_NO_ACCOUNT);
        }
        if (!data.sessionToken) {
          throw this._error(ERROR_AUTH_ERROR,
            "notifyDevices called without a session token");
        }
        return this.fxAccountsClient.notifyDevices(data.sessionToken, deviceIds,
          excludedIds, payload, TTL);
    });
  },

  /**
   * Return the current time in milliseconds as an integer.  Allows tests to
   * manipulate the date to simulate certificate expiration.
   */
  now() {
    return this.fxAccountsClient.now();
  },

  getAccountsClient() {
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
  checkEmailStatus: function checkEmailStatus(sessionToken, options = {}) {
    if (!sessionToken) {
      return Promise.reject(new Error(
        "checkEmailStatus called without a session token"));
    }
    return this.fxAccountsClient.recoveryEmailStatus(sessionToken,
      options).catch(error => this._handleTokenError(error));
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
   *          kSync: An encryption key for Sync
   *          kXCS: A key hash of kB for the X-Client-State header
   *          kExtSync: An encryption key for WebExtensions syncing
   *          kExtKbHash: A key hash of kB for WebExtensions syncing
   *          verified: email verification status
   *          authAt: The time (seconds since epoch) that this record was
   *                  authenticated
   *        }
   *        or null if no user is signed in.
   */
  async getSignedInUser() {
    let currentState = this.currentAccountState;
    const data = await currentState.getUserAccountData();
    if (!data) {
      return currentState.resolve(null);
    }
    if (!FXA_ENABLED) {
      await this.signOut();
      return currentState.resolve(null);
    }
    if (this.isUserEmailVerified(data)) {
      // This is a work-around for preferences being reset (bug 1550967).
      // Many things check this preference as a flag for "is sync configured",
      // and if not, we try and avoid loading these modules at all. So if a user
      // is signed in but this pref isn't set, things go weird.
      // However, some thing do unconditionally load fxaccounts, such as
      // about:prefs. When that happens we can detect the state and re-add the
      // pref. Note that we only do this for verified users as that's what sync
      // does (ie, if the user is unverified, sync will set it on verification)
      if (!Services.prefs.prefHasUserValue("services.sync.username") && data.email) {
        Services.prefs.setStringPref("services.sync.username", data.email);
      }
    } else {
      // If the email is not verified, start polling for verification,
      // but return null right away.  We don't want to return a promise
      // that might not be fulfilled for a long time.
      this.startVerifiedCheck(data);
    }
    return currentState.resolve(data);
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
  async setSignedInUser(credentials) {
    if (!FXA_ENABLED) {
      throw new Error("Cannot call setSignedInUser when FxA is disabled.");
    }
    log.debug("setSignedInUser - aborting any existing flows");
    const signedInUser = await this.getSignedInUser();
    if (signedInUser) {
      await this._signOutServer(signedInUser.sessionToken, signedInUser.oauthTokens);
    }
    await this.abortExistingFlow();
    let currentAccountState = this.currentAccountState = this.newAccountState(
      Cu.cloneInto(credentials, {}) // Pass a clone of the credentials object.
    );
    // This promise waits for storage, but not for verification.
    // We're telling the caller that this is durable now (although is that
    // really something we should commit to? Why not let the write happen in
    // the background? Already does for updateAccountData ;)
    await currentAccountState.promiseInitialized;
    // Starting point for polling if new user
    if (!this.isUserEmailVerified(credentials)) {
      this.startVerifiedCheck(credentials);
    }
    Services.telemetry.getHistogramById("FXA_CONFIGURED").add(1);
    await this.notifyObservers(ONLOGIN_NOTIFICATION);
    await this.updateDeviceRegistration();
    return currentAccountState.resolve();
  },

  /**
   * Update account data for the currently signed in user.
   *
   * @param credentials
   *        The credentials object containing the fields to be updated.
   *        This object must contain the |uid| field and it must
   *        match the currently signed in user.
   */
  updateUserAccountData(credentials) {
    log.debug("updateUserAccountData called with fields", Object.keys(credentials));
    if (logPII) {
      log.debug("updateUserAccountData called with data", credentials);
    }
    let currentAccountState = this.currentAccountState;
    return currentAccountState.promiseInitialized.then(() => {
      return currentAccountState.getUserAccountData(["uid"]);
    }).then(existing => {
      if (existing.uid != credentials.uid) {
        throw new Error("The specified credentials aren't for the current user");
      }
      // We need to nuke uid as storage will complain if we try and
      // update it (even when the value is the same)
      credentials = Cu.cloneInto(credentials, {}); // clone it first
      delete credentials.uid;
      return currentAccountState.updateUserAccountData(credentials);
    });
  },

  /**
   * returns a promise that fires with the assertion.  If there is no verified
   * signed-in user, fires with null.
   */
  getAssertion: function getAssertion(audience) {
    return this._getAssertion(audience);
  },

  // getAssertion() is "public" so screws with our mock story. This
  // implementation method *can* be (and is) mocked by tests.
  _getAssertion: function _getAssertion(audience) {
    log.debug("enter getAssertion()");
    let currentState = this.currentAccountState;
    return currentState.getUserAccountData().then(data => {
      if (!data) {
        // No signed-in user
        return null;
      }
      if (!this.isUserEmailVerified(data)) {
        // Signed-in user has not verified email
        return null;
      }
      if (!data.sessionToken) {
        // can't get a signed certificate without a session token. This
        // can happen if we request an assertion after clearing an invalid
        // session token from storage.
        throw this._error(ERROR_AUTH_ERROR, "getAssertion called without a session token");
      }
      return this.getKeypairAndCertificate(currentState).then(
        ({keyPair, certificate}) => {
          return this.getAssertionFromCert(data, keyPair, certificate, audience);
        }
      );
    }).catch(err =>
      this._handleTokenError(err)
    ).then(result => currentState.resolve(result));
  },

  /**
   * Invalidate the FxA certificate, so that it will be refreshed from the server
   * the next time it is needed.
   */
  invalidateCertificate() {
    return this.currentAccountState.updateUserAccountData({ cert: null });
  },

  async getDeviceId() {
    let data = await this.currentAccountState.getUserAccountData();
    if (!data) {
      // Without a signed-in user, there can be no device id.
      return null;
    }
    // Try migrating first. Remove this in Firefox 65+.
    if (data.deviceId) {
      log.info("Migrating from deviceId to device.");
      await this.currentAccountState.updateUserAccountData({
        deviceId: null,
        deviceRegistrationVersion: null,
        device: {
          id: data.deviceId,
          registrationVersion: data.deviceRegistrationVersion,
        },
      });
      data = await this.currentAccountState.getUserAccountData();
    }
    const {device} = data;
    if ((await this.checkDeviceUpdateNeeded(device))) {
      return this._registerOrUpdateDevice(data);
    }
    // Return the device id that we already registered with the server.
    return device.id;
  },

  async checkDeviceUpdateNeeded(device) {
    // There is no device registered or the device registration is outdated.
    // Either way, we should register the device with FxA
    // before returning the id to the caller.
    const availableCommandsKeys = Object.keys((await this.availableCommands())).sort();
    return !device || !device.registrationVersion ||
           device.registrationVersion < this.DEVICE_REGISTRATION_VERSION ||
           !device.registeredCommandsKeys ||
           !CommonUtils.arrayEqual(device.registeredCommandsKeys, availableCommandsKeys);
  },

  async getDeviceList() {
    const accountData = await this._getVerifiedAccountOrReject();
    const devices = await this.fxAccountsClient.getDeviceList(accountData.sessionToken);

    // Check if our push registration is still good.
    const ourDevice = devices.find(device => device.isCurrentDevice);
    if (ourDevice.pushEndpointExpired) {
      await this.fxaPushService.unsubscribe();
      await this._registerOrUpdateDevice(accountData);
    }

    return devices;
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
        if (!data.sessionToken) {
          return Promise.reject(new Error(
            "resendVerificationEmail called without a session token"));
        }
        this.startPollEmailStatus(currentState, data.sessionToken, "start");
        return this.fxAccountsClient.resendVerificationEmail(
          data.sessionToken).catch(err => this._handleTokenError(err));
      }
      throw new Error("Cannot resend verification email; no signed-in user");
    });
  },

  /*
   * Reset state such that any previous flow is canceled.
   */
  abortExistingFlow() {
    if (this.currentTimer) {
      log.debug("Polling aborted; Another user signing in");
      clearTimeout(this.currentTimer);
      this.currentTimer = 0;
    }
    if (this._profile) {
      this._profile.tearDown();
      this._profile = null;
    }
    if (this._commands) {
      this._commands = null;
    }
    // We "abort" the accountState and assume our caller is about to throw it
    // away and replace it with a new one.
    return this.currentAccountState.abort();
  },

  accountStatus: function accountStatus() {
    return this.currentAccountState.getUserAccountData().then(data => {
      if (!data) {
        return false;
      }
      return this.fxAccountsClient.accountStatus(data.uid);
    });
  },

  checkVerificationStatus() {
    log.trace("checkVerificationStatus");
    let currentState = this.currentAccountState;
    return currentState.getUserAccountData().then(data => {
      if (!data) {
        log.trace("checkVerificationStatus - no user data");
        return null;
      }

      // Always check the verification status, even if the local state indicates
      // we're already verified. If the user changed their password, the check
      // will fail, and we'll enter the reauth state.
      log.trace("checkVerificationStatus - forcing verification status check");
      return this.startPollEmailStatus(currentState, data.sessionToken, "push");
    });
  },

  _destroyOAuthToken(tokenData) {
    let client = new FxAccountsOAuthGrantClient({
      serverURL: tokenData.server,
      client_id: FX_OAUTH_CLIENT_ID,
    });
    return client.destroyToken(tokenData.token);
  },

  _destroyAllOAuthTokens(tokenInfos) {
    if (!tokenInfos) {
      return Promise.resolve();
    }
    // let's just destroy them all in parallel...
    let promises = [];
    for (let tokenInfo of Object.values(tokenInfos)) {
      promises.push(this._destroyOAuthToken(tokenInfo));
    }
    return Promise.all(promises);
  },

  async signOut(localOnly) {
    let sessionToken;
    let tokensToRevoke;
    const data = await this.currentAccountState.getUserAccountData();
    // Save the sessionToken, tokens before resetting them in _signOutLocal().
    if (data) {
      sessionToken = data.sessionToken;
      tokensToRevoke = data.oauthTokens;
    }
    await this._signOutLocal();
    if (!localOnly) {
      // Do this in the background so *any* slow request won't
      // block the local sign out.
      Services.tm.dispatchToMainThread(async () => {
        await this._signOutServer(sessionToken, tokensToRevoke);
        FxAccountsConfig.resetConfigURLs();
        this.notifyObservers("testhelper-fxa-signout-complete");
      });
    } else {
      // We want to do this either way -- but if we're signing out remotely we
      // need to wait until we destroy the oauth tokens if we want that to succeed.
      FxAccountsConfig.resetConfigURLs();
    }
    return this.notifyObservers(ONLOGOUT_NOTIFICATION);
  },

  async _signOutLocal() {
    await this.currentAccountState.signOut();
    // this "aborts" this.currentAccountState but doesn't make a new one.
    await this.abortExistingFlow();
    this.currentAccountState = this.newAccountState();
    return this.currentAccountState.promiseInitialized;
  },

  async _signOutServer(sessionToken, tokensToRevoke) {
    log.debug("Unsubscribing from FxA push.");
    try {
      await this.fxaPushService.unsubscribe();
    } catch (err) {
      log.error("Could not unsubscribe from push.", err);
    }
    if (sessionToken) {
      log.debug("Destroying session and device.");
      try {
        await this.fxAccountsClient.signOut(sessionToken, {service: "sync"});
      } catch (err) {
        log.error("Error during remote sign out of Firefox Accounts", err);
      }
    } else {
      log.warn("Missing session token; skipping remote sign out");
    }
    log.debug("Destroying all OAuth tokens.");
    try {
      await this._destroyAllOAuthTokens(tokensToRevoke);
    } catch (err) {
      log.error("Error during destruction of oauth tokens during signout", err);
    }
  },

  /**
   * Check the status of the current session using cached credentials.
   *
   * @return Promise
   *        Resolves with a boolean indicating if the session is still valid
   */
  sessionStatus() {
    return this.getSignedInUser().then(data => {
      if (!data.sessionToken) {
        return Promise.reject(new Error(
          "sessionStatus called without a session token"));
      }
      return this.fxAccountsClient.sessionStatus(data.sessionToken);
    });
  },

  /**
   * Checks if we have a valid local session state for the current account.
   *
   * @return Promise
   *        Resolves with a boolean, with true indicating that we appear to
   *        have a valid local session, or false if we need to reauthenticate
   *        with the content server to obtain one.
   *        Note that this doesn't check with the server - it really just tells
   *        us if we are even able to perform that server check. To fully check
   *        the account status, you should first call this method, and if this
   *        returns true, you should then call sessionStatus() to check with
   *        the server.
   */
  async hasLocalSession() {
    let data = await this.getSignedInUser();
    return data && data.sessionToken;
  },

  /**
   * Checks if we currently have encryption keys or if we have enough to
   * be able to successfully fetch them for the signed-in-user.
   */
  async canGetKeys() {
    let currentState = this.currentAccountState;
    let userData = await currentState.getUserAccountData();
    if (!userData) {
      throw new Error("Can't possibly get keys; User is not signed in");
    }
    // - keyFetchToken means we can almost certainly grab them.
    // - kSync, kXCS, kExtSync and kExtKbHash means we already have them.
    // - kB is deprecated but |getKeys| will help us migrate to kSync and friends.
    return userData && (userData.keyFetchToken ||
                        DERIVED_KEYS_NAMES.every(k => userData[k]) ||
                        userData.kB);
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
   *          kSync: An encryption key for Sync
   *          kXCS: A key hash of kB for the X-Client-State header
   *          kExtSync: An encryption key for WebExtensions syncing
   *          kExtKbHash: A key hash of kB for WebExtensions syncing
   *          verified: email verification status
   *        }
   *        or null if no user is signed in
   */
  async getKeys() {
    let currentState = this.currentAccountState;
    try {
      let userData = await currentState.getUserAccountData();
      if (!userData) {
        throw new Error("Can't get keys; User is not signed in");
      }
      if (userData.kB) { // Bug 1426306 - Migrate from kB to derived keys.
        log.info("Migrating kB to derived keys.");
        const {uid, kB} = userData;
        await this.updateUserAccountData({
          uid,
          ...(await this._deriveKeys(uid, CommonUtils.hexToBytes(kB))),
          kA: null, // Remove kA and kB from storage.
          kB: null,
        });
        userData = await this.getUserAccountData();
      }
      if (DERIVED_KEYS_NAMES.every(k => userData[k])) {
        return currentState.resolve(userData);
      }
      if (!currentState.whenKeysReadyDeferred) {
        currentState.whenKeysReadyDeferred = PromiseUtils.defer();
        if (userData.keyFetchToken) {
          this.fetchAndUnwrapKeys(userData.keyFetchToken).then(
            (dataWithKeys) => {
              if (DERIVED_KEYS_NAMES.some(k => !dataWithKeys[k])) {
                const missing = DERIVED_KEYS_NAMES.filter(k => !dataWithKeys[k]);
                currentState.whenKeysReadyDeferred.reject(
                  new Error(`user data missing: ${missing.join(", ")}`)
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
          currentState.whenKeysReadyDeferred.reject("No keyFetchToken");
        }
      }
      return await currentState.resolve(currentState.whenKeysReadyDeferred.promise);
    } catch (err) {
      return currentState.resolve(this._handleTokenError(err));
    }
  },

  async fetchAndUnwrapKeys(keyFetchToken) {
    if (logPII) {
      log.debug("fetchAndUnwrapKeys: token: " + keyFetchToken);
    }
    let currentState = this.currentAccountState;
    // Sign out if we don't have a key fetch token.
    if (!keyFetchToken) {
      log.warn("improper fetchAndUnwrapKeys() call: token missing");
      await this.signOut();
      return currentState.resolve(null);
    }

    let {wrapKB} = await this.fetchKeys(keyFetchToken);

    let data = await currentState.getUserAccountData();

    // Sanity check that the user hasn't changed out from under us
    if (data.keyFetchToken !== keyFetchToken) {
      throw new Error("Signed in user changed while fetching keys!");
    }

    // Next statements must be synchronous until we updateUserAccountData
    // so that we don't risk getting into a weird state.
    let kBbytes = CryptoUtils.xor(CommonUtils.hexToBytes(data.unwrapBKey),
                                  wrapKB);

    if (logPII) {
      log.debug("kBbytes: " + kBbytes);
    }
    let updateData = {
      ...(await this._deriveKeys(data.uid, kBbytes)),
      keyFetchToken: null, // null values cause the item to be removed.
      unwrapBKey: null,
    };

    log.debug("Keys Obtained:" +
              DERIVED_KEYS_NAMES.map(k => `${k}=${!!updateData[k]}`).join(", "));
    if (logPII) {
      log.debug("Keys Obtained:" +
                DERIVED_KEYS_NAMES.map(k => `${k}=${updateData[k]}`).join(", "));
    }

    await currentState.updateUserAccountData(updateData);
    // We are now ready for business. This should only be invoked once
    // per setSignedInUser(), regardless of whether we've rebooted since
    // setSignedInUser() was called.
    await this.notifyObservers(ONVERIFIED_NOTIFICATION);
    // Some parts of the device registration depend on the Sync keys being available,
    // so let's re-trigger it now that we have them.
    await this.updateDeviceRegistration();
    data = await currentState.getUserAccountData();
    return currentState.resolve(data);
  },

  async _deriveKeys(uid, kBbytes) {
    return {
      kSync: CommonUtils.bytesAsHex((await this._deriveSyncKey(kBbytes))),
      kXCS: CommonUtils.bytesAsHex(this._deriveXClientState(kBbytes)),
      kExtSync: CommonUtils.bytesAsHex((await this._deriveWebExtSyncStoreKey(kBbytes))),
      kExtKbHash: CommonUtils.bytesAsHex(this._deriveWebExtKbHash(uid, kBbytes)),
    };
  },

  /**
   * Derive the Sync Key given the byte string kB.
   *
   * @returns HKDF(kB, undefined, "identity.mozilla.com/picl/v1/oldsync", 64)
   */
  _deriveSyncKey(kBbytes) {
    return CryptoUtils.hkdfLegacy(kBbytes, undefined,
                            "identity.mozilla.com/picl/v1/oldsync", 2 * 32);
  },

  /**
   * Derive the WebExtensions Sync Storage Key given the byte string kB.
   *
   * @returns HKDF(kB, undefined, "identity.mozilla.com/picl/v1/chrome.storage.sync", 64)
   */
  _deriveWebExtSyncStoreKey(kBbytes) {
    return CryptoUtils.hkdfLegacy(kBbytes, undefined,
                            "identity.mozilla.com/picl/v1/chrome.storage.sync",
                            2 * 32);
  },

  /**
   * Derive the WebExtensions kbHash given the byte string kB.
   *
   * @returns SHA256(uid + kB)
   */
  _deriveWebExtKbHash(uid, kBbytes) {
    return this._sha256(uid + kBbytes);
  },

  /**
   * Derive the X-Client-State header given the byte string kB.
   *
   * @returns SHA256(kB)[:16]
   */
  _deriveXClientState(kBbytes) {
    return this._sha256(kBbytes).slice(0, 16);
  },

  _sha256(bytes) {
    let hasher = Cc["@mozilla.org/security/hash;1"]
                    .createInstance(Ci.nsICryptoHash);
    hasher.init(hasher.SHA256);
    return CryptoUtils.digestBytes(bytes, hasher);
  },

  async getAssertionFromCert(data, keyPair, cert, audience) {
    log.debug("getAssertionFromCert");
    let options = {
      duration: ASSERTION_LIFETIME,
      localtimeOffsetMsec: this.localtimeOffsetMsec,
      now: this.now(),
    };
    let currentState = this.currentAccountState;
    // "audience" should look like "http://123done.org".
    // The generated assertion will expire in two minutes.
    let assertion = await new Promise((resolve, reject) => {
      jwcrypto.generateAssertion(cert, keyPair, audience, options, (err, signed) => {
        if (err) {
          log.error("getAssertionFromCert: " + err);
          reject(err);
        } else {
          log.debug("getAssertionFromCert returning signed: " + !!signed);
          if (logPII) {
            log.debug("getAssertionFromCert returning signed: " + signed);
          }
          resolve(signed);
        }
      });
    });
    return currentState.resolve(assertion);
  },

  getCertificateSigned(sessionToken, serializedPublicKey, lifetime) {
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

  /**
   * returns a promise that fires with {keyPair, certificate}.
   */
  async getKeypairAndCertificate(currentState) {
    // If the debugging pref to ignore cached authentication credentials is set for Sync,
    // then don't use any cached key pair/certificate, i.e., generate a new
    // one and get it signed.
    // The purpose of this pref is to expedite any auth errors as the result of a
    // expired or revoked FxA session token, e.g., from resetting or changing the FxA
    // password.
    let ignoreCachedAuthCredentials = Services.prefs.getBoolPref("services.sync.debug.ignoreCachedAuthCredentials", false);
    let mustBeValidUntil = this.now() + ASSERTION_USE_PERIOD;
    let accountData = await currentState.getUserAccountData(["cert", "keyPair", "sessionToken"]);

    let keyPairValid = !ignoreCachedAuthCredentials &&
                       accountData.keyPair &&
                       (accountData.keyPair.validUntil > mustBeValidUntil);
    let certValid = !ignoreCachedAuthCredentials &&
                    accountData.cert &&
                    (accountData.cert.validUntil > mustBeValidUntil);
    // TODO: get the lifetime from the cert's .exp field
    if (keyPairValid && certValid) {
      log.debug("getKeypairAndCertificate: already have keyPair and certificate");
      return {
        keyPair: accountData.keyPair.rawKeyPair,
        certificate: accountData.cert.rawCert,
      };
    }
    // We are definately going to generate a new cert, either because it has
    // already expired, or the keyPair has - and a new keyPair means we must
    // generate a new cert.

    // A keyPair has a longer lifetime than a cert, so it's possible we will
    // have a valid keypair but an expired cert, which means we can skip
    // keypair generation.
    // Either way, the cert will require hitting the network, so bail now if
    // we know that's going to fail.
    if (Services.io.offline) {
      throw new Error(ERROR_OFFLINE);
    }

    let keyPair;
    if (keyPairValid) {
      keyPair = accountData.keyPair;
    } else {
      let keyWillBeValidUntil = this.now() + KEY_LIFETIME;
      keyPair = await new Promise((resolve, reject) => {
        jwcrypto.generateKeyPair("DS160", (err, kp) => {
          if (err) {
            reject(err);
            return;
          }
          log.debug("got keyPair");
          resolve({
            rawKeyPair: kp,
            validUntil: keyWillBeValidUntil,
          });
        });
      });
    }

    // and generate the cert.
    let certWillBeValidUntil = this.now() + CERT_LIFETIME;
    let certificate = await this.getCertificateSigned(accountData.sessionToken,
                                                      keyPair.rawKeyPair.serializedPublicKey,
                                                      CERT_LIFETIME);
    log.debug("getCertificate got a new one: " + !!certificate);
    if (certificate) {
      // Cache both keypair and cert.
      let toUpdate = {
        keyPair,
        cert: {
          rawCert: certificate,
          validUntil: certWillBeValidUntil,
        },
      };
      await currentState.updateUserAccountData(toUpdate);
    }
    return {
      keyPair: keyPair.rawKeyPair,
      certificate,
    };
  },

  /**
   * @param {String} scope Single key bearing scope
   */
  async getKeyForScope(scope, {keyRotationTimestamp}) {
    if (scope !== SCOPE_OLD_SYNC) {
      throw new Error(`Unavailable key material for ${scope}`);
    }
    let {kSync, kXCS} = await this.getKeys();
    if (!kSync || !kXCS) {
      throw new Error("Could not find requested key.");
    }
    kXCS = ChromeUtils.base64URLEncode(CommonUtils.hexToArrayBuffer(kXCS), {pad: false});
    kSync = ChromeUtils.base64URLEncode(CommonUtils.hexToArrayBuffer(kSync), {pad: false});
    const kid = `${keyRotationTimestamp}-${kXCS}`;
    return {
      scope,
      kid,
      k: kSync,
      kty: "oct",
    };
  },

  /**
   * @param {String} scopes Space separated requested scopes
   */
  async getScopedKeys(scopes, clientId) {
    const {sessionToken} = await this._getVerifiedAccountOrReject();
    const keyData = await this.fxAccountsClient.getScopedKeyData(sessionToken, clientId, scopes);
    const scopedKeys = {};
    for (const [scope, data] of Object.entries(keyData)) {
      scopedKeys[scope] = await this.getKeyForScope(scope, data);
    }
    return scopedKeys;
  },

  getUserAccountData() {
    return this.currentAccountState.getUserAccountData();
  },

  isUserEmailVerified: function isUserEmailVerified(data) {
    return !!(data && data.verified);
  },

  /**
   * Setup for and if necessary do email verification polling.
   */
  loadAndPoll() {
    let currentState = this.currentAccountState;
    return currentState.getUserAccountData()
      .then(data => {
        if (data) {
          Services.telemetry.getHistogramById("FXA_CONFIGURED").add(1);
          if (!this.isUserEmailVerified(data)) {
            this.startPollEmailStatus(currentState, data.sessionToken, "browser-startup");
          }
        }
        return data;
      });
  },

  startVerifiedCheck(data) {
    log.debug("startVerifiedCheck", data && data.verified);
    if (logPII) {
      log.debug("startVerifiedCheck with user data", data);
    }

    // Get us to the verified state, then get the keys. This returns a promise
    // that will fire when we are completely ready.
    //
    // Login is truly complete once keys have been fetched, so once getKeys()
    // obtains and stores kSync kXCS kExtSync and kExtKbHash, it will fire the
    // onverified observer notification.

    // The callers of startVerifiedCheck never consume a returned promise (ie,
    // this is simply kicking off a background fetch) so we must add a rejection
    // handler to avoid runtime warnings about the rejection not being handled.
    this.whenVerified(data).then(
      () => this.getKeys(),
      err => log.info("startVerifiedCheck promise was rejected: " + err)
    );
  },

  whenVerified(data) {
    let currentState = this.currentAccountState;
    if (data.verified) {
      log.debug("already verified");
      return currentState.resolve(data);
    }
    if (!currentState.whenVerifiedDeferred) {
      log.debug("whenVerified promise starts polling for verified email");
      this.startPollEmailStatus(currentState, data.sessionToken, "start");
    }
    return currentState.whenVerifiedDeferred.promise.then(
      result => currentState.resolve(result)
    );
  },

  async notifyObservers(topic, data) {
    for (let f of this.observerPreloads) {
      try {
        await f();
      } catch (O_o) {}
    }
    log.debug("Notifying observers of " + topic);
    Services.obs.notifyObservers(null, topic, data);
  },

  startPollEmailStatus(currentState, sessionToken, why) {
    log.debug("entering startPollEmailStatus: " + why);
    // If we were already polling, stop and start again.  This could happen
    // if the user requested the verification email to be resent while we
    // were already polling for receipt of an earlier email.
    if (this.currentTimer) {
      log.debug("startPollEmailStatus starting while existing timer is running");
      clearTimeout(this.currentTimer);
      this.currentTimer = null;
    }

    this.pollStartDate = Date.now();
    if (!currentState.whenVerifiedDeferred) {
      currentState.whenVerifiedDeferred = PromiseUtils.defer();
      // This deferred might not end up with any handlers (eg, if sync
      // is yet to start up.)  This might cause "A promise chain failed to
      // handle a rejection" messages, so add an error handler directly
      // on the promise to log the error.
      currentState.whenVerifiedDeferred.promise.catch(err => {
        log.info("the wait for user verification was stopped: " + err);
      });
    }
    return this.pollEmailStatus(currentState, sessionToken, why);
  },

  // We return a promise for testing only. Other callers can ignore this,
  // since verification polling continues in the background.
  async pollEmailStatus(currentState, sessionToken, why) {
    log.debug("entering pollEmailStatus: " + why);
    let nextPollMs;
    try {
      const response = await this.checkEmailStatus(sessionToken, { reason: why });
      log.debug("checkEmailStatus -> " + JSON.stringify(response));
      if (response && response.verified) {
        await this.onPollEmailSuccess(currentState);
        return;
      }
    } catch (error) {
      if (error && error.code && error.code == 401) {
        let error = new Error("Verification status check failed");
        this._rejectWhenVerified(currentState, error);
        return;
      }
      if (error && error.retryAfter) {
        // If the server told us to back off, back off the requested amount.
        nextPollMs = (error.retryAfter + 3) * 1000;
        log.warn(`the server rejected our email status check and told us to try again in ${nextPollMs}ms`);
      } else {
        log.error(`checkEmailStatus failed to poll`, error);
      }
    }
    if (why == "push") {
      return;
    }
    let pollDuration = Date.now() - this.pollStartDate;
    // Polling session expired.
    if (pollDuration >= this.POLL_SESSION) {
      if (currentState.whenVerifiedDeferred) {
        let error = new Error("User email verification timed out.");
        this._rejectWhenVerified(currentState, error);
      }
      log.debug("polling session exceeded, giving up");
      return;
    }
    // Poll email status again after a short delay.
    if (nextPollMs === undefined) {
      let currentMinute = Math.ceil(pollDuration / 60000);
      nextPollMs = (why == "start" && currentMinute < this.VERIFICATION_POLL_START_SLOWDOWN_THRESHOLD) ?
                   this.VERIFICATION_POLL_TIMEOUT_INITIAL :
                   this.VERIFICATION_POLL_TIMEOUT_SUBSEQUENT;
    }
    this._scheduleNextPollEmailStatus(currentState, sessionToken, nextPollMs, why);
  },

  // Easy-to-mock testable method
  _scheduleNextPollEmailStatus(currentState, sessionToken, nextPollMs, why) {
    log.debug("polling with timeout = " + nextPollMs);
    this.currentTimer = setTimeout(() => {
      this.pollEmailStatus(currentState, sessionToken, why);
    }, nextPollMs);
  },

  async onPollEmailSuccess(currentState) {
    try {
      await currentState.updateUserAccountData({ verified: true });
      const accountData = await currentState.getUserAccountData();
      // Now that the user is verified, we can proceed to fetch keys
      if (currentState.whenVerifiedDeferred) {
        currentState.whenVerifiedDeferred.resolve(accountData);
        delete currentState.whenVerifiedDeferred;
      }
    } catch (e) {
      log.error(e);
    }
  },

  _rejectWhenVerified(currentState, error) {
    currentState.whenVerifiedDeferred.reject(error);
    delete currentState.whenVerifiedDeferred;
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
  async getOAuthToken(options = {}) {
    log.debug("getOAuthToken enter");
    let scope = options.scope;
    if (typeof scope === "string") {
      scope = [scope];
    }

    if (!scope || !scope.length) {
      throw this._error(ERROR_INVALID_PARAMETER, "Missing or invalid 'scope' option");
    }

    await this._getVerifiedAccountOrReject();

    // Early exit for a cached token.
    let currentState = this.currentAccountState;
    let cached = currentState.getCachedToken(scope);
    if (cached) {
      log.debug("getOAuthToken returning a cached token");
      return cached.token;
    }

    // We are going to hit the server - this is the string we pass to it.
    let scopeString = scope.join(" ");
    let client = options.client || this.oauthClient;
    let oAuthURL = client.serverURL.href;

    try {
      log.debug("getOAuthToken fetching new token from", oAuthURL);
      let assertion = await this.getAssertion(oAuthURL);
      let result = await client.getTokenFromAssertion(assertion, scopeString);
      let token = result.access_token;
      // If we got one, cache it.
      if (token) {
        let entry = {token, server: oAuthURL};
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
  },

  /**
   *
   * @param {String} clientId
   * @param {String} scope Space separated requested scopes
   * @param {Object} jwk
   */
  async createKeysJWE(clientId, scope, jwk) {
    let scopedKeys = await this.getScopedKeys(scope, clientId);
    scopedKeys = new TextEncoder().encode(JSON.stringify(scopedKeys));
    return jwcrypto.generateJWE(jwk, scopedKeys);
  },

  /**
   * Retrieves an OAuth authorization code
   *
   * @param {Object} options
   * @param options.client_id
   * @param options.state
   * @param options.scope
   * @param options.access_type
   * @param options.code_challenge_method
   * @param options.code_challenge
   * @param [options.keys_jwe]
   * @returns {Promise<Object>} Object containing "code" and "state" properties.
   */
  async authorizeOAuthCode(options) {
    await this._getVerifiedAccountOrReject();
    const client = this.oauthClient;
    const oAuthURL = client.serverURL.href;
    const params = {...options};
    if (params.keys_jwk) {
      const jwk = JSON.parse(new TextDecoder().decode(ChromeUtils.base64URLDecode(params.keys_jwk, {padding: "reject"})));
      params.keys_jwe = await this.createKeysJWE(params.client_id, params.scope, jwk);
      delete params.keys_jwk;
    }
    try {
      const assertion = await this.getAssertion(oAuthURL);
      return client.authorizeCodeFromAssertion(assertion, params);
    } catch (err) {
      throw this._errorToErrorClass(err);
    }
  },

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
  async removeCachedOAuthToken(options) {
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
  },

  async _getVerifiedAccountOrReject() {
    let data = await this.currentAccountState.getUserAccountData();
    if (!data) {
      // No signed-in user
      throw this._error(ERROR_NO_ACCOUNT);
    }
    if (!this.isUserEmailVerified(data)) {
      // Signed-in user has not verified email
      throw this._error(ERROR_UNVERIFIED_ACCOUNT);
    }
    return data;
  },

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
  _errorToErrorClass(aError) {
    if (aError.errno) {
      let error = SERVER_ERRNO_TO_ERROR[aError.errno];
      return this._error(ERROR_TO_GENERAL_ERROR_CLASS[error] || ERROR_UNKNOWN, aError);
    } else if (aError.message &&
        (aError.message === "INVALID_PARAMETER" ||
        aError.message === "NO_ACCOUNT" ||
        aError.message === "UNVERIFIED_ACCOUNT" ||
        aError.message === "AUTH_ERROR")) {
      return aError;
    }
    return this._error(ERROR_UNKNOWN, aError);
  },

  _error(aError, aDetails) {
    log.error("FxA rejecting with error ${aError}, details: ${aDetails}", {aError, aDetails});
    let reason = new Error(aError);
    if (aDetails) {
      reason.details = aDetails;
    }
    return reason;
  },

  /**
   * Get the user's account and profile data if it is locally cached. If
   * not cached it will return null, but cause the profile data to be fetched
   * in the background, after which a ON_PROFILE_CHANGE_NOTIFICATION
   * observer notification will be sent, at which time this can be called
   * again to obtain the most recent profile info.
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
  getSignedInUserProfile() {
    let currentState = this.currentAccountState;
    return this.profile.getProfile().then(
      profileData => {
        let profile = Cu.cloneInto(profileData, {});
        return currentState.resolve(profile);
      },
      error => {
        log.error("Could not retrieve profile data", error);
        return currentState.reject(error);
      }
    ).catch(err => Promise.reject(this._errorToErrorClass(err)));
  },

  // Attempt to update the auth server with whatever device details are stored
  // in the account data. Returns a promise that always resolves, never rejects.
  // If the promise resolves to a value, that value is the device id.
  async updateDeviceRegistration() {
    try {
      const signedInUser = await this.getSignedInUser();
      if (signedInUser) {
        await this._registerOrUpdateDevice(signedInUser);
      }
    } catch (error) {
      await this._logErrorAndResetDeviceRegistrationVersion(error);
    }
  },

  async handleDeviceDisconnection(deviceId) {
    const accountData = await this.currentAccountState.getUserAccountData();
    if (!accountData || !accountData.device) {
      // Nothing we can do here.
      return;
    }
    const localDeviceId = accountData.device.id;
    const isLocalDevice = (deviceId == localDeviceId);
    if (isLocalDevice) {
      this.signOut(true);
    }
    const data = JSON.stringify({ isLocalDevice });
    await this.notifyObservers(ON_DEVICE_DISCONNECTED_NOTIFICATION, data);
  },

  handleEmailUpdated(newEmail) {
    Services.prefs.setStringPref(PREF_LAST_FXA_USER, CryptoUtils.sha256Base64(newEmail));
    return this.currentAccountState.updateUserAccountData({ email: newEmail });
  },

  async handleAccountDestroyed(uid) {
    const accountData = await this.currentAccountState.getUserAccountData();
    const localUid = accountData ? accountData.uid : null;
    if (!localUid) {
      log.info(`Account destroyed push notification received, but we're already logged-out`);
      return null;
    }
    if (uid == localUid) {
      const data = JSON.stringify({ isLocalDevice: true });
      await this.notifyObservers(ON_DEVICE_DISCONNECTED_NOTIFICATION, data);
      return this.signOut(true);
    }
    log.info(
      `The destroyed account uid doesn't match with the local uid. ` +
      `Local: ${localUid}, account uid destroyed: ${uid}`);
    return null;
  },

  /**
   * Delete all the cached persisted credentials we store for FxA.
   *
   * @return Promise resolves when the user data has been persisted
  */
  resetCredentials() {
    // Delete all fields except those required for the user to
    // reauthenticate.
    let updateData = {};
    let clearField = field => {
      if (!FXA_PWDMGR_REAUTH_WHITELIST.has(field)) {
        updateData[field] = null;
      }
    };
    FXA_PWDMGR_PLAINTEXT_FIELDS.forEach(clearField);
    FXA_PWDMGR_SECURE_FIELDS.forEach(clearField);
    FXA_PWDMGR_MEMORY_FIELDS.forEach(clearField);

    let currentState = this.currentAccountState;
    return currentState.updateUserAccountData(updateData);
  },

  getProfileCache() {
    return this.currentAccountState.getUserAccountData(["profileCache"])
      .then(data => data ? data.profileCache : null);
  },

  setProfileCache(profileCache) {
    return this.currentAccountState.updateUserAccountData({
      profileCache,
    });
  },

  // @returns Promise<Subscription>.
  getPushSubscription() {
    return this.fxaPushService.getSubscription();
  },

  async availableCommands() {
    if (!Services.prefs.getBoolPref("identity.fxaccounts.commands.enabled", true)) {
      return {};
    }
    const sendTabKey = await this.commands.sendTab.getEncryptedKey();
    if (!sendTabKey) { // This will happen if the account is not verified yet.
      return {};
    }
    return {
      [COMMAND_SENDTAB]: sendTabKey,
    };
  },

  // If you change what we send to the FxA servers during device registration,
  // you'll have to bump the DEVICE_REGISTRATION_VERSION number to force older
  // devices to re-register when Firefox updates
  async _registerOrUpdateDevice(signedInUser) {
    const {sessionToken, device: currentDevice} = signedInUser;
    if (!sessionToken) {
      throw new Error("_registerOrUpdateDevice called without a session token");
    }

    try {
      const subscription = await this.fxaPushService.registerPushEndpoint();
      const deviceName = this._getDeviceName();
      let deviceOptions = {};

      // if we were able to obtain a subscription
      if (subscription && subscription.endpoint) {
        deviceOptions.pushCallback = subscription.endpoint;
        let publicKey = subscription.getKey("p256dh");
        let authKey = subscription.getKey("auth");
        if (publicKey && authKey) {
          deviceOptions.pushPublicKey = urlsafeBase64Encode(publicKey);
          deviceOptions.pushAuthKey = urlsafeBase64Encode(authKey);
        }
      }
      deviceOptions.availableCommands = await this.availableCommands();
      const availableCommandsKeys = Object.keys(deviceOptions.availableCommands).sort();

      let device;
      if (currentDevice && currentDevice.id) {
        log.debug("updating existing device details");
        device = await this.fxAccountsClient.updateDevice(
          sessionToken, currentDevice.id, deviceName, deviceOptions);
      } else {
        log.debug("registering new device details");
        device = await this.fxAccountsClient.registerDevice(
          sessionToken, deviceName, this._getDeviceType(), deviceOptions);
        Services.obs.notifyObservers(null, ON_NEW_DEVICE_ID);
      }

      // Get the freshest device props before updating them.
      let {device: deviceProps} = await this.getSignedInUser();
      await this.currentAccountState.updateUserAccountData({
        device: {
          ...deviceProps, // Copy the other properties (e.g. handledCommands).
          id: device.id,
          registrationVersion: this.DEVICE_REGISTRATION_VERSION,
          registeredCommandsKeys: availableCommandsKeys,
        },
      });
      return device.id;
    } catch (error) {
      return this._handleDeviceError(error, sessionToken);
    }
  },

  _getDeviceName() {
    return Utils.getDeviceName();
  },

  _getDeviceType() {
    return Utils.getDeviceType();
  },

  _handleDeviceError(error, sessionToken) {
    return Promise.resolve().then(() => {
      if (error.code === 400) {
        if (error.errno === ERRNO_UNKNOWN_DEVICE) {
          return this._recoverFromUnknownDevice();
        }

        if (error.errno === ERRNO_DEVICE_SESSION_CONFLICT) {
          return this._recoverFromDeviceSessionConflict(error, sessionToken);
        }
      }

      // `_handleTokenError` re-throws the error.
      return this._handleTokenError(error);
    }).catch(error =>
      this._logErrorAndResetDeviceRegistrationVersion(error)
    ).catch(() => {});
  },

  async _recoverFromUnknownDevice() {
    // FxA did not recognise the device id. Handle it by clearing the device
    // id on the account data. At next sync or next sign-in, registration is
    // retried and should succeed.
    log.warn("unknown device id, clearing the local device data");
    try {
      await this.currentAccountState.updateUserAccountData({device: null});
    } catch (error) {
      await this._logErrorAndResetDeviceRegistrationVersion(error);
    }
  },

  async _recoverFromDeviceSessionConflict(error, sessionToken) {
    // FxA has already associated this session with a different device id.
    // Perhaps we were beaten in a race to register. Handle the conflict:
    //   1. Fetch the list of devices for the current user from FxA.
    //   2. Look for ourselves in the list.
    //   3. If we find a match, set the correct device id and device registration
    //      version on the account data and return the correct device id. At next
    //      sync or next sign-in, registration is retried and should succeed.
    //   4. If we don't find a match, log the original error.
    log.warn("device session conflict, attempting to ascertain the correct device id");
    try {
      const devices = await this.fxAccountsClient.getDeviceList(sessionToken);
      const matchingDevices = devices.filter(device => device.isCurrentDevice);
      const length = matchingDevices.length;
      if (length === 1) {
        const deviceId = matchingDevices[0].id;
        await this.currentAccountState.updateUserAccountData({
          device: {
            id: deviceId,
            registrationVersion: null,
          },
        });
        return deviceId;
      }
      if (length > 1) {
        log.error("insane server state, " + length + " devices for this session");
      }
      await this._logErrorAndResetDeviceRegistrationVersion(error);
    } catch (secondError) {
      log.error("failed to recover from device-session conflict", secondError);
      await this._logErrorAndResetDeviceRegistrationVersion(error);
    }
    return null;
  },

  async _logErrorAndResetDeviceRegistrationVersion(error) {
    // Device registration should never cause other operations to fail.
    // If we've reached this point, just log the error and reset the device
    // on the account data. At next sync or next sign-in,
    // registration will be retried.
    log.error("device registration failed", error);
    try {
      this.currentAccountState.updateUserAccountData({
        device: null,
      });
    } catch (secondError) {
      log.error(
        "failed to reset the device registration version, device registration won't be retried",
        secondError);
    }
  },

  _handleTokenError(err) {
    if (!err || err.code != 401 || err.errno != ERRNO_INVALID_AUTH_TOKEN) {
      throw err;
    }
    log.warn("recovering from invalid token error", err);
    return this.accountStatus().then(exists => {
      if (!exists) {
        // Delete all local account data. Since the account no longer
        // exists, we can skip the remote calls.
        log.info("token invalidated because the account no longer exists");
        return this.signOut(true);
      }
      log.info("clearing credentials to handle invalid token error");
      return this.resetCredentials();
    }).then(() => Promise.reject(err));
  },
};


// A getter for the instance to export
XPCOMUtils.defineLazyGetter(this, "fxAccounts", function() {
  let a = new FxAccounts();

  // XXX Bug 947061 - We need a strategy for resuming email verification after
  // browser restart
  a.loadAndPoll();

  return a;
});

var EXPORTED_SYMBOLS = ["fxAccounts", "FxAccounts"];
