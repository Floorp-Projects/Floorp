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
Cu.import("resource://gre/modules/FxAccountsUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "jwcrypto",
  "resource://gre/modules/identity/jwcrypto.jsm");

// All properties exposed by the public FxAccounts API.
let publicProperties = [
  "getAccountsURI",
  "getAssertion",
  "getKeys",
  "getSignedInUser",
  "loadAndPoll",
  "localtimeOffsetMsec",
  "now",
  "promiseAccountsForceSigninURI",
  "resendVerificationEmail",
  "setSignedInUser",
  "signOut",
  "version",
  "whenVerified"
];

/**
 * The public API's constructor.
 */
this.FxAccounts = function (mockInternal) {
  let internal = new FxAccountsInternal();
  let external = {};

  // Copy all public properties to the 'external' object.
  let prototype = FxAccountsInternal.prototype;
  let options = {keys: publicProperties, bind: internal};
  FxAccountsUtils.copyObjectProperties(prototype, external, options);

  // Copy all of the mock's properties to the internal object.
  if (mockInternal && !mockInternal.onlySetInternal) {
    FxAccountsUtils.copyObjectProperties(mockInternal, internal);
  }

  if (mockInternal) {
    // Exposes the internal object for testing only.
    external.internal = internal;
  }

  return Object.freeze(external);
}

/**
 * The internal API's constructor.
 */
function FxAccountsInternal() {
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

  // We don't reference |profileDir| in the top-level module scope
  // as we may be imported before we know where it is.
  this.signedInUserStorage = new JSONStorage({
    filename: DEFAULT_STORAGE_FILENAME,
    baseDir: OS.Constants.Path.profileDir,
  });
}

/**
 * The internal API's prototype.
 */
FxAccountsInternal.prototype = {

  /**
   * The current data format's version number.
   */
  version: DATA_FORMAT_VERSION,

  /**
   * Return the current time in milliseconds as an integer.  Allows tests to
   * manipulate the date to simulate certificate expiration.
   */
  now: function() {
    return this.fxAccountsClient.now();
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
    log.debug("fetchKeys: " + keyFetchToken);
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
   *        }
   *        or null if no user is signed in.
   */
  getSignedInUser: function getSignedInUser() {
    return this.getUserAccountData().then(data => {
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
    });
  },

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
    this.abortExistingFlow();

    let record = {version: this.version, accountData: credentials};
    // Cache a clone of the credentials object.
    this.signedInUser = JSON.parse(JSON.stringify(record));

    // This promise waits for storage, but not for verification.
    // We're telling the caller that this is durable now.
    return this.signedInUserStorage.set(record).then(() => {
      this.notifyObservers(ONLOGIN_NOTIFICATION);
      if (!this.isUserEmailVerified(credentials)) {
        this.startVerifiedCheck(credentials);
      }
    });
  },

  /**
   * returns a promise that fires with the assertion.  If there is no verified
   * signed-in user, fires with null.
   */
  getAssertion: function getAssertion(audience) {
    log.debug("enter getAssertion()");
    let mustBeValidUntil = this.now() + ASSERTION_LIFETIME;
    return this.getUserAccountData().then(data => {
      if (!data) {
        // No signed-in user
        return null;
      }
      if (!this.isUserEmailVerified(data)) {
        // Signed-in user has not verified email
        return null;
      }
      return this.getKeyPair(mustBeValidUntil).then(keyPair => {
        return this.getCertificate(data, keyPair, mustBeValidUntil)
          .then(cert => {
            return this.getAssertionFromCert(data, keyPair, cert, audience);
          });
      });
    });
  },

  /**
   * Resend the verification email fot the currently signed-in user.
   *
   */
  resendVerificationEmail: function resendVerificationEmail() {
    return this.getSignedInUser().then(data => {
      // If the caller is asking for verification to be re-sent, and there is
      // no signed-in user to begin with, this is probably best regarded as an
      // error.
      if (data) {
        this.pollEmailStatus(data.sessionToken, "start");
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
        this.fetchAndUnwrapKeys(data.keyFetchToken).then(data => {
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
        yield this.signOut();
        return null;
      }
      let myGenerationCount = this.generationCount;

      let {kA, wrapKB} = yield this.fetchKeys(keyFetchToken);

      let data = yield this.getUserAccountData();

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
      if (this.generationCount !== myGenerationCount) {
        return null;
      }

      yield this.setUserAccountData(data);

      // We are now ready for business. This should only be invoked once
      // per setSignedInUser(), regardless of whether we've rebooted since
      // setSignedInUser() was called.
      this.notifyObservers(ONVERIFIED_NOTIFICATION);
      return data;
    }.bind(this));
  },

  getAssertionFromCert: function(data, keyPair, cert, audience) {
    log.debug("getAssertionFromCert");
    let payload = {};
    let d = Promise.defer();
    let options = {
      localtimeOffsetMsec: this.localtimeOffsetMsec,
      now: this.now()
    };
    // "audience" should look like "http://123done.org".
    // The generated assertion will expire in two minutes.
    jwcrypto.generateAssertion(cert, keyPair, audience, options, (err, signed) => {
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
      log.debug("whenVerified promise starts polling for verified email");
      this.pollEmailStatus(data.sessionToken, "start");
    }
    return this.whenVerifiedPromise.promise;
  },

  notifyObservers: function(topic) {
    log.debug("Notifying observers of " + topic);
    Services.obs.notifyObservers(null, topic, null);
  },

  pollEmailStatus: function pollEmailStatus(sessionToken, why) {
    let myGenerationCount = this.generationCount;
    log.debug("entering pollEmailStatus: " + why + " " + myGenerationCount);
    if (why == "start") {
      // If we were already polling, stop and start again.  This could happen
      // if the user requested the verification email to be resent while we
      // were already polling for receipt of an earlier email.
      this.pollTimeRemaining = this.POLL_SESSION;
      if (!this.whenVerifiedPromise) {
        this.whenVerifiedPromise = Promise.defer();
      }
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
    return this.signedInUserStorage.get().then(record => {
      record.accountData = accountData;
      this.signedInUser = record;
      return this.signedInUserStorage.set(record)
        .then(() => accountData);
    });
  },

  // Return the URI of the remote UI flows.
  getAccountsURI: function() {
    let url = Services.urlFormatter.formatURLPref("identity.fxaccounts.remote.uri");
    if (!/^https:/.test(url)) { // Comment to un-break emacs js-mode highlighting
      throw new Error("Firefox Accounts server must use HTTPS");
    }
    return url;
  },

  // Returns a promise that resolves with the URL to use to force a re-signin
  // of the current account.
  promiseAccountsForceSigninURI: function() {
    let url = Services.urlFormatter.formatURLPref("identity.fxaccounts.remote.force_auth.uri");
    if (!/^https:/.test(url)) { // Comment to un-break emacs js-mode highlighting
      throw new Error("Firefox Accounts server must use HTTPS");
    }
    // but we need to append the email address onto a query string.
    return this.getSignedInUser().then(accountData => {
      if (!accountData) {
        return null;
      }
      let newQueryPortion = url.indexOf("?") == -1 ? "?" : "&";
      newQueryPortion += "email=" + encodeURIComponent(accountData.email);
      return url + newQueryPortion;
    });
  }
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
  a.loadAndPoll();

  return a;
});
