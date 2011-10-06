/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Firefox Sync.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Philipp von Weitershausen <philipp@weitershausen.de>
 *   Richard Newman <rnewman@mozilla.com>
 *   Dan Mills <thunder@mozilla.com>
 *   Anant Narayanan <anant@kix.in>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://services-sync/log4moz.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/constants.js");

const EXPORTED_SYMBOLS = ["RESTRequest", "SyncStorageRequest"];

const STORAGE_REQUEST_TIMEOUT = 5 * 60; // 5 minutes

/**
 * Single use HTTP requests to RESTish resources.
 * 
 * @param uri
 *        URI for the request. This can be an nsIURI object or a string
 *        that can be used to create one. An exception will be thrown if
 *        the string is not a valid URI.
 *
 * Examples:
 *
 * (1) Quick GET request:
 *
 *   new RESTRequest("http://server/rest/resource").get(function (error) {
 *     if (error) {
 *       // Deal with a network error.
 *       processNetworkErrorCode(error.result);
 *       return;
 *     }
 *     if (!this.response.success) {

 *  *       // Bail out if we're not getting an HTTP 2xx code.
 *       processHTTPError(this.response.status);
 *       return;
 *     }
 *     processData(this.response.body);
 *   });
 *
 * (2) Quick PUT request (non-string data is automatically JSONified)
 *
 *   new RESTRequest("http://server/rest/resource").put(data, function (error) {
 *     ...
 *   });
 *
 * (3) Streaming GET
 *
 *   let request = new RESTRequest("http://server/rest/resource");
 *   request.setHeader("Accept", "application/newlines");
 *   request.onComplete = function (error) {
 *     if (error) {
 *       // Deal with a network error.
 *       processNetworkErrorCode(error.result);
 *       return;
 *     }
 *     callbackAfterRequestHasCompleted()
 *   });
 *   request.onProgress = function () {
 *     if (!this.response.success) {
 *       // Bail out if we're not getting an HTTP 2xx code.
 *       return;
 *     }
 *     // Process body data and reset it so we don't process the same data twice.
 *     processIncrementalData(this.response.body);
 *     this.response.body = "";
 *   });
 *   request.get();
 */
