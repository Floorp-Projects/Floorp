/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A client to fetch profile information for a Firefox Account.
 */
"use strict;";

var EXPORTED_SYMBOLS = [
  "FxAccountsProfileClient",
  "FxAccountsProfileClientError",
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
  log,
  SCOPE_PROFILE,
  SCOPE_PROFILE_WRITE,
} = ChromeUtils.import("resource://gre/modules/FxAccountsCommon.js");
const { fxAccounts } = ChromeUtils.import(
  "resource://gre/modules/FxAccounts.jsm"
);
const { RESTRequest } = ChromeUtils.import(
  "resource://services-common/rest.js"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

/**
 * Create a new FxAccountsProfileClient to be able to fetch Firefox Account profile information.
 *
 * @param {Object} options Options
 *   @param {String} options.serverURL
 *   The URL of the profile server to query.
 *   Example: https://profile.accounts.firefox.com/v1
 *   @param {String} options.token
 *   The bearer token to access the profile server
 * @constructor
 */
var FxAccountsProfileClient = function(options) {
  if (!options || !options.serverURL) {
    throw new Error("Missing 'serverURL' configuration option");
  }

  this.fxai = options.fxai || fxAccounts._internal;

  try {
    this.serverURL = new URL(options.serverURL);
  } catch (e) {
    throw new Error("Invalid 'serverURL'");
  }
  log.debug("FxAccountsProfileClient: Initialized");
};

FxAccountsProfileClient.prototype = {
  /**
   * {nsIURI}
   * The server to fetch profile information from.
   */
  serverURL: null,

  /**
   * Interface for making remote requests.
   */
  _Request: RESTRequest,

  /**
   * Remote request helper which abstracts authentication away.
   *
   * @param {String} path
   *        Profile server path, i.e "/profile".
   * @param {String} [method]
   *        Type of request, e.g. "GET".
   * @param {String} [etag]
   *        Optional ETag used for caching purposes.
   * @param {Object} [body]
   *        Optional request body, to be sent as application/json.
   * @return Promise
   *         Resolves: {body: Object, etag: Object} Successful response from the Profile server.
   *         Rejects: {FxAccountsProfileClientError} Profile client error.
   * @private
   */
  async _createRequest(path, method = "GET", etag = null, body = null) {
    method = method.toUpperCase();
    let token = await this._getTokenForRequest(method);
    try {
      return await this._rawRequest(path, method, token, etag, body);
    } catch (ex) {
      if (!(ex instanceof FxAccountsProfileClientError) || ex.code != 401) {
        throw ex;
      }
      // it's an auth error - assume our token expired and retry.
      log.info(
        "Fetching the profile returned a 401 - revoking our token and retrying"
      );
      await this.fxai.removeCachedOAuthToken({ token });
      token = await this._getTokenForRequest(method);
      // and try with the new token - if that also fails then we fail after
      // revoking the token.
      try {
        return await this._rawRequest(path, method, token, etag, body);
      } catch (ex) {
        if (!(ex instanceof FxAccountsProfileClientError) || ex.code != 401) {
          throw ex;
        }
        log.info(
          "Retry fetching the profile still returned a 401 - revoking our token and failing"
        );
        await this.fxai.removeCachedOAuthToken({ token });
        throw ex;
      }
    }
  },

  /**
   * Helper to get an OAuth token for a request.
   *
   * OAuth tokens are cached, so it's fine to call this for each request.
   *
   * @param {String} [method]
   *        Type of request, i.e "GET".
   * @return Promise
   *         Resolves: Object containing "scope", "token" and "key" properties
   *         Rejects: {FxAccountsProfileClientError} Profile client error.
   * @private
   */
  async _getTokenForRequest(method) {
    let scope = SCOPE_PROFILE;
    if (method === "POST") {
      scope = SCOPE_PROFILE_WRITE;
    }
    return this.fxai.getOAuthToken({ scope });
  },

  /**
   * Remote "raw" request helper - doesn't handle auth errors and tokens.
   *
   * @param {String} path
   *        Profile server path, i.e "/profile".
   * @param {String} method
   *        Type of request, i.e "GET".
   * @param {String} token
   * @param {String} etag
   * @param {Object} payload
   *        The payload of the request, if any.
   * @return Promise
   *         Resolves: {body: Object, etag: Object} Successful response from the Profile server
                        or null if 304 is hit (same ETag).
   *         Rejects: {FxAccountsProfileClientError} Profile client error.
   * @private
   */
  async _rawRequest(path, method, token, etag = null, payload = null) {
    let profileDataUrl = this.serverURL + path;
    let request = new this._Request(profileDataUrl);

    request.setHeader("Authorization", "Bearer " + token);
    request.setHeader("Accept", "application/json");
    if (etag) {
      request.setHeader("If-None-Match", etag);
    }

    if (method != "GET" && method != "POST") {
      // method not supported
      throw new FxAccountsProfileClientError({
        error: ERROR_NETWORK,
        errno: ERRNO_NETWORK,
        code: ERROR_CODE_METHOD_NOT_ALLOWED,
        message: ERROR_MSG_METHOD_NOT_ALLOWED,
      });
    }
    try {
      await request.dispatch(method, payload);
    } catch (error) {
      throw new FxAccountsProfileClientError({
        error: ERROR_NETWORK,
        errno: ERRNO_NETWORK,
        message: error.toString(),
      });
    }

    let body = null;
    try {
      if (request.response.status == 304) {
        return null;
      }
      body = JSON.parse(request.response.body);
    } catch (e) {
      throw new FxAccountsProfileClientError({
        error: ERROR_PARSE,
        errno: ERRNO_PARSE,
        code: request.response.status,
        message: request.response.body,
      });
    }

    // "response.success" means status code is 200
    if (!request.response.success) {
      throw new FxAccountsProfileClientError({
        error: body.error || ERROR_UNKNOWN,
        errno: body.errno || ERRNO_UNKNOWN_ERROR,
        code: request.response.status,
        message: body.message || body,
      });
    }
    return {
      body,
      etag: request.response.headers.etag,
    };
  },

  /**
   * Retrieve user's profile from the server
   *
   * @param {String} [etag]
   *        Optional ETag used for caching purposes. (may generate a 304 exception)
   * @return Promise
   *         Resolves: {body: Object, etag: Object} Successful response from the '/profile' endpoint.
   *         Rejects: {FxAccountsProfileClientError} profile client error.
   */
  fetchProfile(etag) {
    log.debug("FxAccountsProfileClient: Requested profile");
    return this._createRequest("/profile", "GET", etag);
  },
};

/**
 * Normalized profile client errors
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
var FxAccountsProfileClientError = function(details) {
  details = details || {};

  this.name = "FxAccountsProfileClientError";
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
FxAccountsProfileClientError.prototype._toStringFields = function() {
  return {
    name: this.name,
    code: this.code,
    errno: this.errno,
    error: this.error,
    message: this.message,
  };
};

/**
 * String representation of a profile client error
 *
 * @returns {String}
 */
FxAccountsProfileClientError.prototype.toString = function() {
  return this.name + "(" + JSON.stringify(this._toStringFields()) + ")";
};
