/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["FxAccountsClient"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-common/hawkclient.js");
Cu.import("resource://services-common/hawkrequest.js");
Cu.import("resource://services-crypto/utils.js");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/Credentials.jsm");

const HOST = Services.prefs.getCharPref("identity.fxaccounts.auth.uri");

const SIGNIN = "/account/login";
const SIGNUP = "/account/create";

this.FxAccountsClient = function(host = HOST) {
  this.host = host;

  // The FxA auth server expects requests to certain endpoints to be authorized
  // using Hawk.
  this.hawk = new HawkClient(host);
  this.hawk.observerPrefix = "FxA:hawk";

  // Manage server backoff state. C.f.
  // https://github.com/mozilla/fxa-auth-server/blob/master/docs/api.md#backoff-protocol
  this.backoffError = null;
};

this.FxAccountsClient.prototype = {

  /**
   * Return client clock offset, in milliseconds, as determined by hawk client.
   * Provided because callers should not have to know about hawk
   * implementation.
   *
   * The offset is the number of milliseconds that must be added to the client
   * clock to make it equal to the server clock.  For example, if the client is
   * five minutes ahead of the server, the localtimeOffsetMsec will be -300000.
   */
  get localtimeOffsetMsec() {
    return this.hawk.localtimeOffsetMsec;
  },

  /*
   * Return current time in milliseconds
   *
   * Not used by this module, but made available to the FxAccounts.jsm
   * that uses this client.
   */
  now: function() {
    return this.hawk.now();
  },

  /**
   * Common code from signIn and signUp.
   *
   * @param path
   *        Request URL path. Can be /account/create or /account/login
   * @param email
   *        The email address for the account (utf8)
   * @param password
   *        The user's password
   * @param [getKeys=false]
   *        If set to true the keyFetchToken will be retrieved
   * @param [retryOK=true]
   *        If capitalization of the email is wrong and retryOK is set to true,
   *        we will retry with the suggested capitalization from the server
   * @return Promise
   *        Returns a promise that resolves to an object:
   *        {
   *          authAt: authentication time for the session (seconds since epoch)
   *          email: the primary email for this account
   *          keyFetchToken: a key fetch token (hex)
   *          sessionToken: a session token (hex)
   *          uid: the user's unique ID (hex)
   *          unwrapBKey: used to unwrap kB, derived locally from the
   *                      password (not revealed to the FxA server)
   *          verified (optional): flag indicating verification status of the
   *                               email
   *        }
   */
  _createSession: function(path, email, password, getKeys=false,
                           retryOK=true) {
    return Credentials.setup(email, password).then((creds) => {
      let data = {
        authPW: CommonUtils.bytesAsHex(creds.authPW),
        email: email,
      };
      let keys = getKeys ? "?keys=true" : "";

      return this._request(path + keys, "POST", null, data).then(
        // Include the canonical capitalization of the email in the response so
        // the caller can set its signed-in user state accordingly.
        result => {
          result.email = data.email;
          result.unwrapBKey = CommonUtils.bytesAsHex(creds.unwrapBKey);

          return result;
        },
        error => {
          log.debug("Session creation failed", error);
          // If the user entered an email with different capitalization from
          // what's stored in the database (e.g., Greta.Garbo@gmail.COM as
          // opposed to greta.garbo@gmail.com), the server will respond with a
          // errno 120 (code 400) and the expected capitalization of the email.
          // We retry with this email exactly once.  If successful, we use the
          // server's version of the email as the signed-in-user's email. This
          // is necessary because the email also serves as salt; so we must be
          // in agreement with the server on capitalization.
          //
          // API reference:
          // https://github.com/mozilla/fxa-auth-server/blob/master/docs/api.md
          if (ERRNO_INCORRECT_EMAIL_CASE === error.errno && retryOK) {
            if (!error.email) {
              log.error("Server returned errno 120 but did not provide email");
              throw error;
            }
            return this._createSession(path, error.email, password, getKeys,
                                       false);
          }
          throw error;
        }
      );
    });
  },

  /**
   * Create a new Firefox Account and authenticate
   *
   * @param email
   *        The email address for the account (utf8)
   * @param password
   *        The user's password
   * @param [getKeys=false]
   *        If set to true the keyFetchToken will be retrieved
   * @return Promise
   *        Returns a promise that resolves to an object:
   *        {
   *          uid: the user's unique ID (hex)
   *          sessionToken: a session token (hex)
   *          keyFetchToken: a key fetch token (hex),
   *          unwrapBKey: used to unwrap kB, derived locally from the
   *                      password (not revealed to the FxA server)
   *        }
   */
  signUp: function(email, password, getKeys=false) {
    return this._createSession(SIGNUP, email, password, getKeys,
                               false /* no retry */);
  },

  /**
   * Authenticate and create a new session with the Firefox Account API server
   *
   * @param email
   *        The email address for the account (utf8)
   * @param password
   *        The user's password
   * @param [getKeys=false]
   *        If set to true the keyFetchToken will be retrieved
   * @return Promise
   *        Returns a promise that resolves to an object:
   *        {
   *          authAt: authentication time for the session (seconds since epoch)
   *          email: the primary email for this account
   *          keyFetchToken: a key fetch token (hex)
   *          sessionToken: a session token (hex)
   *          uid: the user's unique ID (hex)
   *          unwrapBKey: used to unwrap kB, derived locally from the
   *                      password (not revealed to the FxA server)
   *          verified: flag indicating verification status of the email
   *        }
   */
  signIn: function signIn(email, password, getKeys=false) {
    return this._createSession(SIGNIN, email, password, getKeys,
                               true /* retry */);
  },

  /**
   * Destroy the current session with the Firefox Account API server
   *
   * @param sessionTokenHex
   *        The session token encoded in hex
   * @return Promise
   */
  signOut: function (sessionTokenHex, options = {}) {
    let path = "/session/destroy";
    if (options.service) {
      path += "?service=" + encodeURIComponent(options.service);
    }
    return this._request(path, "POST",
      deriveHawkCredentials(sessionTokenHex, "sessionToken"));
  },

  /**
   * Check the verification status of the user's FxA email address
   *
   * @param sessionTokenHex
   *        The current session token encoded in hex
   * @return Promise
   */
  recoveryEmailStatus: function (sessionTokenHex, options = {}) {
    let path = "/recovery_email/status";
    if (options.reason) {
      path += "?reason=" + encodeURIComponent(options.reason);
    }

    return this._request(path, "GET",
      deriveHawkCredentials(sessionTokenHex, "sessionToken"));
  },

  /**
   * Resend the verification email for the user
   *
   * @param sessionTokenHex
   *        The current token encoded in hex
   * @return Promise
   */
  resendVerificationEmail: function(sessionTokenHex) {
    return this._request("/recovery_email/resend_code", "POST",
      deriveHawkCredentials(sessionTokenHex, "sessionToken"));
  },

  /**
   * Retrieve encryption keys
   *
   * @param keyFetchTokenHex
   *        A one-time use key fetch token encoded in hex
   * @return Promise
   *        Returns a promise that resolves to an object:
   *        {
   *          kA: an encryption key for recevorable data (bytes)
   *          wrapKB: an encryption key that requires knowledge of the
   *                  user's password (bytes)
   *        }
   */
  accountKeys: function (keyFetchTokenHex) {
    let creds = deriveHawkCredentials(keyFetchTokenHex, "keyFetchToken");
    let keyRequestKey = creds.extra.slice(0, 32);
    let morecreds = CryptoUtils.hkdf(keyRequestKey, undefined,
                                     Credentials.keyWord("account/keys"), 3 * 32);
    let respHMACKey = morecreds.slice(0, 32);
    let respXORKey = morecreds.slice(32, 96);

    return this._request("/account/keys", "GET", creds).then(resp => {
      if (!resp.bundle) {
        throw new Error("failed to retrieve keys");
      }

      let bundle = CommonUtils.hexToBytes(resp.bundle);
      let mac = bundle.slice(-32);

      let hasher = CryptoUtils.makeHMACHasher(Ci.nsICryptoHMAC.SHA256,
        CryptoUtils.makeHMACKey(respHMACKey));

      let bundleMAC = CryptoUtils.digestBytes(bundle.slice(0, -32), hasher);
      if (mac !== bundleMAC) {
        throw new Error("error unbundling encryption keys");
      }

      let keyAWrapB = CryptoUtils.xor(respXORKey, bundle.slice(0, 64));

      return {
        kA: keyAWrapB.slice(0, 32),
        wrapKB: keyAWrapB.slice(32)
      };
    });
  },

  /**
   * Sends a public key to the FxA API server and returns a signed certificate
   *
   * @param sessionTokenHex
   *        The current session token encoded in hex
   * @param serializedPublicKey
   *        A public key (usually generated by jwcrypto)
   * @param lifetime
   *        The lifetime of the certificate
   * @return Promise
   *        Returns a promise that resolves to the signed certificate.
   *        The certificate can be used to generate a Persona assertion.
   * @throws a new Error
   *         wrapping any of these HTTP code/errno pairs:
   *           https://github.com/mozilla/fxa-auth-server/blob/master/docs/api.md#response-12
   */
  signCertificate: function (sessionTokenHex, serializedPublicKey, lifetime) {
    let creds = deriveHawkCredentials(sessionTokenHex, "sessionToken");

    let body = { publicKey: serializedPublicKey,
                 duration: lifetime };
    return Promise.resolve()
      .then(_ => this._request("/certificate/sign", "POST", creds, body))
      .then(resp => resp.cert,
            err => {
              log.error("HAWK.signCertificate error: " + JSON.stringify(err));
              throw err;
            });
  },

  /**
   * Determine if an account exists
   *
   * @param email
   *        The email address to check
   * @return Promise
   *        The promise resolves to true if the account exists, or false
   *        if it doesn't. The promise is rejected on other errors.
   */
  accountExists: function (email) {
    return this.signIn(email, "").then(
      (cantHappen) => {
        throw new Error("How did I sign in with an empty password?");
      },
      (expectedError) => {
        switch (expectedError.errno) {
          case ERRNO_ACCOUNT_DOES_NOT_EXIST:
            return false;
            break;
          case ERRNO_INCORRECT_PASSWORD:
            return true;
            break;
          default:
            // not so expected, any more ...
            throw expectedError;
            break;
        }
      }
    );
  },

  /**
   * Given the uid of an existing account (not an arbitrary email), ask
   * the server if it still exists via /account/status.
   *
   * Used for differentiating between password change and account deletion.
   */
  accountStatus: function(uid) {
    return this._request("/account/status?uid="+uid, "GET").then(
      (result) => {
        return result.exists;
      },
      (error) => {
        log.error("accountStatus failed with: " + error);
        return Promise.reject(error);
      }
    );
  },

  /**
   * Register a new device
   *
   * @method registerDevice
   * @param  sessionTokenHex
   *         Session token obtained from signIn
   * @param  name
   *         Device name
   * @param  type
   *         Device type (mobile|desktop)
   * @param  [options]
   *         Extra device options
   * @param  [options.pushCallback]
   *         `pushCallback` push endpoint callback
   * @return Promise
   *         Resolves to an object:
   *         {
   *           id: Device identifier
   *           createdAt: Creation time (milliseconds since epoch)
   *           name: Name of device
   *           type: Type of device (mobile|desktop)
   *         }
   */
  registerDevice(sessionTokenHex, name, type, options = {}) {
    let path = "/account/device";

    let creds = deriveHawkCredentials(sessionTokenHex, "sessionToken");
    let body = { name, type };

    if (options.pushCallback) {
      body.pushCallback = options.pushCallback;
    }

    return this._request(path, "POST", creds, body);
  },

  /**
   * Update the session or name for an existing device
   *
   * @method updateDevice
   * @param  sessionTokenHex
   *         Session token obtained from signIn
   * @param  id
   *         Device identifier
   * @param  name
   *         Device name
   * @param  [options]
   *         Extra device options
   * @param  [options.pushCallback]
   *         `pushCallback` push endpoint callback
   * @return Promise
   *         Resolves to an object:
   *         {
   *           id: Device identifier
   *           name: Device name
   *         }
   */
  updateDevice(sessionTokenHex, id, name, options = {}) {
    let path = "/account/device";

    let creds = deriveHawkCredentials(sessionTokenHex, "sessionToken");
    let body = { id, name };
    if (options.pushCallback) {
      body.pushCallback = options.pushCallback;
    }

    return this._request(path, "POST", creds, body);
  },

  /**
   * Delete a device and its associated session token, signing the user
   * out of the server.
   *
   * @method signOutAndDestroyDevice
   * @param  sessionTokenHex
   *         Session token obtained from signIn
   * @param  id
   *         Device identifier
   * @param  [options]
   *         Options object
   * @param  [options.service]
   *         `service` query parameter
   * @return Promise
   *         Resolves to an empty object:
   *         {}
   */
  signOutAndDestroyDevice(sessionTokenHex, id, options = {}) {
    let path = "/account/device/destroy";

    if (options.service) {
      path += "?service=" + encodeURIComponent(options.service);
    }

    let creds = deriveHawkCredentials(sessionTokenHex, "sessionToken");
    let body = { id };

    return this._request(path, "POST", creds, body);
  },

  /**
   * Get a list of currently registered devices
   *
   * @method getDeviceList
   * @param  sessionTokenHex
   *         Session token obtained from signIn
   * @return Promise
   *         Resolves to an array of objects:
   *         [
   *           {
   *             id: Device id
   *             isCurrentDevice: Boolean indicating whether the item
   *                              represents the current device
   *             name: Device name
   *             type: Device type (mobile|desktop)
   *           },
   *           ...
   *         ]
   */
  getDeviceList(sessionTokenHex) {
    let path = "/account/devices";
    let creds = deriveHawkCredentials(sessionTokenHex, "sessionToken");

    return this._request(path, "GET", creds, {});
  },

  _clearBackoff: function() {
      this.backoffError = null;
  },

  /**
   * A general method for sending raw API calls to the FxA auth server.
   * All request bodies and responses are JSON.
   *
   * @param path
   *        API endpoint path
   * @param method
   *        The HTTP request method
   * @param credentials
   *        Hawk credentials
   * @param jsonPayload
   *        A JSON payload
   * @return Promise
   *        Returns a promise that resolves to the JSON response of the API call,
   *        or is rejected with an error. Error responses have the following properties:
   *        {
   *          "code": 400, // matches the HTTP status code
   *          "errno": 107, // stable application-level error number
   *          "error": "Bad Request", // string description of the error type
   *          "message": "the value of salt is not allowed to be undefined",
   *          "info": "https://docs.dev.lcip.og/errors/1234" // link to more info on the error
   *        }
   */
  _request: function hawkRequest(path, method, credentials, jsonPayload) {
    let deferred = Promise.defer();

    // We were asked to back off.
    if (this.backoffError) {
      log.debug("Received new request during backoff, re-rejecting.");
      deferred.reject(this.backoffError);
      return deferred.promise;
    }

    this.hawk.request(path, method, credentials, jsonPayload).then(
      (response) => {
        try {
          let responseObj = JSON.parse(response.body);
          deferred.resolve(responseObj);
        } catch (err) {
          log.error("json parse error on response: " + response.body);
          deferred.reject({error: err});
        }
      },

      (error) => {
        log.error("error " + method + "ing " + path + ": " + JSON.stringify(error));
        if (error.retryAfter) {
          log.debug("Received backoff response; caching error as flag.");
          this.backoffError = error;
          // Schedule clearing of cached-error-as-flag.
          CommonUtils.namedTimer(
            this._clearBackoff,
            error.retryAfter * 1000,
            this,
            "fxaBackoffTimer"
           );
        }
        deferred.reject(error);
      }
    );

    return deferred.promise;
  },
};

function isInvalidTokenError(error) {
  if (error.code != 401) {
    return false;
  }
  switch (error.errno) {
    case ERRNO_INVALID_AUTH_TOKEN:
    case ERRNO_INVALID_AUTH_TIMESTAMP:
    case ERRNO_INVALID_AUTH_NONCE:
      return true;
  }
  return false;
}
