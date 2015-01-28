/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MERGED_COMPARTMENT

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

this.EXPORTED_SYMBOLS = [
  "RESTRequest",
  "RESTResponse",
  "TokenAuthenticatedRESTRequest",
];

#endif

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-common/utils.js");

XPCOMUtils.defineLazyModuleGetter(this, "CryptoUtils",
                                  "resource://services-crypto/utils.js");

const Prefs = new Preferences("services.common.rest.");

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
 *       // Bail out if we're not getting an HTTP 2xx code.
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
this.RESTRequest = function RESTRequest(uri) {
  this.status = this.NOT_SENT;

  // If we don't have an nsIURI object yet, make one. This will throw if
  // 'uri' isn't a valid URI string.
  if (!(uri instanceof Ci.nsIURI)) {
    uri = Services.io.newURI(uri, null, null);
  }
  this.uri = uri;

  this._headers = {};
  this._log = Log.repository.getLogger(this._logName);
  this._log.level =
    Log.Level[Prefs.get("log.logger.rest.request")];
}
RESTRequest.prototype = {

  _logName: "Services.Common.RESTRequest",

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIBadCertListener2,
    Ci.nsIInterfaceRequestor,
    Ci.nsIChannelEventSink
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
   * nsIRequest load flags. Don't do any caching by default. Don't send user
   * cookies and such over the wire (Bug 644734).
   */
  loadFlags: Ci.nsIRequest.LOAD_BYPASS_CACHE | Ci.nsIRequest.INHIBIT_CACHING | Ci.nsIRequest.LOAD_ANONYMOUS,

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
   * HTTP status text of response
   */
  statusText: null,

  /**
   * Request timeout (in seconds, though decimal values can be used for
   * up to millisecond granularity.)
   *
   * 0 for no timeout.
   */
  timeout: null,

  /**
   * The encoding with which the response to this request must be treated.
   * If a charset parameter is available in the HTTP Content-Type header for
   * this response, that will always be used, and this value is ignored. We
   * default to UTF-8 because that is a reasonable default.
   */
  charset: "utf-8",

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
   * Perform an HTTP PATCH.
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
  patch: function patch(data, onComplete, onProgress) {
    return this.dispatch("PATCH", data, onComplete, onProgress);
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
    let channel = Services.io.newChannelFromURI2(this.uri,
                                                 null,      // aLoadingNode
                                                 Services.scriptSecurityManager.getSystemPrincipal(),
                                                 null,      // aTriggeringPrincipal
                                                 Ci.nsILoadInfo.SEC_NORMAL,
                                                 Ci.nsIContentPolicy.TYPE_OTHER)
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
    if (method == "PUT" || method == "POST" || method == "PATCH") {
      // Convert non-string bodies into JSON.
      if (typeof data != "string") {
        data = JSON.stringify(data);
      }

      this._log.debug(method + " Length: " + data.length);
      if (this._log.level <= Log.Level.Trace) {
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

    // Before opening the channel, set the charset that serves as a hint
    // as to what the response might be encoded as.
    channel.contentCharset = this.charset;

    // Blast off!
    try {
      channel.asyncOpen(this, null);
    } catch (ex) {
      // asyncOpen can throw in a bunch of cases -- e.g., a forbidden port.
      this._log.warn("Caught an error in asyncOpen: " + CommonUtils.exceptionStr(ex));
      CommonUtils.nextTick(onComplete.bind(this, ex));
    }
    this.status = this.SENT;
    this.delayTimeout();
    return this;
  },

  /**
   * Create or push back the abort timer that kills this request.
   */
  delayTimeout: function delayTimeout() {
    if (this.timeout) {
      CommonUtils.namedTimer(this.abortTimeout, this.timeout * 1000, this,
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
    if (!this.onComplete) {
      this._log.error("Unexpected error: onComplete not defined in " +
                      "abortTimeout.");
      return;
    }
    this.onComplete(error);
  },

  /*** nsIStreamListener ***/

  onStartRequest: function onStartRequest(channel) {
    if (this.status == this.ABORTED) {
      this._log.trace("Not proceeding with onStartRequest, request was aborted.");
      return;
    }

    try {
      channel.QueryInterface(Ci.nsIHttpChannel);
    } catch (ex) {
      this._log.error("Unexpected error: channel is not a nsIHttpChannel!");
      this.status = this.ABORTED;
      channel.cancel(Cr.NS_BINDING_ABORTED);
      return;
    }

    this.status = this.IN_PROGRESS;

    this._log.trace("onStartRequest: " + channel.requestMethod + " " +
                    channel.URI.spec);

    // Create a response object and fill it with some data.
    let response = this.response = new RESTResponse();
    response.request = this;
    response.body = "";

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

    try {
      channel.QueryInterface(Ci.nsIHttpChannel);
    } catch (ex) {
      this._log.error("Unexpected error: channel not nsIHttpChannel!");
      this.status = this.ABORTED;
      return;
    }
    this.status = this.COMPLETED;

    let statusSuccess = Components.isSuccessCode(statusCode);
    let uri = channel && channel.URI && channel.URI.spec || "<unknown>";
    this._log.trace("Channel for " + channel.requestMethod + " " + uri +
                    " returned status code " + statusCode);

    if (!this.onComplete) {
      this._log.error("Unexpected error: onComplete not defined in " +
                      "abortRequest.");
      this.onProgress = null;
      return;
    }

    // Throw the failure code and stop execution.  Use Components.Exception()
    // instead of Error() so the exception is QI-able and can be passed across
    // XPCOM borders while preserving the status code.
    if (!statusSuccess) {
      let message = Components.Exception("", statusCode).name;
      let error = Components.Exception(message, statusCode);
      this._log.debug(this.method + " " + uri + " failed: " + statusCode + " - " + message);
      this.onComplete(error);
      this.onComplete = this.onProgress = null;
      return;
    }

    this._log.debug(this.method + " " + uri + " " + this.response.status);

    // Additionally give the full response body when Trace logging.
    if (this._log.level <= Log.Level.Trace) {
      this._log.trace(this.method + " body: " + this.response.body);
    }

    delete this._inputStream;

    this.onComplete(null);
    this.onComplete = this.onProgress = null;
  },

  onDataAvailable: function onDataAvailable(channel, cb, stream, off, count) {
    // We get an nsIRequest, which doesn't have contentCharset.
    try {
      channel.QueryInterface(Ci.nsIHttpChannel);
    } catch (ex) {
      this._log.error("Unexpected error: channel not nsIHttpChannel!");
      this.abort();

      if (this.onComplete) {
        this.onComplete(ex);
      }

      this.onComplete = this.onProgress = null;
      return;
    }

    if (channel.contentCharset) {
      this.response.charset = channel.contentCharset;

      if (!this._converterStream) {
        this._converterStream = Cc["@mozilla.org/intl/converter-input-stream;1"]
                                   .createInstance(Ci.nsIConverterInputStream);
      }

      this._converterStream.init(stream, channel.contentCharset, 0,
                                 this._converterStream.DEFAULT_REPLACEMENT_CHARACTER);

      try {
        let str = {};
        let num = this._converterStream.readString(count, str);
        if (num != 0) {
          this.response.body += str.value;
        }
      } catch (ex) {
        this._log.warn("Exception thrown reading " + count + " bytes from " +
                       "the channel.");
        this._log.warn(CommonUtils.exceptionStr(ex));
        throw ex;
      }
    } else {
      this.response.charset = null;

      if (!this._inputStream) {
        this._inputStream = Cc["@mozilla.org/scriptableinputstream;1"]
                              .createInstance(Ci.nsIScriptableInputStream);
      }

      this._inputStream.init(stream);

      this.response.body += this._inputStream.read(count);
    }

    try {
      this.onProgress();
    } catch (ex) {
      this._log.warn("Got exception calling onProgress handler, aborting " +
                     this.method + " " + channel.URI.spec);
      this._log.debug("Exception: " + CommonUtils.exceptionStr(ex));
      this.abort();

      if (!this.onComplete) {
        this._log.error("Unexpected error: onComplete not defined in " +
                        "onDataAvailable.");
        this.onProgress = null;
        return;
      }

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
  },

  /**
   * Returns true if headers from the old channel should be
   * copied to the new channel. Invoked when a channel redirect
   * is in progress.
   */
  shouldCopyOnRedirect: function shouldCopyOnRedirect(oldChannel, newChannel, flags) {
    let isInternal = !!(flags & Ci.nsIChannelEventSink.REDIRECT_INTERNAL);
    let isSameURI  = newChannel.URI.equals(oldChannel.URI);
    this._log.debug("Channel redirect: " + oldChannel.URI.spec + ", " +
                    newChannel.URI.spec + ", internal = " + isInternal);
    return isInternal && isSameURI;
  },

  /*** nsIChannelEventSink ***/
  asyncOnChannelRedirect:
    function asyncOnChannelRedirect(oldChannel, newChannel, flags, callback) {

    try {
      newChannel.QueryInterface(Ci.nsIHttpChannel);
    } catch (ex) {
      this._log.error("Unexpected error: channel not nsIHttpChannel!");
      callback.onRedirectVerifyCallback(Cr.NS_ERROR_NO_INTERFACE);
      return;
    }

    // For internal redirects, copy the headers that our caller set.
    try {
      if (this.shouldCopyOnRedirect(oldChannel, newChannel, flags)) {
        this._log.trace("Copying headers for safe internal redirect.");
        for (let key in this._headers) {
          newChannel.setRequestHeader(key, this._headers[key], false);
        }
      }
    } catch (ex) {
      this._log.error("Error copying headers: " + CommonUtils.exceptionStr(ex));
    }

    this.channel = newChannel;

    // We let all redirects proceed.
    callback.onRedirectVerifyCallback(Cr.NS_OK);
  }
};

/**
 * Response object for a RESTRequest. This will be created automatically by
 * the RESTRequest.
 */
this.RESTResponse = function RESTResponse() {
  this._log = Log.repository.getLogger(this._logName);
  this._log.level =
    Log.Level[Prefs.get("log.logger.rest.response")];
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
      status = this.request.channel.responseStatus;
    } catch (ex) {
      this._log.debug("Caught exception fetching HTTP status code:" +
                      CommonUtils.exceptionStr(ex));
      return null;
    }
    Object.defineProperty(this, "status", {value: status});
    return status;
  },

  /**
   * HTTP status text
   */
  get statusText() {
    let statusText;
    try {
      statusText = this.request.channel.responseStatusText;
    } catch (ex) {
      this._log.debug("Caught exception fetching HTTP status text:" +
                      CommonUtils.exceptionStr(ex));
      return null;
    }
    Object.defineProperty(this, "statusText", {value: statusText});
    return statusText;
  },

  /**
   * Boolean flag that indicates whether the HTTP status code is 2xx or not.
   */
  get success() {
    let success;
    try {
      success = this.request.channel.requestSucceeded;
    } catch (ex) {
      this._log.debug("Caught exception fetching HTTP success flag:" +
                      CommonUtils.exceptionStr(ex));
      return null;
    }
    Object.defineProperty(this, "success", {value: success});
    return success;
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
                      CommonUtils.exceptionStr(ex));
      return null;
    }

    Object.defineProperty(this, "headers", {value: headers});
    return headers;
  },

  /**
   * HTTP body (string)
   */
  body: null

};

/**
 * Single use MAC authenticated HTTP requests to RESTish resources.
 *
 * @param uri
 *        URI going to the RESTRequest constructor.
 * @param authToken
 *        (Object) An auth token of the form {id: (string), key: (string)}
 *        from which the MAC Authentication header for this request will be
 *        derived. A token as obtained from
 *        TokenServerClient.getTokenFromBrowserIDAssertion is accepted.
 * @param extra
 *        (Object) Optional extra parameters. Valid keys are: nonce_bytes, ts,
 *        nonce, and ext. See CrytoUtils.computeHTTPMACSHA1 for information on
 *        the purpose of these values.
 */
this.TokenAuthenticatedRESTRequest =
 function TokenAuthenticatedRESTRequest(uri, authToken, extra) {
  RESTRequest.call(this, uri);
  this.authToken = authToken;
  this.extra = extra || {};
}
TokenAuthenticatedRESTRequest.prototype = {
  __proto__: RESTRequest.prototype,

  dispatch: function dispatch(method, data, onComplete, onProgress) {
    let sig = CryptoUtils.computeHTTPMACSHA1(
      this.authToken.id, this.authToken.key, method, this.uri, this.extra
    );

    this.setHeader("Authorization", sig.getHeader());

    return RESTRequest.prototype.dispatch.call(
      this, method, data, onComplete, onProgress
    );
  },
};
