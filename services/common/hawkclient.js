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

Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-crypto/utils.js");
Cu.import("resource://services-common/hawkrequest.js");
Cu.import("resource://services-common/observers.js");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// loglevel should be one of "Fatal", "Error", "Warn", "Info", "Config",
// "Debug", "Trace" or "All". If none is specified, "Error" will be used by
// default.
const PREF_LOG_LEVEL = "services.hawk.loglevel";

// A pref that can be set so "sensitive" information (eg, personally
// identifiable info, credentials, etc) will be logged.
const PREF_LOG_SENSITIVE_DETAILS = "services.hawk.log.sensitive";

XPCOMUtils.defineLazyGetter(this, "log", function() {
  let log = Log.repository.getLogger("Hawk");
  log.addAppender(new Log.DumpAppender());
  log.level = Log.Level.Error;
  try {
    let level =
      Services.prefs.getPrefType(PREF_LOG_LEVEL) == Ci.nsIPrefBranch.PREF_STRING
      && Services.prefs.getCharPref(PREF_LOG_LEVEL);
    log.level = Log.Level[level] || Log.Level.Error;
  } catch (e) {
    log.error(e);
  }

  return log;
});

// A boolean to indicate if personally identifiable information (or anything
// else sensitive, such as credentials) should be logged.
XPCOMUtils.defineLazyGetter(this, 'logPII', function() {
  try {
    return Services.prefs.getBoolPref(PREF_LOG_SENSITIVE_DETAILS);
  } catch (_) {
    return false;
  }
});

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
    let errorObj = {
      error: errorString,
      message: restResponse.statusText,
      code: restResponse.status,
      errno: restResponse.status
    };
    let retryAfter = restResponse.headers && restResponse.headers["retry-after"];
    retryAfter = retryAfter ? parseInt(retryAfter) : retryAfter;
    if (retryAfter) {
      errorObj.retryAfter = retryAfter;
      // and notify observers of the retry interval
      if (this.observerPrefix) {
        Observers.notify(this.observerPrefix + ":backoff:interval", retryAfter);
      }
    }
    return errorObj;
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

    function _onComplete(error) {
      let restResponse = this.response;
      let status = restResponse.status;

      log.debug("(Response) " + path + ": code: " + status +
                " - Status text: " + restResponse.statusText);
      if (logPII) {
        log.debug("Response text: " + restResponse.body);
      }

      // All responses may have backoff headers, which are a server-side safety
      // valve to allow slowing down clients without hurting performance.
      self._maybeNotifyBackoff(restResponse, "x-weave-backoff");
      self._maybeNotifyBackoff(restResponse, "x-backoff");

      if (error) {
        // When things really blow up, reconstruct an error object that follows
        // the general format of the server on error responses.
        return deferred.reject(self._constructError(restResponse, error));
      }

      self._updateClockOffset(restResponse.headers["date"]);

      if (status === 401 && retryOK && !("retry-after" in restResponse.headers)) {
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

    function onComplete(error) {
      try {
        // |this| is the RESTRequest object and we need to ensure _onComplete
        // gets the same one.
        _onComplete.call(this, error);
      } catch (ex) {
        log.error("Unhandled exception processing response:" +
                  CommonUtils.exceptionStr(ex));
        deferred.reject(ex);
      }
    }

    let extra = {
      now: this.now(),
      localtimeOffsetMsec: this.localtimeOffsetMsec,
    };

    let request = this.newHAWKAuthenticatedRESTRequest(uri, credentials, extra);
    if (method == "post" || method == "put") {
      request[method](payloadObj, onComplete);
    } else {
      request[method](onComplete);
    }

    return deferred.promise;
  },

  /*
   * The prefix used for all notifications sent by this module.  This
   * allows the handler of notifications to be sure they are handling
   * notifications for the service they expect.
   *
   * If not set, no notifications will be sent.
   */
  observerPrefix: null,

  // Given an optional header value, notify that a backoff has been requested.
  _maybeNotifyBackoff: function (response, headerName) {
    if (!this.observerPrefix || !response.headers) {
      return;
    }
    let headerVal = response.headers[headerName];
    if (!headerVal) {
      return;
    }
    let backoffInterval;
    try {
      backoffInterval = parseInt(headerVal, 10);
    } catch (ex) {
      log.error("hawkclient response had invalid backoff value in '" +
                headerName + "' header: " + headerVal);
      return;
    }
    Observers.notify(this.observerPrefix + ":backoff:interval", backoffInterval);
  },

  // override points for testing.
  newHAWKAuthenticatedRESTRequest: function(uri, credentials, extra) {
    return new HAWKAuthenticatedRESTRequest(uri, credentials, extra);
  },

}
