/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["FxAccountsClient"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { CommonUtils } = ChromeUtils.import(
  "resource://services-common/utils.js"
);
const { HawkClient } = ChromeUtils.import(
  "resource://services-common/hawkclient.js"
);
const { deriveHawkCredentials } = ChromeUtils.import(
  "resource://services-common/hawkrequest.js"
);
const { CryptoUtils } = ChromeUtils.import(
  "resource://services-crypto/utils.js"
);
const {
  ERRNO_ACCOUNT_DOES_NOT_EXIST,
  ERRNO_INCORRECT_EMAIL_CASE,
  ERRNO_INCORRECT_PASSWORD,
  ERRNO_INVALID_AUTH_NONCE,
  ERRNO_INVALID_AUTH_TIMESTAMP,
  ERRNO_INVALID_AUTH_TOKEN,
  log,
} = ChromeUtils.import("resource://gre/modules/FxAccountsCommon.js");
const { Credentials } = ChromeUtils.import(
  "resource://gre/modules/Credentials.jsm"
);

const HOST_PREF = "identity.fxaccounts.auth.uri";

const SIGNIN = "/account/login";
const SIGNUP = "/account/create";

var FxAccountsClient = function(host = Services.prefs.getCharPref(HOST_PREF)) {
  this.host = host;

  // The FxA auth server expects requests to certain endpoints to be authorized
  // using Hawk.
  this.hawk = new HawkClient(host);
  this.hawk.observerPrefix = "FxA:hawk";

  // Manage server backoff state. C.f.
  // https://github.com/mozilla/fxa-auth-server/blob/master/docs/api.md#backoff-protocol
  this.backoffError = null;
};

FxAccountsClient.prototype = {
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
  now() {
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
  _createSession(path, email, password, getKeys = false, retryOK = true) {
    return Credentials.setup(email, password).then(creds => {
      let data = {
        authPW: CommonUtils.bytesAsHex(creds.authPW),
        email,
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
            return this._createSession(
              path,
              error.email,
              password,
              getKeys,
              false
            );
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
  signUp(email, password, getKeys = false) {
    return this._createSession(
      SIGNUP,
      email,
      password,
      getKeys,
      false /* no retry */
    );
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
  signIn: function signIn(email, password, getKeys = false) {
    return this._createSession(
      SIGNIN,
      email,
      password,
      getKeys,
      true /* retry */
    );
  },

  /**
   * Check the status of a session given a session token
   *
   * @param sessionTokenHex
   *        The session token encoded in hex
   * @return Promise
   *        Resolves with a boolean indicating if the session is still valid
   */
  async sessionStatus(sessionTokenHex) {
    const credentials = await deriveHawkCredentials(
      sessionTokenHex,
      "sessionToken"
    );
    return this._request("/session/status", "GET", credentials).then(
      () => Promise.resolve(true),
      error => {
        if (isInvalidTokenError(error)) {
          return Promise.resolve(false);
        }
        throw error;
      }
    );
  },

  /**
   * List all the clients connected to the authenticated user's account,
   * including devices, OAuth clients, and web sessions.
   *
   * @param sessionTokenHex
   *        The session token encoded in hex
   * @return Promise
   */
  async attachedClients(sessionTokenHex) {
    const credentials = await deriveHawkCredentials(
      sessionTokenHex,
      "sessionToken"
    );
    return this._requestWithHeaders(
      "/account/attached_clients",
      "GET",
      credentials
    );
  },

  /**
   * Retrieves an OAuth authorization code.
   *
   * @param String sessionTokenHex
   *        The session token encoded in hex
   * @param {Object} options
   * @param options.client_id
   * @param options.state
   * @param options.scope
   * @param options.access_type
   * @param options.code_challenge_method
   * @param options.code_challenge
   * @param [options.keys_jwe]
   * @returns {Promise<Object>} Object containing `code` and `state`.
   */
  async oauthAuthorize(sessionTokenHex, options) {
    const credentials = await deriveHawkCredentials(
      sessionTokenHex,
      "sessionToken"
    );
    const body = {
      client_id: options.client_id,
      response_type: "code",
      state: options.state,
      scope: options.scope,
      access_type: options.access_type,
      code_challenge: options.code_challenge,
      code_challenge_method: options.code_challenge_method,
    };
    if (options.keys_jwe) {
      body.keys_jwe = options.keys_jwe;
    }
    return this._request("/oauth/authorization", "POST", credentials, body);
  },

  /**
   * Destroy an OAuth access token or refresh token.
   *
   * @param String clientId
   * @param String token The token to be revoked.
   */
  async oauthDestroy(clientId, token) {
    const body = {
      client_id: clientId,
      token,
    };
    return this._request("/oauth/destroy", "POST", null, body);
  },

  /**
   * Query for the information required to derive
   * scoped encryption keys requested by the specified OAuth client.
   *
   * @param sessionTokenHex
   *        The session token encoded in hex
   * @param clientId
   * @param scope
   *        Space separated list of scopes
   * @return Promise
   */
  async getScopedKeyData(sessionTokenHex, clientId, scope) {
    if (!clientId) {
      throw new Error("Missing 'clientId' parameter");
    }
    if (!scope) {
      throw new Error("Missing 'scope' parameter");
    }
    const params = {
      client_id: clientId,
      scope,
    };
    const credentials = await deriveHawkCredentials(
      sessionTokenHex,
      "sessionToken"
    );
    return this._request(
      "/account/scoped-key-data",
      "POST",
      credentials,
      params
    );
  },

  /**
   * Destroy the current session with the Firefox Account API server and its
   * associated device.
   *
   * @param sessionTokenHex
   *        The session token encoded in hex
   * @return Promise
   */
  async signOut(sessionTokenHex, options = {}) {
    const credentials = await deriveHawkCredentials(
      sessionTokenHex,
      "sessionToken"
    );
    let path = "/session/destroy";
    if (options.service) {
      path += "?service=" + encodeURIComponent(options.service);
    }
    return this._request(path, "POST", credentials);
  },

  /**
   * Check the verification status of the user's FxA email address
   *
   * @param sessionTokenHex
   *        The current session token encoded in hex
   * @return Promise
   */
  async recoveryEmailStatus(sessionTokenHex, options = {}) {
    const credentials = await deriveHawkCredentials(
      sessionTokenHex,
      "sessionToken"
    );
    let path = "/recovery_email/status";
    if (options.reason) {
      path += "?reason=" + encodeURIComponent(options.reason);
    }

    return this._request(path, "GET", credentials);
  },

  /**
   * Resend the verification email for the user
   *
   * @param sessionTokenHex
   *        The current token encoded in hex
   * @return Promise
   */
  async resendVerificationEmail(sessionTokenHex) {
    const credentials = await deriveHawkCredentials(
      sessionTokenHex,
      "sessionToken"
    );
    return this._request("/recovery_email/resend_code", "POST", credentials);
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
  async accountKeys(keyFetchTokenHex) {
    let creds = await deriveHawkCredentials(keyFetchTokenHex, "keyFetchToken");
    let keyRequestKey = creds.extra.slice(0, 32);
    let morecreds = await CryptoUtils.hkdfLegacy(
      keyRequestKey,
      undefined,
      Credentials.keyWord("account/keys"),
      3 * 32
    );
    let respHMACKey = morecreds.slice(0, 32);
    let respXORKey = morecreds.slice(32, 96);

    const resp = await this._request("/account/keys", "GET", creds);
    if (!resp.bundle) {
      throw new Error("failed to retrieve keys");
    }

    let bundle = CommonUtils.hexToBytes(resp.bundle);
    let mac = bundle.slice(-32);
    let key = CommonUtils.byteStringToArrayBuffer(respHMACKey);
    // CryptoUtils.hmac takes ArrayBuffers as inputs for the key and data and
    // returns an ArrayBuffer.
    let bundleMAC = await CryptoUtils.hmac(
      "SHA-256",
      key,
      CommonUtils.byteStringToArrayBuffer(bundle.slice(0, -32))
    );
    if (mac !== CommonUtils.arrayBufferToByteString(bundleMAC)) {
      throw new Error("error unbundling encryption keys");
    }

    let keyAWrapB = CryptoUtils.xor(respXORKey, bundle.slice(0, 64));

    return {
      kA: keyAWrapB.slice(0, 32),
      wrapKB: keyAWrapB.slice(32),
    };
  },

  /**
   * Obtain an OAuth access token by authenticating using a session token.
   *
   * @param {String} sessionTokenHex
   *        The session token encoded in hex
   * @param {String} clientId
   * @param {String} scope
   *        List of space-separated scopes.
   * @param {Number} ttl
   *        Token time to live.
   * @return {Promise<Object>} Object containing an `access_token`.
   */
  async accessTokenWithSessionToken(sessionTokenHex, clientId, scope, ttl) {
    const credentials = await deriveHawkCredentials(
      sessionTokenHex,
      "sessionToken"
    );
    const body = {
      client_id: clientId,
      grant_type: "fxa-credentials",
      scope,
      ttl,
    };
    return this._request("/oauth/token", "POST", credentials, body);
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
  accountExists(email) {
    return this.signIn(email, "").then(
      cantHappen => {
        throw new Error("How did I sign in with an empty password?");
      },
      expectedError => {
        switch (expectedError.errno) {
          case ERRNO_ACCOUNT_DOES_NOT_EXIST:
            return false;
          case ERRNO_INCORRECT_PASSWORD:
            return true;
          default:
            // not so expected, any more ...
            throw expectedError;
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
  accountStatus(uid) {
    return this._request("/account/status?uid=" + uid, "GET").then(
      result => {
        return result.exists;
      },
      error => {
        log.error("accountStatus failed", error);
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
   * @param  [options.availableCommands]
   *         Available commands for this device
   * @param  [options.pushCallback]
   *         `pushCallback` push endpoint callback
   * @param  [options.pushPublicKey]
   *         `pushPublicKey` push public key (URLSafe Base64 string)
   * @param  [options.pushAuthKey]
   *         `pushAuthKey` push auth secret (URLSafe Base64 string)
   * @return Promise
   *         Resolves to an object:
   *         {
   *           id: Device identifier
   *           createdAt: Creation time (milliseconds since epoch)
   *           name: Name of device
   *           type: Type of device (mobile|desktop)
   *         }
   */
  async registerDevice(sessionTokenHex, name, type, options = {}) {
    let path = "/account/device";

    let creds = await deriveHawkCredentials(sessionTokenHex, "sessionToken");
    let body = { name, type };

    if (options.pushCallback) {
      body.pushCallback = options.pushCallback;
    }
    if (options.pushPublicKey && options.pushAuthKey) {
      body.pushPublicKey = options.pushPublicKey;
      body.pushAuthKey = options.pushAuthKey;
    }
    body.availableCommands = options.availableCommands;

    return this._request(path, "POST", creds, body);
  },

  /**
   * Sends a message to other devices. Must conform with the push payload schema:
   * https://github.com/mozilla/fxa-auth-server/blob/master/docs/pushpayloads.schema.json
   *
   * @method notifyDevice
   * @param  sessionTokenHex
   *         Session token obtained from signIn
   * @param  deviceIds
   *         Devices to send the message to. If null, will be sent to all devices.
   * @param  excludedIds
   *         Devices to exclude when sending to all devices (deviceIds must be null).
   * @param  payload
   *         Data to send with the message
   * @return Promise
   *         Resolves to an empty object:
   *         {}
   */
  async notifyDevices(
    sessionTokenHex,
    deviceIds,
    excludedIds,
    payload,
    TTL = 0
  ) {
    const credentials = await deriveHawkCredentials(
      sessionTokenHex,
      "sessionToken"
    );
    if (deviceIds && excludedIds) {
      throw new Error(
        "You cannot specify excluded devices if deviceIds is set."
      );
    }
    const body = {
      to: deviceIds || "all",
      payload,
      TTL,
    };
    if (excludedIds) {
      body.excluded = excludedIds;
    }
    return this._request("/account/devices/notify", "POST", credentials, body);
  },

  /**
   * Retrieves pending commands for our device.
   *
   * @method getCommands
   * @param  sessionTokenHex - Session token obtained from signIn
   * @param  [index] - If specified, only messages received after the one who
   *                   had that index will be retrieved.
   * @param  [limit] - Maximum number of messages to retrieve.
   */
  async getCommands(sessionTokenHex, { index, limit }) {
    const credentials = await deriveHawkCredentials(
      sessionTokenHex,
      "sessionToken"
    );
    const params = new URLSearchParams();
    if (index != undefined) {
      params.set("index", index);
    }
    if (limit != undefined) {
      params.set("limit", limit);
    }
    const path = `/account/device/commands?${params.toString()}`;
    return this._request(path, "GET", credentials);
  },

  /**
   * Invokes a command on another device.
   *
   * @method invokeCommand
   * @param  sessionTokenHex - Session token obtained from signIn
   * @param  command - Name of the command to invoke
   * @param  target - Recipient device ID.
   * @param  payload
   * @return Promise
   *         Resolves to the request's response, (which should be an empty object)
   */
  async invokeCommand(sessionTokenHex, command, target, payload) {
    const credentials = await deriveHawkCredentials(
      sessionTokenHex,
      "sessionToken"
    );
    const body = {
      command,
      target,
      payload,
    };
    return this._request(
      "/account/devices/invoke_command",
      "POST",
      credentials,
      body
    );
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
   * @param  [options.availableCommands]
   *         Available commands for this device
   * @param  [options.pushCallback]
   *         `pushCallback` push endpoint callback
   * @param  [options.pushPublicKey]
   *         `pushPublicKey` push public key (URLSafe Base64 string)
   * @param  [options.pushAuthKey]
   *         `pushAuthKey` push auth secret (URLSafe Base64 string)
   * @return Promise
   *         Resolves to an object:
   *         {
   *           id: Device identifier
   *           name: Device name
   *         }
   */
  async updateDevice(sessionTokenHex, id, name, options = {}) {
    let path = "/account/device";

    let creds = await deriveHawkCredentials(sessionTokenHex, "sessionToken");
    let body = { id, name };
    if (options.pushCallback) {
      body.pushCallback = options.pushCallback;
    }
    if (options.pushPublicKey && options.pushAuthKey) {
      body.pushPublicKey = options.pushPublicKey;
      body.pushAuthKey = options.pushAuthKey;
    }
    body.availableCommands = options.availableCommands;

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
  async getDeviceList(sessionTokenHex) {
    let path = "/account/devices";
    let creds = await deriveHawkCredentials(sessionTokenHex, "sessionToken");

    return this._request(path, "GET", creds, {});
  },

  _clearBackoff() {
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
  async _requestWithHeaders(path, method, credentials, jsonPayload) {
    // We were asked to back off.
    if (this.backoffError) {
      log.debug("Received new request during backoff, re-rejecting.");
      throw this.backoffError;
    }
    let response;
    try {
      response = await this.hawk.request(
        path,
        method,
        credentials,
        jsonPayload
      );
    } catch (error) {
      log.error(`error ${method}ing ${path}`, error);
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
      throw error;
    }
    try {
      return { body: JSON.parse(response.body), headers: response.headers };
    } catch (error) {
      log.error("json parse error on response: " + response.body);
      // eslint-disable-next-line no-throw-literal
      throw { error };
    }
  },

  async _request(path, method, credentials, jsonPayload) {
    const response = await this._requestWithHeaders(
      path,
      method,
      credentials,
      jsonPayload
    );
    return response.body;
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