function RESTRequest(uri) {
  this.status = this.NOT_SENT;

  // If we don't have an nsIURI object yet, make one. This will throw if
  // 'uri' isn't a valid URI string.
  if (!(uri instanceof Ci.nsIURI)) {
    uri = Services.io.newURI(uri, null, null);
  }
  this.uri = uri;

  this._headers = {};
  this._log = Log4Moz.repository.getLogger(this._logName);
  this._log.level =
    Log4Moz.Level[Svc.Prefs.get("log.logger.network.resources")];
}
RESTRequest.prototype = {

  _logName: "Sync.RESTRequest",

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIBadCertListener2,
    Ci.nsIInterfaceRequestor
  ]),

  /*** Public API: ***/

  /**
   * URI for the request (an nsIURI object).
   */
  uri: null,

  /**
   * HTTP method (e.g. "GET")
   */
  method: null,

  /**
   * RESTResponse object
   */
  response: null,

  /**
   * nsIRequest load flags. Don't do any caching by default.
   */
  loadFlags: Ci.nsIRequest.LOAD_BYPASS_CACHE | Ci.nsIRequest.INHIBIT_CACHING,

  /**
   * nsIHttpChannel
   */
  channel: null,

  /**
   * Flag to indicate the status of the request.
   *
   * One of NOT_SENT, SENT, IN_PROGRESS, COMPLETED, ABORTED.
   */
  status: null,

  NOT_SENT:    0,
  SENT:        1,
  IN_PROGRESS: 2,
  COMPLETED:   4,
  ABORTED:     8,

  /**
   * Request timeout (in seconds, though decimal values can be used for
   * up to millisecond granularity.)
   *
   * 0 for no timeout.
   */
  timeout: null,

  /**
   * Called when the request has been completed, including failures and
   * timeouts.
   * 
   * @param error
   *        Error that occurred while making the request, null if there
   *        was no error.
   */
  onComplete: function onComplete(error) {
  },

  /**
   * Called whenever data is being received on the channel. If this throws an
   * exception, the request is aborted and the exception is passed as the
   * error to onComplete().
   */
  onProgress: function onProgress() {
  },

  /**
   * Set a request header.
   */
  setHeader: function setHeader(name, value) {
    this._headers[name.toLowerCase()] = value;
  },

  /**
   * Perform an HTTP GET.
   *
   * @param onComplete
   *        Short-circuit way to set the 'onComplete' method. Optional.
   * @param onProgress
   *        Short-circuit way to set the 'onProgress' method. Optional.
   *
   * @return the request object.
   */
  get: function get(onComplete, onProgress) {
    return this.dispatch("GET", null, onComplete, onProgress);
  },

  /**
   * Perform an HTTP PUT.
   *
   * @param data
   *        Data to be used as the request body. If this isn't a string
   *        it will be JSONified automatically.
   * @param onComplete
   *        Short-circuit way to set the 'onComplete' method. Optional.
   * @param onProgress
   *        Short-circuit way to set the 'onProgress' method. Optional.
   *
   * @return the request object.
   */
  put: function put(data, onComplete, onProgress) {
    return this.dispatch("PUT", data, onComplete, onProgress);
  },

  /**
   * Perform an HTTP POST.
   *
   * @param data
   *        Data to be used as the request body. If this isn't a string
   *        it will be JSONified automatically.
   * @param onComplete
   *        Short-circuit way to set the 'onComplete' method. Optional.
   * @param onProgress
   *        Short-circuit way to set the 'onProgress' method. Optional.
   *
   * @return the request object.
   */
  post: function post(data, onComplete, onProgress) {
    return this.dispatch("POST", data, onComplete, onProgress);
  },

  /**
   * Perform an HTTP DELETE.
   *
   * @param onComplete
   *        Short-circuit way to set the 'onComplete' method. Optional.
   * @param onProgress
   *        Short-circuit way to set the 'onProgress' method. Optional.
   *
   * @return the request object.
   */
  delete: function delete_(onComplete, onProgress) {
    return this.dispatch("DELETE", null, onComplete, onProgress);
  },

  /**
   * Abort an active request.
   */
  abort: function abort() {
    if (this.status != this.SENT && this.status != this.IN_PROGRESS) {
      throw "Can only abort a request that has been sent.";
    }

    this.status = this.ABORTED;
    this.channel.cancel(Cr.NS_BINDING_ABORTED);

    if (this.timeoutTimer) {
      // Clear the abort timer now that the channel is done.
      this.timeoutTimer.clear();
    }
  },

  /*** Implementation stuff ***/

  dispatch: function dispatch(method, data, onComplete, onProgress) {
    if (this.status != this.NOT_SENT) {
      throw "Request has already been sent!";
    }

    this.method = method;
    if (onComplete) {
      this.onComplete = onComplete;
    }
    if (onProgress) {
      this.onProgress = onProgress;
    }

    // Create and initialize HTTP channel.
    let channel = Services.io.newChannelFromURI(this.uri, null, null)
                          .QueryInterface(Ci.nsIRequest)
                          .QueryInterface(Ci.nsIHttpChannel);
    this.channel = channel;
    channel.loadFlags |= this.loadFlags;
    channel.notificationCallbacks = this;

    // Set request headers.
    let headers = this._headers;
    for (let key in headers) {
      if (key == 'authorization') {
        this._log.trace("HTTP Header " + key + ": ***** (suppressed)");
      } else {
        this._log.trace("HTTP Header " + key + ": " + headers[key]);
      }
      channel.setRequestHeader(key, headers[key], false);
    }

    // Set HTTP request body.
    if (method == "PUT" || method == "POST") {
      // Convert non-string bodies into JSON.
      if (typeof data != "string") {
        data = JSON.stringify(data);
      }

      this._log.debug(method + " Length: " + data.length);
      if (this._log.level <= Log4Moz.Level.Trace) {
        this._log.trace(method + " Body: " + data);
      }

      let stream = Cc["@mozilla.org/io/string-input-stream;1"]
                     .createInstance(Ci.nsIStringInputStream);
      stream.setData(data, data.length);

      let type = headers["content-type"] || "text/plain";
      channel.QueryInterface(Ci.nsIUploadChannel);
      channel.setUploadStream(stream, type, data.length);
    }
    // We must set this after setting the upload stream, otherwise it
    // will always be 'PUT'. Yeah, I know.
    channel.requestMethod = method;

    // Blast off!
    channel.asyncOpen(this, null);
    this.status = this.SENT;
    this.delayTimeout();
    return this;
  },

  /**
   * Create or push back the abort timer that kills this request.
   */
  delayTimeout: function delayTimeout() {
    if (this.timeout) {
      Utils.namedTimer(this.abortTimeout, this.timeout * 1000, this,
                       "timeoutTimer");
    }
  },

  /**
   * Abort the request based on a timeout.
   */
  abortTimeout: function abortTimeout() {
    this.abort();
    let error = Components.Exception("Aborting due to channel inactivity.",
                                     Cr.NS_ERROR_NET_TIMEOUT);
    this.onComplete(error);
  },

  /*** nsIStreamListener ***/

  onStartRequest: function onStartRequest(channel) {
    if (this.status == this.ABORTED) {
      this._log.trace("Not proceeding with onStartRequest, request was aborted.");
      return;
    }
    this.status = this.IN_PROGRESS;

    channel.QueryInterface(Ci.nsIHttpChannel);
    this._log.trace("onStartRequest: " + channel.requestMethod + " " +
                    channel.URI.spec);

    // Create a response object and fill it with some data.
    let response = this.response = new RESTResponse();
    response.request = this;
    response.body = "";

    // Define this here so that we don't have make a new one each time
    // onDataAvailable() gets called.
    this._inputStream = Cc["@mozilla.org/scriptableinputstream;1"]
                          .createInstance(Ci.nsIScriptableInputStream);

    this.delayTimeout();
  },

  onStopRequest: function onStopRequest(channel, context, statusCode) {
    if (this.timeoutTimer) {
      // Clear the abort timer now that the channel is done.
      this.timeoutTimer.clear();
    }

    // We don't want to do anything for a request that's already been aborted.
    if (this.status == this.ABORTED) {
      this._log.trace("Not proceeding with onStopRequest, request was aborted.");
      return;
    }
    this.status = this.COMPLETED;

    /**
     * Shim to help investigate Bug 672878.
     * We seem to be seeing a situation in which onStopRequest is being called
     * with a success code, but no mResponseHead yet set (a failure condition).
     * One possibility, according to bzbarsky, is that the channel status and
     * the status argument don't match up. This block will log that situation.
     *
     * Fetching channel.status should not throw, but if it does requestStatus
     * will be NS_ERROR_UNEXPECTED, which will cause this method to report
     * failure.
     *
     * This code parallels nearly identical shimming in resource.js.
     */
    let requestStatus = Cr.NS_ERROR_UNEXPECTED;
    let statusSuccess = Components.isSuccessCode(statusCode);
    try {
      // From nsIRequest.
      requestStatus = channel.status;
      this._log.trace("Request status is " + requestStatus);
    } catch (ex) {
      this._log.warn("Got exception " + Utils.exceptionStr(ex) +
                     " fetching channel.status.");
    }
    if (statusSuccess && (statusCode != requestStatus)) {
      this._log.error("Request status " + requestStatus +
                      " does not match status arg " + statusCode);
      try {
        channel.responseStatus;
      } catch (ex) {
        this._log.error("... and we got " + Utils.exceptionStr(ex) +
                        " retrieving responseStatus.");
      }
    }

    let requestStatusSuccess = Components.isSuccessCode(requestStatus);

    let uri = channel && channel.URI && channel.URI.spec || "<unknown>";
    this._log.trace("Channel for " + channel.requestMethod + " " + uri +
                    " returned status code " + statusCode);

    // Throw the failure code and stop execution.  Use Components.Exception()
    // instead of Error() so the exception is QI-able and can be passed across
    // XPCOM borders while preserving the status code.
    if (!statusSuccess || !requestStatusSuccess) {
      let message = Components.Exception("", statusCode).name;
      let error = Components.Exception(message, statusCode);
      this.onComplete(error);
      this.onComplete = this.onProgress = null;
      return;
    }

    this._log.debug(this.method + " " + uri + " " + this.response.status);

    // Additionally give the full response body when Trace logging.
    if (this._log.level <= Log4Moz.Level.Trace) {
      this._log.trace(this.method + " body: " + this.response.body);
    }

    delete this._inputStream;

    this.onComplete(null);
    this.onComplete = this.onProgress = null;
  },

  onDataAvailable: function onDataAvailable(req, cb, stream, off, count) {
    this._inputStream.init(stream);
    try {
      this.response.body += this._inputStream.read(count);
    } catch (ex) {
      this._log.warn("Exception thrown reading " + count +
                     " bytes from the channel.");
      this._log.debug(Utils.exceptionStr(ex));
      throw ex;
    }

    try {
      this.onProgress();
    } catch (ex) {
      this._log.warn("Got exception calling onProgress handler, aborting " +
                     this.method + " " + req.URI.spec);
      this._log.debug("Exception: " + Utils.exceptionStr(ex));
      this.abort();
      this.onComplete(ex);
      this.onComplete = this.onProgress = null;
      return;
    }

    this.delayTimeout();
  },

  /*** nsIInterfaceRequestor ***/

  getInterface: function(aIID) {
    return this.QueryInterface(aIID);
  },

  /*** nsIBadCertListener2 ***/

  notifyCertProblem: function notifyCertProblem(socketInfo, sslStatus, targetHost) {
    this._log.warn("Invalid HTTPS certificate encountered!");
    // Suppress invalid HTTPS certificate warnings in the UI.
    // (The request will still fail.)
    return true;
  }
};


