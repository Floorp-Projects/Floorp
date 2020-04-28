/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Firefox Accounts OAuth Grant Client allows clients to obtain
 * an OAuth token from a BrowserID assertion. Only certain client
 * IDs support this privilege.
 */

var EXPORTED_SYMBOLS = [
  "FxAccountsOAuthGrantClient",
  "FxAccountsOAuthGrantClientError",
];

const {
  ERRNO_NETWORK,
  ERRNO_PARSE,
  ERRNO_UNKNOWN_ERROR,
  ERROR_CODE_METHOD_NOT_ALLOWED,
  ERROR_MSG_METHOD_NOT_ALLOWED,
  ERROR_NETWORK,
  ERROR_PARSE,
  ERROR_UNKNOWN,
  OAUTH_SERVER_ERRNO_OFFSET,
  log,
} = ChromeUtils.import("resource://gre/modules/FxAccountsCommon.js");
const { RESTRequest } = ChromeUtils.import(
  "resource://services-common/rest.js"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

const AUTH_ENDPOINT = "/authorization";
const HOST_PREF = "identity.fxaccounts.remote.oauth.uri";

// This is the same pref that's used by FxAccounts.jsm.
const ALLOW_HTTP_PREF = "identity.fxaccounts.allowHttp";

/**
 * Create a new FxAccountsOAuthClient for the browser.
 *
 * @param {Object} options Options
 *   @param {String} options.client_id
 *   OAuth client id under which this client is registered
 *   @param {String} options.parameters.serverURL
 *   The FxA OAuth server URL
 * @constructor
 */
var FxAccountsOAuthGrantClient = function(options = {}) {
  this.parameters = { ...options };
  if (!this.parameters.serverURL) {
    this.parameters.serverURL = Services.prefs.getCharPref(HOST_PREF);
  }
  this._validateOptions(this.parameters);

  try {
    this.serverURL = new URL(this.parameters.serverURL);
  } catch (e) {
    throw new Error("Invalid 'serverURL'");
  }

  let forceHTTPS = !Services.prefs.getBoolPref(ALLOW_HTTP_PREF, false);
  if (forceHTTPS && this.serverURL.protocol != "https:") {
    throw new Error("'serverURL' must be HTTPS");
  }

  log.debug("FxAccountsOAuthGrantClient Initialized");
};

FxAccountsOAuthGrantClient.prototype = {
  /**
   * Retrieves an OAuth access token for the signed in user
   *
   * @param {Object} assertion BrowserID assertion
   * @param {String} scope OAuth scope
   * @param {Number} ttl token time to live
   * @return Promise
   *        Resolves: {Object} Object with access_token property
   */
  getTokenFromAssertion(assertion, scope, ttl) {
    if (!assertion) {
      throw new Error("Missing 'assertion' parameter");
    }
    if (!scope) {
      throw new Error("Missing 'scope' parameter");
    }
    let params = {
      scope,
      client_id: this.parameters.client_id,
      assertion,
      response_type: "token",
      ttl,
    };

    return this._createRequest(AUTH_ENDPOINT, "POST", params);
  },

  /**
   * Validates the required FxA OAuth parameters
   *
   * @param options {Object}
   *        OAuth client options
   * @private
   */
  _validateOptions(options) {
    if (!options) {
      throw new Error("Missing configuration options");
    }

    ["serverURL", "client_id"].forEach(option => {
      if (!options[option]) {
        throw new Error("Missing '" + option + "' parameter");
      }
    });
  },

  /**
   * Interface for making remote requests.
   */
  _Request: RESTRequest,

  /**
   * Remote request helper
   *
   * @param {String} path
   *        OAuth server path, i.e "/token".
   * @param {String} [method]
   *        Type of request, i.e "GET".
   * @return Promise
   *         Resolves: {Object} Successful response from the Oauth server.
   *         Rejects: {FxAccountsOAuthGrantClientError} OAuth client error.
   * @private
   */
  async _createRequest(path, method = "POST", params) {
    let requestUrl = this.serverURL + path;
    let request = new this._Request(requestUrl);
    method = method.toUpperCase();

    request.setHeader("Accept", "application/json");
    request.setHeader("Content-Type", "application/json");

    if (method != "POST") {
      throw new FxAccountsOAuthGrantClientError({
        error: ERROR_NETWORK,
        errno: ERRNO_NETWORK,
        code: ERROR_CODE_METHOD_NOT_ALLOWED,
        message: ERROR_MSG_METHOD_NOT_ALLOWED,
      });
    }

    try {
      await request.post(params);
    } catch (error) {
      throw new FxAccountsOAuthGrantClientError({
        error: ERROR_NETWORK,
        errno: ERRNO_NETWORK,
        message: error.toString(),
      });
    }

    let body = null;
    try {
      body = JSON.parse(request.response.body);
    } catch (e) {
      throw new FxAccountsOAuthGrantClientError({
        error: ERROR_PARSE,
        errno: ERRNO_PARSE,
        code: request.response.status,
        message: request.response.body,
      });
    }

    if (request.response.success) {
      return body;
    }

    if (typeof body.errno === "number") {
      // Offset oauth server errnos to avoid conflict with other FxA server errnos
      body.errno += OAUTH_SERVER_ERRNO_OFFSET;
    } else if (body.errno) {
      body.errno = ERRNO_UNKNOWN_ERROR;
    }
    throw new FxAccountsOAuthGrantClientError(body);
  },
};

/**
 * Normalized oauth client errors
 * @param {Object} [details]
 *        Error details object
 *   @param {number} [details.code]
 *          Error code
 *   @param {number} [details.errno]
 *          Error number
 *   @param {String} [details.error]
 *          Error description
 *   @param {String|null} [details.message]
 *          Error message
 * @constructor
 */
var FxAccountsOAuthGrantClientError = function(details) {
  details = details || {};

  this.name = "FxAccountsOAuthGrantClientError";
  this.code = details.code || null;
  this.errno = details.errno || ERRNO_UNKNOWN_ERROR;
  this.error = details.error || ERROR_UNKNOWN;
  this.message = details.message || null;
};

/**
 * Returns error object properties
 *
 * @returns {{name: *, code: *, errno: *, error: *, message: *}}
 * @private
 */
FxAccountsOAuthGrantClientError.prototype._toStringFields = function() {
  return {
    name: this.name,
    code: this.code,
    errno: this.errno,
    error: this.error,
    message: this.message,
  };
};

/**
 * String representation of a oauth grant client error
 *
 * @returns {String}
 */
FxAccountsOAuthGrantClientError.prototype.toString = function() {
  return this.name + "(" + JSON.stringify(this._toStringFields()) + ")";
};
