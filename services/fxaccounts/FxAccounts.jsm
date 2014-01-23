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
Cu.import("resource://gre/modules/FxAccountsClient.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");

XPCOMUtils.defineLazyModuleGetter(this, "jwcrypto",
                                  "resource://gre/modules/identity/jwcrypto.jsm");

InternalMethods = function(mock) {
  this.cert = null;
  this.keyPair = null;
  this.signedInUser = null;
  this.version = DATA_FORMAT_VERSION;

  // Make a local copy of these constants so we can mock it in testing
  this.POLL_STEP = POLL_STEP;
  this.POLL_SESSION = POLL_SESSION;
  // We will create this.pollTimeRemaining below; it will initially be
  // set to the value of POLL_SESSION.

  // We interact with the Firefox Accounts auth server in order to confirm that
  // a user's email has been verified and also to fetch the user's keys from
  // the server.  We manage these processes in possibly long-lived promises
  // that are internal to this object (never exposed to callers).  Because
  // Firefox Accounts allows for only one logged-in user, and because it's
  // conceivable that while we are waiting to verify one identity, a caller
  // could start verification on a second, different identity, we need to be
  // able to abort all work on the first sign-in process.  The currentTimer and
  // generationCount are used for this purpose.
  this.whenVerifiedPromise = null;
  this.whenKeysReadyPromise = null;
  this.currentTimer = null;
  this.generationCount = 0;

  this.fxAccountsClient = new FxAccountsClient();

  if (mock) { // Testing.
    Object.keys(mock).forEach((prop) => {
      log.debug("InternalMethods: mocking: " + prop);
      this[prop] = mock[prop];
    });
  }
  if (!this.signedInUserStorage) {
    // Normal (i.e., non-testing) initialization.
    // We don't reference |profileDir| in the top-level module scope
    // as we may be imported before we know where it is.
    this.signedInUserStorage = new JSONStorage({
      filename: DEFAULT_STORAGE_FILENAME,
      baseDir: OS.Constants.Path.profileDir,
    });
  }
}
InternalMethods.prototype = {

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
    log.debug("fetchKeys: " + keyFetchToken);
    return this.fxAccountsClient.accountKeys(keyFetchToken);
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
    this.generationCount++;
    log.debug("generationCount: " + this.generationCount);

    if (this.whenVerifiedPromise) {
      this.whenVerifiedPromise.reject(
        new Error("Verification aborted; Another user signing in"));
      this.whenVerifiedPromise = null;
    }

    if (this.whenKeysReadyPromise) {
      this.whenKeysReadyPromise.reject(
        new Error("KeyFetch aborted; Another user signing in"));
      this.whenKeysReadyPromise = null;
    }
  },

  signOut: function signOut() {
    this.abortExistingFlow();
    this.signedInUser = null; // clear in-memory cache
    return this.signedInUserStorage.set(null).then(() => {
      this.notifyObservers(ONLOGOUT_NOTIFICATION);
    });
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
    return this.getUserAccountData().then((data) => {
      if (!data) {
        throw new Error("Can't get keys; User is not signed in");
      }
      if (data.kA && data.kB) {
        return data;
      }
      if (!this.whenKeysReadyPromise) {
        this.whenKeysReadyPromise = Promise.defer();
        return this.fetchAndUnwrapKeys(data.keyFetchToken)
          .then((data) => {
            if (this.whenKeysReadyPromise) {
              this.whenKeysReadyPromise.resolve(data);
            }
          });
      }
      return this.whenKeysReadyPromise.promise;
      });
   },

  fetchAndUnwrapKeys: function(keyFetchToken) {
    log.debug("fetchAndUnwrapKeys: token: " + keyFetchToken);
    return Task.spawn(function* task() {
      // Sign out if we don't have a key fetch token.
      if (!keyFetchToken) {
        yield internal.signOut();
        return null;
      }
      let myGenerationCount = internal.generationCount;

      let {kA, wrapKB} = yield internal.fetchKeys(keyFetchToken);

      let data = yield internal.getUserAccountData();

      // Sanity check that the user hasn't changed out from under us
      if (data.keyFetchToken !== keyFetchToken) {
        throw new Error("Signed in user changed while fetching keys!");
      }

      // Next statements must be synchronous until we setUserAccountData
      // so that we don't risk getting into a weird state.
      let kB_hex = CryptoUtils.xor(CommonUtils.hexToBytes(data.unwrapBKey),
                                   wrapKB);

      log.debug("kB_hex: " + kB_hex);
      data.kA = CommonUtils.bytesAsHex(kA);
      data.kB = CommonUtils.bytesAsHex(kB_hex);

      delete data.keyFetchToken;

      log.debug("Keys Obtained: kA=" + data.kA + ", kB=" + data.kB);

      // Before writing any data, ensure that a new flow hasn't been
      // started behind our backs.
      if (internal.generationCount !== myGenerationCount) {
        return null;
      }

      yield internal.setUserAccountData(data);

      // We are now ready for business. This should only be invoked once
      // per setSignedInUser(), regardless of whether we've rebooted since
      // setSignedInUser() was called.
      internal.notifyObservers(ONVERIFIED_NOTIFICATION);
      return data;
    }.bind(this));
  },

  getAssertionFromCert: function(data, keyPair, cert, audience) {
    log.debug("getAssertionFromCert");
    let payload = {};
    let d = Promise.defer();
    // "audience" should look like "http://123done.org".
    // The generated assertion will expire in two minutes.
    jwcrypto.generateAssertion(cert, keyPair, audience, function(err, signed) {
      if (err) {
        log.error("getAssertionFromCert: " + err);
        d.reject(err);
      } else {
        log.debug("getAssertionFromCert returning signed: " + signed);
        d.resolve(signed);
      }
    });
    return d.promise;
  },

  getCertificate: function(data, keyPair, mustBeValidUntil) {
    log.debug("getCertificate" + JSON.stringify(this.signedInUserStorage));
    // TODO: get the lifetime from the cert's .exp field
    if (this.cert && this.cert.validUntil > mustBeValidUntil) {
      log.debug(" getCertificate already had one");
      return Promise.resolve(this.cert.cert);
    }
    // else get our cert signed
    let willBeValidUntil = this.now() + CERT_LIFETIME;
    return this.getCertificateSigned(data.sessionToken,
                                     keyPair.serializedPublicKey,
                                     CERT_LIFETIME)
      .then((cert) => {
        this.cert = {
          cert: cert,
          validUntil: willBeValidUntil
        };
        return cert;
      }
    );
  },

  getCertificateSigned: function(sessionToken, serializedPublicKey, lifetime) {
    log.debug("getCertificateSigned: " + sessionToken + " " + serializedPublicKey);
    return this.fxAccountsClient.signCertificate(sessionToken,
                                                 JSON.parse(serializedPublicKey),
                                                 lifetime);
  },

  getKeyPair: function(mustBeValidUntil) {
    if (this.keyPair && (this.keyPair.validUntil > mustBeValidUntil)) {
      log.debug("getKeyPair: already have a keyPair");
      return Promise.resolve(this.keyPair.keyPair);
    }
    // Otherwse, create a keypair and set validity limit.
    let willBeValidUntil = this.now() + KEY_LIFETIME;
    let d = Promise.defer();
    jwcrypto.generateKeyPair("DS160", (err, kp) => {
      if (err) {
        d.reject(err);
      } else {
        this.keyPair = {
          keyPair: kp,
          validUntil: willBeValidUntil
        };
        log.debug("got keyPair");
        delete this.cert;
        d.resolve(this.keyPair.keyPair);
      }
    });
    return d.promise;
  },

  getUserAccountData: function() {
    // Skip disk if user is cached.
    if (this.signedInUser) {
      return Promise.resolve(this.signedInUser.accountData);
    }

    let deferred = Promise.defer();
    this.signedInUserStorage.get()
      .then((user) => {
        log.debug("getUserAccountData -> " + JSON.stringify(user));
        if (user && user.version == this.version) {
          log.debug("setting signed in user");
          this.signedInUser = user;
        }
        deferred.resolve(user ? user.accountData : null);
      },
      (err) => {
        if (err instanceof OS.File.Error && err.becauseNoSuchFile) {
          // File hasn't been created yet.  That will be done
          // on the first call to getSignedInUser
          deferred.resolve(null);
        } else {
          deferred.reject(err);
        }
      }
    );

    return deferred.promise;
  },

  isUserEmailVerified: function isUserEmailVerified(data) {
    return !!(data && data.verified);
  },

  /**
   * Setup for and if necessary do email verification polling.
   */
  loadAndPoll: function() {
    return this.getUserAccountData()
      .then(data => {
        if (data && !this.isUserEmailVerified(data)) {
          this.pollEmailStatus(data.sessionToken, "start");
        }
        return data;
      });
  },

  startVerifiedCheck: function(data) {
    log.debug("startVerifiedCheck " + JSON.stringify(data));
    // Get us to the verified state, then get the keys. This returns a promise
    // that will fire when we are completely ready.
    //
    // Login is truly complete once keys have been fetched, so once getKeys()
    // obtains and stores kA and kB, it will fire the onverified observer
    // notification.
    return this.whenVerified(data)
      .then((data) => this.getKeys(data));
  },

  whenVerified: function(data) {
    if (data.verified) {
      log.debug("already verified");
      return Promise.resolve(data);
    }
    if (!this.whenVerifiedPromise) {
      this.whenVerifiedPromise = Promise.defer();
      log.debug("whenVerified promise starts polling for verified email");
      this.pollEmailStatus(data.sessionToken, "start");
    }
    return this.whenVerifiedPromise.promise;
  },

  notifyObservers: function(topic) {
    log.debug("Notifying observers of " + topic);
    Services.obs.notifyObservers(null, topic, null);
  },

  /**
   * Give xpcshell tests an override point for duration testing. This is
   * necessary because the tests need to manipulate the date in order to
   * simulate certificate expiration.
   */
  now: function() {
    return Date.now();
  },

  pollEmailStatus: function pollEmailStatus(sessionToken, why) {
    let myGenerationCount = this.generationCount;
    log.debug("entering pollEmailStatus: " + why + " " + myGenerationCount);
    if (why == "start") {
      if (this.currentTimer) {
        // safety check - this case should have been caught on
        // entry with setSignedInUser
        throw new Error("Already polling for email status");
      }
      this.pollTimeRemaining = this.POLL_SESSION;
    }

    this.checkEmailStatus(sessionToken)
      .then((response) => {
        log.debug("checkEmailStatus -> " + JSON.stringify(response));
        // Check to see if we're still current.
        // If for some ghastly reason we are not, stop processing.
        if (this.generationCount !== myGenerationCount) {
          log.debug("generation count differs from " + this.generationCount + " - aborting");
          log.debug("sessionToken on abort is " + sessionToken);
          return;
        }

        if (response && response.verified) {
          // Bug 947056 - Server should be able to tell FxAccounts.jsm to back
          // off or stop polling altogether
          this.getUserAccountData()
            .then((data) => {
              data.verified = true;
              return this.setUserAccountData(data);
            })
            .then((data) => {
              // Now that the user is verified, we can proceed to fetch keys
              if (this.whenVerifiedPromise) {
                this.whenVerifiedPromise.resolve(data);
                delete this.whenVerifiedPromise;
              }
            });
        } else {
          log.debug("polling with step = " + this.POLL_STEP);
          this.pollTimeRemaining -= this.POLL_STEP;
          log.debug("time remaining: " + this.pollTimeRemaining);
          if (this.pollTimeRemaining > 0) {
            this.currentTimer = setTimeout(() => {
              this.pollEmailStatus(sessionToken, "timer")}, this.POLL_STEP);
            log.debug("started timer " + this.currentTimer);
          } else {
            if (this.whenVerifiedPromise) {
              this.whenVerifiedPromise.reject(
                new Error("User email verification timed out.")
              );
              delete this.whenVerifiedPromise;
            }
          }
        }
      });
    },

  setUserAccountData: function(accountData) {
    return this.signedInUserStorage.get().then((record) => {
      record.accountData = accountData;
      this.signedInUser = record;
      return this.signedInUserStorage.set(record)
        .then(() => accountData);
    });
  }
};