/**
 * Response object for a RESTRequest. This will be created automatically by
 * the RESTRequest.
 */
function RESTResponse() {
  this._log = Log4Moz.repository.getLogger(this._logName);
  this._log.level =
    Log4Moz.Level[Svc.Prefs.get("log.logger.network.resources")];
}
RESTResponse.prototype = {

  _logName: "Sync.RESTResponse",

  /**
   * Corresponding REST request
   */
  request: null,

  /**
   * HTTP status code
   */
  get status() {
    let status;
    try {
      let channel = this.request.channel.QueryInterface(Ci.nsIHttpChannel);
      status = channel.responseStatus;
    } catch (ex) {
      this._log.debug("Caught exception fetching HTTP status code:" +
                      Utils.exceptionStr(ex));
      return null;
    }
    delete this.status;
    return this.status = status;
  },

  /**
   * Boolean flag that indicates whether the HTTP status code is 2xx or not.
   */
  get success() {
    let success;
    try {
      let channel = this.request.channel.QueryInterface(Ci.nsIHttpChannel);
      success = channel.requestSucceeded;
    } catch (ex) {
      this._log.debug("Caught exception fetching HTTP success flag:" +
                      Utils.exceptionStr(ex));
      return null;
    }
    delete this.success;
    return this.success = success;
  },

  /**
   * Object containing HTTP headers (keyed as lower case)
   */
  get headers() {
    let headers = {};
    try {
      this._log.trace("Processing response headers.");
      let channel = this.request.channel.QueryInterface(Ci.nsIHttpChannel);
      channel.visitResponseHeaders(function (header, value) {
        headers[header.toLowerCase()] = value;
      });
    } catch (ex) {
      this._log.debug("Caught exception processing response headers:" +
                      Utils.exceptionStr(ex));
      return null;
    }

    delete this.headers;
    return this.headers = headers;
  },

  /**
   * HTTP body (string)
   */
  body: null

};


