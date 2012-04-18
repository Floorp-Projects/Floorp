/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = [
  "TokenServerClient",
  "TokenServerClientError",
  "TokenServerClientNetworkError",
  "TokenServerClientServerError"
];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-common/preferences.js");
Cu.import("resource://services-common/rest.js");
Cu.import("resource://services-common/utils.js");

const Prefs = new Preferences("services.common.tokenserverclient.");

/**
 * Represents a TokenServerClient error that occurred on the client.
 *
 * This is the base type for all errors raised by client operations.
 *
 * @param message
 *        (string) Error message.
 */
function TokenServerClientError(message) {
  this.name = "TokenServerClientError";
  this.message = message || "Client error.";
}
TokenServerClientError.prototype = new Error();
TokenServerClientError.prototype.constructor = TokenServerClientError;

/**
 * Represents a TokenServerClient error that occurred in the network layer.
 *
 * @param error
 *        The underlying error thrown by the network layer.
 */
function TokenServerClientNetworkError(error) {
  this.name = "TokenServerClientNetworkError";
  this.error = error;
}
TokenServerClientNetworkError.prototype = new TokenServerClientError();
TokenServerClientNetworkError.prototype.constructor =
  TokenServerClientNetworkError;

/**
 * Represents a TokenServerClient error that occurred on the server.
 *
 * This type will be encountered for all non-200 response codes from the
 * server.
 *
 * @param message
 *        (string) Error message.
 */
function TokenServerClientServerError(message) {
  this.name = "TokenServerClientServerError";
  this.message = message || "Server error.";
}
TokenServerClientServerError.prototype = new TokenServerClientError();
TokenServerClientServerError.prototype.constructor =
  TokenServerClientServerError;

/**
 * Represents a client to the Token Server.
 *
 * http://docs.services.mozilla.com/token/index.html
 *
 * The Token Server supports obtaining tokens for arbitrary apps by
 * constructing URI paths of the form <app>/<app_version>. However, the service
 * discovery mechanism emphasizes the use of full URIs and tries to not force
 * the client to manipulate URIs. This client currently enforces this practice
 * by not implementing an API which would perform URI manipulation.
 *
 * If you are tempted to implement this API in the future, consider this your
 * warning that you may be doing it wrong and that you should store full URIs
 * instead.
 *
 * Areas to Improve:
 *
 *  - The server sends a JSON response on error. The client does not currently
 *    parse this. It might be convenient if it did.
 *  - Currently all non-200 status codes are rolled into one error type. It
 *    might be helpful if callers had a richer API that communicated who was
 *    at fault (e.g. differentiating a 503 from a 401).
 */
function TokenServerClient() {
  this._log = Log4Moz.repository.getLogger("Common.TokenServerClient");
  this._log.level = Log4Moz.Level[Prefs.get("logger.level")];
}
TokenServerClient.prototype = {
  /**
   * Logger instance.
   */
  _log: null,

  /**
   * Obtain a token from a BrowserID assertion against a specific URL.
   *
   * This asynchronously obtains the token. The callback receives 2 arguments.
   * The first signifies an error and is a TokenServerClientError (or derived)
   * type when an error occurs. If an HTTP response was seen, a RESTResponse
   * instance will be stored in the "response" property of this object.
   *
   * The second argument to the callback is a map containing the results from
   * the server. This map has the following keys:
   *
   *   id       (string) HTTP MAC public key identifier.
   *   key      (string) HTTP MAC shared symmetric key.
   *   endpoint (string) URL where service can be connected to.
   *   uid      (string) user ID for requested service.
   *
   * e.g.
   *
   *   let client = new TokenServerClient();
   *   let assertion = getBrowserIDAssertionFromSomewhere();
   *   let url = "https://token.services.mozilla.com/1.0/sync/2.0";
   *
   *   client.getTokenFromBrowserIDAssertion(url, assertion,
   *                                         function(error, result) {
   *     if (error) {
   *       // Do error handling.
   *       return;
   *     }
   *
   *     let {id: id, key: key, uid: uid, endpoint: endpoint} = result;
   *     // Do stuff with data and carry on.
   *   });
   *
   * @param  url
   *         (string) URL to fetch token from.
   * @param  assertion
   *         (string) BrowserID assertion to exchange token for.
   * @param  cb
   *         (function) Callback to be invoked with result of operation.
   */
  getTokenFromBrowserIDAssertion:
    function getTokenFromBrowserIDAssertion(url, assertion, cb) {
    if (!url) {
      throw new TokenServerClientError("url argument is not valid.");
    }

    if (!assertion) {
      throw new TokenServerClientError("assertion argument is not valid.");
    }

    if (!cb) {
      throw new TokenServerClientError("cb argument is not valid.");
    }

    this._log.debug("Beginning BID assertion exchange: " + url);

    let req = new RESTRequest(url);
    req.setHeader("accept", "application/json");
    req.setHeader("authorization", "Browser-ID " + assertion);
    let client = this;
    req.get(function onResponse(error) {
      if (error) {
        cb(new TokenServerClientNetworkError(error), null);
        return;
      }

      let self = this;
      function callCallback(error, result) {
        if (!cb) {
          self._log.warn("Callback already called! Did it throw?");
          return;
        }

        try {
          cb(error, result);
        } catch (ex) {
          self._log.warn("Exception when calling user-supplied callback: " +
                         CommonUtils.exceptionStr(ex));
        }

        cb = null;
      }

      try {
        client._processTokenResponse(this.response, callCallback);
      } catch (ex) {
        this._log.warn("Error processing token server response: " +
                       CommonUtils.exceptionStr(ex));

        let error = new TokenServerClientError(ex);
        error.response = this.response;
        callCallback(error, null);
      }
    });
  },

  /**
   * Handler to process token request responses.
   *
   * @param response
   *        RESTResponse from token HTTP request.
   * @param cb
   *        The original callback passed to the public API.
   */
  _processTokenResponse: function processTokenResponse(response, cb) {
    this._log.debug("Got token response.");

    if (!response.success) {
      this._log.info("Non-200 response code to token request: " +
                     response.status);
      this._log.debug("Response body: " + response.body);
      let error = new TokenServerClientServerError("Non 200 response code: " +
                                                   response.status);
      error.response = response;
      cb(error, null);
      return;
    }

    let ct = response.headers["content-type"];
    if (ct != "application/json" && ct.indexOf("application/json;") != 0) {
      let error =  new TokenServerClientError("Unsupported media type: " + ct);
      error.response = response;
      cb(error, null);
      return;
    }

    let result;
    try {
      result = JSON.parse(response.body);
    } catch (ex) {
      let error = new TokenServerClientServerError("Invalid JSON returned " +
                                                   "from server.");
      error.response = response;
      cb(error, null);
      return;
    }

    for each (let k in ["id", "key", "api_endpoint", "uid"]) {
      if (!(k in result)) {
        let error = new TokenServerClientServerError("Expected key not " +
                                                     " present in result: " +
                                                     k);
        error.response = response;
        cb(error, null);
        return;
      }
    }

    this._log.debug("Successful token response: " + result.id);
    cb(null, {
      id:       result.id,
      key:      result.key,
      endpoint: result.api_endpoint,
      uid:      result.uid,
    });
  }
};
