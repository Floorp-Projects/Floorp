/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["FxAccountsClient"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-common/hawk.js");
Cu.import("resource://services-crypto/utils.js");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/Credentials.jsm");

// Default can be changed by the preference 'identity.fxaccounts.auth.uri'
let _host = "https://api-accounts.dev.lcip.org/v1";
try {
  _host = Services.prefs.getCharPref("identity.fxaccounts.auth.uri");
} catch(keepDefault) {}

const HOST = _host;
this.FxAccountsClient = function(host = HOST) {
  this.host = host;

  // The FxA auth server expects requests to certain endpoints to be authorized
  // using Hawk.
  this.hawk = new HawkClient(host);
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
   * Create a new Firefox Account and authenticate
   *
   * @param email
   *        The email address for the account (utf8)
   * @param password
   *        The user's password
   * @return Promise
   *        Returns a promise that resolves to an object:
   *        {
   *          uid: the user's unique ID (hex)
   *          sessionToken: a session token (hex)
   *          keyFetchToken: a key fetch token (hex)
   *        }
   */
  signUp: function(email, password) {
    return Credentials.setup(email, password).then((creds) => {
      let data = {
        email: creds.emailUTF8,
        authPW: CommonUtils.bytesAsHex(creds.authPW),
      };
      return this._request("/account/create", "POST", null, data);
    });
  },

  /**
   * Authenticate and create a new session with the Firefox Account API server
   *
   * @param email
   *        The email address for the account (utf8)
   * @param password
   *        The user's password
   * @return Promise
   *        Returns a promise that resolves to an object:
   *        {
   *          uid: the user's unique ID (hex)
   *          sessionToken: a session token (hex)
   *          keyFetchToken: a key fetch token (hex)
   *          verified: flag indicating verification status of the email
   *        }
   */
  signIn: function signIn(email, password) {
    return Credentials.setup(email, password).then((creds) => {
      let data = {
        email: creds.emailUTF8,
        authPW: CommonUtils.bytesAsHex(creds.authPW),
      };
      return this._request("/account/login", "POST", null, data);
    });
  },

  /**
   * Destroy the current session with the Firefox Account API server
   *
   * @param sessionTokenHex
   *        The session token encoded in hex
   * @return Promise
   */
  signOut: function (sessionTokenHex) {
    return this._request("/session/destroy", "POST",
      this._deriveHawkCredentials(sessionTokenHex, "sessionToken"));
  },

  /**
   * Check the verification status of the user's FxA email address
   *
   * @param sessionTokenHex
   *        The current session token encoded in hex
   * @return Promise
   */
  recoveryEmailStatus: function (sessionTokenHex) {
    return this._request("/recovery_email/status", "GET",
      this._deriveHawkCredentials(sessionTokenHex, "sessionToken"));
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
      this._deriveHawkCredentials(sessionTokenHex, "sessionToken"));
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
    let creds = this._deriveHawkCredentials(keyFetchTokenHex, "keyFetchToken");
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
   *        Returns a promise that resolves to the signed certificate. The certificate
   *        can be used to generate a Persona assertion.
   */
  signCertificate: function (sessionTokenHex, serializedPublicKey, lifetime) {
    let creds = this._deriveHawkCredentials(sessionTokenHex, "sessionToken");

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
   * The FxA auth server expects requests to certain endpoints to be authorized using Hawk.
   * Hawk credentials are derived using shared secrets, which depend on the context
   * (e.g. sessionToken vs. keyFetchToken).
   *
   * @param tokenHex
   *        The current session token encoded in hex
   * @param context
   *        A context for the credentials
   * @param size
   *        The size in bytes of the expected derived buffer
   * @return credentials
   *        Returns an object:
   *        {
   *          algorithm: sha256
   *          id: the Hawk id (from the first 32 bytes derived)
   *          key: the Hawk key (from bytes 32 to 64)
   *          extra: size - 64 extra bytes
   *        }
   */
  _deriveHawkCredentials: function (tokenHex, context, size) {
    let token = CommonUtils.hexToBytes(tokenHex);
    let out = CryptoUtils.hkdf(token, undefined, Credentials.keyWord(context), size || 3 * 32);

    return {
      algorithm: "sha256",
      key: out.slice(32, 64),
      extra: out.slice(64),
      id: CommonUtils.bytesAsHex(out.slice(0, 32))
    };
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

    this.hawk.request(path, method, credentials, jsonPayload).then(
      (responseText) => {
        try {
          let response = JSON.parse(responseText);
          deferred.resolve(response);
        } catch (err) {
          log.error("json parse error on response: " + responseText);
          deferred.reject({error: err});
        }
      },

      (error) => {
        log.error("request error: " + JSON.stringify(error));
        deferred.reject(error);
      }
    );

    return deferred.promise;
  },
};