/**
 * RESTRequest variant for use against a Sync storage server.
 */
function SyncStorageRequest(uri) {
  RESTRequest.call(this, uri);
}
SyncStorageRequest.prototype = {

  __proto__: RESTRequest.prototype,

  _logName: "Sync.StorageRequest",

  /**
   * The string to use as the base User-Agent in Sync requests.
   * These strings will look something like
   * 
   *   Firefox/4.0 FxSync/1.8.0.20100101.mobile
   * 
   * or
   * 
   *   Firefox Aurora/5.0a1 FxSync/1.9.0.20110409.desktop
   */
  userAgent:
    Services.appinfo.name + "/" + Services.appinfo.version +  // Product.
    " FxSync/" + WEAVE_VERSION + "." +                        // Sync.
    Services.appinfo.appBuildID + ".",                        // Build.

  /**
   * Wait 5 minutes before killing a request.
   */
  timeout: STORAGE_REQUEST_TIMEOUT,

  dispatch: function dispatch(method, data, onComplete, onProgress) {
    // Compose a UA string fragment from the various available identifiers.
    if (Svc.Prefs.get("sendVersionInfo", true)) {
      let ua = this.userAgent + Svc.Prefs.get("client.type", "desktop");
      this.setHeader("user-agent", ua);
    }

    // Set the BasicAuth header.
    let id = ID.get("WeaveID");
    if (id) {
      let auth_header = "Basic " + btoa(id.username + ':' + id.passwordUTF8);
      this.setHeader("authorization", auth_header);
    } else {
      this._log.debug("Couldn't set Authentication header: WeaveID not found.");
    }

    return RESTRequest.prototype.dispatch.apply(this, arguments);
  },

  onStartRequest: function onStartRequest(channel) {
    RESTRequest.prototype.onStartRequest.call(this, channel);
    if (this.status == this.ABORTED) {
      return;
    }

    let headers = this.response.headers;
    // Save the latest server timestamp when possible.
    if (headers["x-weave-timestamp"]) {
      SyncStorageRequest.serverTime = parseFloat(headers["x-weave-timestamp"]);
    }

    // This is a server-side safety valve to allow slowing down
    // clients without hurting performance.
    if (headers["x-weave-backoff"]) {
      Svc.Obs.notify("weave:service:backoff:interval",
                     parseInt(headers["x-weave-backoff"], 10));
    }

    if (this.response.success && headers["x-weave-quota-remaining"]) {
      Svc.Obs.notify("weave:service:quota:remaining",
                     parseInt(headers["x-weave-quota-remaining"], 10));
    }
  }
};
