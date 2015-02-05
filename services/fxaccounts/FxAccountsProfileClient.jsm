/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A client to fetch profile information for a Firefox Account.
 */

this.EXPORTED_SYMBOLS = ["FxAccountsProfileClient", "FxAccountsProfileClientError"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://services-common/rest.js");

Cu.importGlobalProperties(["URL"]);

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
this.FxAccountsProfileClient = function(options) {
  if (!options || !options.serverURL || !options.token) {
    throw new Error("Missing 'serverURL' or 'token' configuration option");
  }

  try {
    this.serverURL = new URL(options.serverURL);
  } catch (e) {
    throw new Error("Invalid 'serverURL'");
  }
  this.token = options.token;
  log.debug("FxAccountsProfileClient: Initialized");
};

this.FxAccountsProfileClient.prototype = {
  /**
   * {nsIURI}
   * The server to fetch profile information from.
   */
  serverURL: null,

  /**
   * {String}
   * Profile server bearer OAuth token.
   */
  token: null,

  /**
   * Interface for making remote requests.
   */
  _Request: RESTRequest,

  /**
   * Remote request helper
   *
   * @param {String} path
   *        Profile server path, i.e "/profile".
   * @param {String} [method]
   *        Type of request, i.e "GET".
   * @return Promise
   *         Resolves: {Object} Successful response from the Profile server.
   *         Rejects: {FxAccountsProfileClientError} Profile client error.
   * @private
   */
  _createRequest: function(path, method = "GET") {
    return new Promise((resolve, reject) => {
      let profileDataUrl = this.serverURL + path;
      let request = new this._Request(profileDataUrl);
      method = method.toUpperCase();

      request.setHeader("Authorization", "Bearer " + this.token);
      request.setHeader("Accept", "application/json");

      request.onComplete = function (error) {
        if (error) {
          return reject(new FxAccountsProfileClientError({
            error: ERROR_NETWORK,
            errno: ERRNO_NETWORK,
            message: error.toString(),
          }));
        }

        let body = null;
        try {
          body = JSON.parse(request.response.body);
        } catch (e) {
          return reject(new FxAccountsProfileClientError({
            error: ERROR_PARSE,
            errno: ERRNO_PARSE,
            code: request.response.status,
            message: request.response.body,
          }));
        }

        // "response.success" means status code is 200
        if (request.response.success) {
          return resolve(body);
        } else {
          return reject(new FxAccountsProfileClientError(body));
        }
      };

      if (method === "GET") {
        request.get();
      } else {
        // method not supported
        return reject(new FxAccountsProfileClientError({
          error: ERROR_NETWORK,
          errno: ERRNO_NETWORK,
          code: ERROR_CODE_METHOD_NOT_ALLOWED,
          message: ERROR_MSG_METHOD_NOT_ALLOWED,
        }));
      }
    });
  },

  /**
   * Retrieve user's profile from the server
   *
   * @return Promise
   *         Resolves: {Object} Successful response from the '/profile' endpoint.
   *         Rejects: {FxAccountsProfileClientError} profile client error.
   */
  fetchProfile: function () {
    log.debug("FxAccountsProfileClient: Requested profile");
    return this._createRequest("/profile", "GET");
  },

  /**
   * Retrieve user's profile from the server
   *
   * @return Promise
   *         Resolves: {Object} Successful response from the '/avatar' endpoint.
   *         Rejects: {FxAccountsProfileClientError} profile client error.
   */
  fetchProfileImage: function () {
    log.debug("FxAccountsProfileClient: Requested avatar");
    return this._createRequest("/avatar", "GET");
  }
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
this.FxAccountsProfileClientError = function(details) {
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
