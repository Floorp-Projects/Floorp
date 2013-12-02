/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["FxAccountsClient"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-crypto/utils.js");

// Default can be changed by the preference 'identity.fxaccounts.auth.uri'
let _host = "https://api-accounts.dev.lcip.org/v1";
try {
  _host = Services.prefs.getCharPref("identity.fxaccounts.auth.uri");
} catch(keepDefault) {}

const HOST = _host;
const PREFIX_NAME = "identity.mozilla.com/picl/v1/";

const XMLHttpRequest =
  Components.Constructor("@mozilla.org/xmlextras/xmlhttprequest;1");


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
};

this.FxAccountsClient.prototype = {
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
   *          isVerified: flag indicating verification status of the email
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
   *        The session token endcoded in hex
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
   *        The current session token endcoded in hex
   * @return Promise
   */
  recoveryEmailStatus: function (sessionTokenHex) {
    return this._request("/recovery_email/status", "GET",
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
                                     PREFIX_NAME + "account/keys", 3 * 32);
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
   *        The current session token endcoded in hex
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
            err => {dump("HAWK.signCertificate error: " + err + "\n");
                    throw err;});
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
          // the account doesn't exist
          if (err.errno === 102) {
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
   *        The current session token endcoded in hex
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
    let out = CryptoUtils.hkdf(token, undefined, PREFIX_NAME + context, size || 3 * 32);

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
    let xhr = new XMLHttpRequest({mozSystem: true});
    let URI = this.host + path;
    let payload;

    xhr.mozBackgroundRequest = true;

    if (jsonPayload) {
      payload = JSON.stringify(jsonPayload);
    }

    xhr.open(method, URI);
    xhr.channel.loadFlags = Ci.nsIChannel.LOAD_BYPASS_CACHE |
                            Ci.nsIChannel.INHIBIT_CACHING;

    // When things really blow up, reconstruct an error object that follows the general format
    // of the server on error responses.
    function constructError(err) {
      return { error: err, message: xhr.statusText, code: xhr.status, errno: xhr.status };
    }

    xhr.onerror = function() {
      deferred.reject(constructError('Request failed'));
    };

    xhr.onload = function onload() {
      try {
        let response = JSON.parse(xhr.responseText);
        if (xhr.status !== 200 || response.error) {
          // In this case, the response is an object with error information.
          return deferred.reject(response);
        }
        deferred.resolve(response);
      } catch (e) {
        deferred.reject(constructError(e));
      }
    };

    let uri = Services.io.newURI(URI, null, null);

    if (credentials) {
      let header = CryptoUtils.computeHAWK(uri, method, {
                          credentials: credentials,
                          payload: payload,
                          contentType: "application/json"
                        });
      xhr.setRequestHeader("authorization", header.field);
    }

    xhr.setRequestHeader("Content-Type", "application/json");
    xhr.send(payload);

    return deferred.promise;
  },
};