let internal = null;

/**
 * FxAccounts delegates private methods to an instance of InternalMethods,
 * which is not exported. The xpcshell tests need two overrides:
 *  1) Access to the real internal.signedInUserStorage.
 *  2) The ability to mock InternalMethods.
 * If mockInternal is undefined, we are live.
 * If mockInternal.onlySetInternal is present, we are executing the first
 * case by binding internal to the FxAccounts instance.
 * Otherwise if we have a mock instance, we are executing the second case.
 */
this.FxAccounts = function(mockInternal) {
  let mocks = mockInternal;
  if (mocks && mocks.onlySetInternal) {
    mocks = null;
  }
  internal = new InternalMethods(mocks);
  if (mockInternal) {
    // Exposes the internal object for testing only.
    this.internal = internal;
  }
}
this.FxAccounts.prototype = Object.freeze({
  version: DATA_FORMAT_VERSION,

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
   * Set the current user signed in to Firefox Accounts.
   *
   * @param credentials
   *        The credentials object obtained by logging in or creating
   *        an account on the FxA server:
   *        {
   *          email: The users email address
   *          uid: The user's unique id
   *          sessionToken: Session for the FxA server
   *          keyFetchToken: an unused keyFetchToken
   *          verified: true/false
   *        }
   * @return Promise
   *         The promise resolves to null when the data is saved
   *         successfully and is rejected on error.
   */
  setSignedInUser: function setSignedInUser(credentials) {
    log.debug("setSignedInUser - aborting any existing flows");
    internal.abortExistingFlow();

    let record = {version: this.version, accountData: credentials };
    // Cache a clone of the credentials object.
    internal.signedInUser = JSON.parse(JSON.stringify(record));

    // This promise waits for storage, but not for verification.
    // We're telling the caller that this is durable now.
    return internal.signedInUserStorage.set(record)
      .then(() => {
        internal.notifyObservers(ONLOGIN_NOTIFICATION);
        if (!internal.isUserEmailVerified(credentials)) {
          internal.startVerifiedCheck(credentials);
        }
      });
  },

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
   *        }
   *        or null if no user is signed in.
   */
  getSignedInUser: function getSignedInUser() {
    return internal.getUserAccountData()
      .then((data) => {
        if (!data) {
          return null;
        }
        if (!internal.isUserEmailVerified(data)) {
          // If the email is not verified, start polling for verification,
          // but return null right away.  We don't want to return a promise
          // that might not be fulfilled for a long time.
          internal.startVerifiedCheck(data);
        }
        return data;
      });
  },

  /**
   * returns a promise that fires with the assertion.  If there is no verified
   * signed-in user, fires with null.
   */
  getAssertion: function getAssertion(audience) {
    log.debug("enter getAssertion()");
    let mustBeValidUntil = internal.now() + ASSERTION_LIFETIME;
    return internal.getUserAccountData()
      .then((data) => {
        if (!data) {
          // No signed-in user
          return null;
        }
        if (!internal.isUserEmailVerified(data)) {
          // Signed-in user has not verified email
          return null;
        }
        return internal.getKeyPair(mustBeValidUntil)
          .then((keyPair) => {
            return internal.getCertificate(data, keyPair, mustBeValidUntil)
              .then((cert) => {
                return internal.getAssertionFromCert(data, keyPair,
                                                     cert, audience)
              });
          });
      });
  },

  getKeys: function() {
    return internal.getKeys();
  },

  whenVerified: function(userData) {
    return internal.whenVerified(userData);
  },


  /**
   * Sign the current user out.
   *
   * @return Promise
   *         The promise is rejected if a storage error occurs.
   */
  signOut: function signOut() {
    return internal.signOut();
  },

  // Return the URI of the remote UI flows.
  getAccountsURI: function() {
    let url = Services.urlFormatter.formatURLPref("identity.fxaccounts.remote.uri");
    if (!/^https:/.test(url)) { // Comment to un-break emacs js-mode highlighting
      throw new Error("Firefox Accounts server must use HTTPS");
    }
    return url;
  }
});

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
};

JSONStorage.prototype = {
  set: function(contents) {
    return OS.File.makeDir(this.baseDir, {ignoreExisting: true})
      .then(CommonUtils.writeJSON.bind(null, contents, this.path));
  },

  get: function() {
    return CommonUtils.readJSON(this.path);
  }
};

// A getter for the instance to export
XPCOMUtils.defineLazyGetter(this, "fxAccounts", function() {
  let a = new FxAccounts();

  // XXX Bug 947061 - We need a strategy for resuming email verification after
  // browser restart
  internal.loadAndPoll();

  return a;
});

