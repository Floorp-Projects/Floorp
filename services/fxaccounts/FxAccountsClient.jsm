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

// Default can be changed by the preference 'identity.fxaccounts.auth.uri'
let _host = "https://api-accounts.dev.lcip.org/v1";
try {
  _host = Services.prefs.getCharPref("identity.fxaccounts.auth.uri");
} catch(keepDefault) {}

const HOST = _host;
const PROTOCOL_VERSION = "identity.mozilla.com/picl/v1/";

function KW(context) {
  // This is used as a salt.  It's specified by the protocol.  Note that the
  // value of PROTOCOL_VERSION does not refer in any wy to the version of the
  // Firefox Accounts API.  For this reason, it is not exposed as a pref.
  //
  // See:
  // https://github.com/mozilla/fxa-auth-server/wiki/onepw-protocol#creating-the-account
  return PROTOCOL_VERSION + context;
}

function stringToHex(str) {
  let encoder = new TextEncoder("utf-8");
  let bytes = encoder.encode(str);
  return bytesToHex(bytes);
}

// XXX Sadly, CommonUtils.bytesAsHex doesn't handle typed arrays.
function bytesToHex(bytes) {
  let hex = [];
  for (let i = 0; i < bytes.length; i++) {
    hex.push((bytes[i] >>> 4).toString(16));
    hex.push((bytes[i] & 0xF).toString(16));
  }
  return hex.join("");
}

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
   *          uid: the user's unique ID
   *          sessionToken: a session token
   *        }
   */
  signUp: function (email, password) {
    let uid;
    let hexEmail = stringToHex(email);
    let uidPromise = this._request("/raw_password/account/create", "POST", null,
                          {email: hexEmail, password: password});

    return uidPromise.then((result) => {
      uid = result.uid;
      return this.signIn(email, password)
        .then(function(result) {
          result.uid = uid;
          return result;
        });
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
   *          uid: the user's unique ID
   *          sessionToken: a session token
   *          verified: flag indicating verification status of the email
   *        }
   */
  signIn: function signIn(email, password) {
    let hexEmail = stringToHex(email);
    return this._request("/raw_password/session/create", "POST", null,
                         {email: hexEmail, password: password});
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
   *          kA: an encryption key for recevorable data
   *          wrapKB: an encryption key that requires knowledge of the user's password
   *        }
   */
  accountKeys: function (keyFetchTokenHex) {
    let creds = this._deriveHawkCredentials(keyFetchTokenHex, "keyFetchToken");
    let keyRequestKey = creds.extra.slice(0, 32);
    let morecreds = CryptoUtils.hkdf(keyRequestKey, undefined,
                                     KW("account/keys"), 3 * 32);
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
    let hexEmail = stringToHex(email);
    return this._request("/auth/start", "POST", null, { email: hexEmail })
      .then(
        // the account exists
        (result) => true,
        (err) => {
          log.error("accountExists: error: " + JSON.stringify(err));
          // the account doesn't exist
          if (err.errno === 102) {
            log.debug("returning false for errno 102");
            return false;
          }
          // propogate other request errors
          throw err;
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
    let out = CryptoUtils.hkdf(token, undefined, KW(context), size || 3 * 32);

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
          deferred.reject({error: err});
        }
      },

      (error) => {
        deferred.reject(error);
      }
    );

    return deferred.promise;
  },
};

