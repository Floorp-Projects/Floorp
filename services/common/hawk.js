/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * HAWK is an HTTP authentication scheme using a message authentication code
 * (MAC) algorithm to provide partial HTTP request cryptographic verification.
 *
 * For details, see: https://github.com/hueniverse/hawk
 *
 * With HAWK, it is essential that the clocks on clients and server not have an
 * absolute delta of greater than one minute, as the HAWK protocol uses
 * timestamps to reduce the possibility of replay attacks.  However, it is
 * likely that some clients' clocks will be more than a little off, especially
 * in mobile devices, which would break HAWK-based services (like sync and
 * firefox accounts) for those clients.
 *
 * This library provides a stateful HAWK client that calculates (roughly) the
 * clock delta on the client vs the server.  The library provides an interface
 * for deriving HAWK credentials and making HAWK-authenticated REST requests to
 * a single remote server.  Therefore, callers who want to interact with
 * multiple HAWK services should instantiate one HawkClient per service.
 */

this.EXPORTED_SYMBOLS = ["HawkClient"];

const {interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-crypto/utils.js");
Cu.import("resource://services-common/rest.js");
Cu.import("resource://gre/modules/Promise.jsm");

/*
 * A general purpose client for making HAWK authenticated requests to a single
 * host.  Keeps track of the clock offset between the client and the host for
 * computation of the timestamp in the HAWK Authorization header.
 *
 * Clients should create one HawkClient object per each server they wish to
 * interact with.
 *
 * @param host
 *        The url of the host
 */
this.HawkClient = function(host) {
  this.host = host;

  // Clock offset in milliseconds between our client's clock and the date
  // reported in responses from our host.
  this._localtimeOffsetMsec = 0;
}

this.HawkClient.prototype = {

  /*
   * Construct an error message for a response.  Private.
   *
   * @param restResponse
   *        A RESTResponse object from a RESTRequest
   *
   * @param errorString
   *        A string describing the error
   */
  _constructError: function(restResponse, errorString) {
    return {
      error: errorString,
      message: restResponse.statusText,
      code: restResponse.status,
      errno: restResponse.status
    };
  },

  /*
   *
   * Update clock offset by determining difference from date gives in the (RFC
   * 1123) Date header of a server response.  Because HAWK tolerates a window
   * of one minute of clock skew (so two minutes total since the skew can be
   * positive or negative), the simple method of calculating offset here is
   * probably good enough.  We keep the value in milliseconds to make life
   * easier, even though the value will not have millisecond accuracy.
   *
   * @param dateString
   *        An RFC 1123 date string (e.g., "Mon, 13 Jan 2014 21:45:06 GMT")
   *
   * For HAWK clock skew and replay protection, see
   * https://github.com/hueniverse/hawk#replay-protection
   */
  _updateClockOffset: function(dateString) {
    try {
      let serverDateMsec = Date.parse(dateString);
      this._localtimeOffsetMsec = serverDateMsec - this.now();
      log.debug("Clock offset vs " + this.host + ": " + this._localtimeOffsetMsec);
    } catch(err) {
      log.warn("Bad date header in server response: " + dateString);
    }
  },

  /*
   * Get the current clock offset in milliseconds.
   *
   * The offset is the number of milliseconds that must be added to the client
   * clock to make it equal to the server clock.  For example, if the client is
   * five minutes ahead of the server, the localtimeOffsetMsec will be -300000.
   */
  get localtimeOffsetMsec() {
    return this._localtimeOffsetMsec;
  },

  /*
   * return current time in milliseconds
   */
  now: function() {
    return Date.now();
  },

  /* A general method for sending raw RESTRequest calls authorized using HAWK
   *
   * @param path
   *        API endpoint path
   * @param method
   *        The HTTP request method
   * @param credentials
   *        Hawk credentials
   * @param payloadObj
   *        An object that can be encodable as JSON as the payload of the
   *        request
   * @return Promise
   *        Returns a promise that resolves to the text response of the API call,
   *        or is rejected with an error.  If the server response can be parsed
   *        as JSON and contains an 'error' property, the promise will be
   *        rejected with this JSON-parsed response.
   */
  request: function(path, method, credentials=null, payloadObj={}, retryOK=true) {
    method = method.toLowerCase();

    let deferred = Promise.defer();
    let uri = this.host + path;
    let self = this;

    function onComplete(error) {
      let restResponse = this.response;
      let status = restResponse.status;

      log.debug("(Response) code: " + status +
                " - Status text: " + restResponse.statusText,
                " - Response text: " + restResponse.body);

      if (error) {
        // When things really blow up, reconstruct an error object that follows
        // the general format of the server on error responses.
        return deferred.reject(self._constructError(restResponse, error));
      }

      self._updateClockOffset(restResponse.headers["date"]);

      if (status === 401 && retryOK) {
        // Retry once if we were rejected due to a bad timestamp.
        // Clock offset is adjusted already in the top of this function.
        log.debug("Received 401 for " + path + ": retrying");
        return deferred.resolve(
            self.request(path, method, credentials, payloadObj, false));
      }

      // If the server returned a json error message, use it in the rejection
      // of the promise.
      //
      // In the case of a 401, in which we are probably being rejected for a
      // bad timestamp, retry exactly once, during which time clock offset will
      // be adjusted.

      let jsonResponse = {};
      try {
        jsonResponse = JSON.parse(restResponse.body);
      } catch(notJSON) {}

      let okResponse = (200 <= status && status < 300);
      if (!okResponse || jsonResponse.error) {
        if (jsonResponse.error) {
          return deferred.reject(jsonResponse);
        }
        return deferred.reject(self._constructError(restResponse, "Request failed"));
      }
      // It's up to the caller to know how to decode the response.
      // We just return the raw text.
      deferred.resolve(this.response.body);
    };

    let extra = {
      now: this.now(),
      localtimeOffsetMsec: this.localtimeOffsetMsec,
    };

    let request = new HAWKAuthenticatedRESTRequest(uri, credentials, extra);
    if (method == "post" || method == "put") {
      request[method](payloadObj, onComplete);
    } else {
      request[method](onComplete);
    }

    return deferred.promise;
  }
}
