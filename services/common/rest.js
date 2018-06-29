/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = [
  "RESTRequest",
  "RESTResponse",
  "TokenAuthenticatedRESTRequest",
];

ChromeUtils.import("resource://gre/modules/Preferences.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Log.jsm");
ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm");
ChromeUtils.import("resource://services-common/utils.js");

ChromeUtils.defineModuleGetter(this, "CryptoUtils",
                               "resource://services-crypto/utils.js");

function decodeString(data, charset) {
  if (!data || !charset) {
    return data;
  }

  // This could be simpler if we assumed the charset is only ever UTF-8.
  // It's unclear to me how willing we are to assume this, though...
  let stringStream = Cc["@mozilla.org/io/string-input-stream;1"]
                     .createInstance(Ci.nsIStringInputStream);
  stringStream.setData(data, data.length);

  let converterStream = Cc["@mozilla.org/intl/converter-input-stream;1"]
                        .createInstance(Ci.nsIConverterInputStream);

  converterStream.init(stringStream, charset, 0,
                       converterStream.DEFAULT_REPLACEMENT_CHARACTER);

  let remaining = data.length;
  let body = "";
  while (remaining > 0) {
    let str = {};
    let num = converterStream.readString(remaining, str);
    if (!num) {
      break;
    }
    remaining -= num;
    body += str.value;
  }
  return body;
}

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
 *   let response = await new RESTRequest("http://server/rest/resource").get();
 *   if (!response.success) {
 *     // Bail out if we're not getting an HTTP 2xx code.
 *     processHTTPError(response.status);
 *     return;
 *   }
 *   processData(response.body);
 *
 * (2) Quick PUT request (non-string data is automatically JSONified)
 *
 *   let response = await new RESTRequest("http://server/rest/resource").put(data);
 */
function RESTRequest(uri) {
  this.status = this.NOT_SENT;

  // If we don't have an nsIURI object yet, make one. This will throw if
  // 'uri' isn't a valid URI string.
  if (!(uri instanceof Ci.nsIURI)) {
    uri = Services.io.newURI(uri);
  }
  this.uri = uri;

  this._headers = {};
  this._deferred = PromiseUtils.defer();
  this._log = Log.repository.getLogger(this._logName);
  this._log.manageLevelFromPref("services.common.log.logger.rest.request");
}

RESTRequest.prototype = {

  _logName: "Services.Common.RESTRequest",

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIBadCertListener2,
    Ci.nsIInterfaceRequestor,
    Ci.nsIChannelEventSink
  ]),

  /** Public API: **/

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
   * Set a request header.
   */
  setHeader(name, value) {
    this._headers[name.toLowerCase()] = value;
  },

  /**
   * Perform an HTTP GET.
   *
   * @return Promise<RESTResponse>
   */
  async get() {
    return this.dispatch("GET", null);
  },

  /**
   * Perform an HTTP PATCH.
   *
   * @param data
   *        Data to be used as the request body. If this isn't a string
   *        it will be JSONified automatically.
   *
   * @return Promise<RESTResponse>
   */
  async patch(data) {
    return this.dispatch("PATCH", data);
  },

  /**
   * Perform an HTTP PUT.
   *
   * @param data
   *        Data to be used as the request body. If this isn't a string
   *        it will be JSONified automatically.
   *
   * @return Promise<RESTResponse>
   */
  async put(data) {
    return this.dispatch("PUT", data);
  },

  /**
   * Perform an HTTP POST.
   *
   * @param data
   *        Data to be used as the request body. If this isn't a string
   *        it will be JSONified automatically.
   *
   * @return Promise<RESTResponse>
   */
  async post(data) {
    return this.dispatch("POST", data);
  },

  /**
   * Perform an HTTP DELETE.
   *
   * @return Promise<RESTResponse>
   */
  async delete() {
    return this.dispatch("DELETE", null);
  },

  /**
   * Abort an active request.
   */
  abort(rejectWithError = null) {
    if (this.status != this.SENT && this.status != this.IN_PROGRESS) {
      throw new Error("Can only abort a request that has been sent.");
    }

    this.status = this.ABORTED;
    this.channel.cancel(Cr.NS_BINDING_ABORTED);

    if (this.timeoutTimer) {
      // Clear the abort timer now that the channel is done.
      this.timeoutTimer.clear();
    }
    if (rejectWithError) {
      this._deferred.reject(rejectWithError);
    }
  },

  /** Implementation stuff **/

  async dispatch(method, data) {
    if (this.status != this.NOT_SENT) {
      throw new Error("Request has already been sent!");
    }

    this.method = method;

    // Create and initialize HTTP channel.
    let channel = NetUtil.newChannel({uri: this.uri, loadUsingSystemPrincipal: true})
                         .QueryInterface(Ci.nsIRequest)
                         .QueryInterface(Ci.nsIHttpChannel);
    this.channel = channel;
    channel.loadFlags |= this.loadFlags;
    channel.notificationCallbacks = this;

    this._log.debug(`${method} request to ${this.uri.spec}`);
    // Set request headers.
    let headers = this._headers;
    for (let key in headers) {
      if (key == "authorization" || key == "x-client-state") {
        this._log.trace("HTTP Header " + key + ": ***** (suppressed)");
      } else {
        this._log.trace("HTTP Header " + key + ": " + headers[key]);
      }
      channel.setRequestHeader(key, headers[key], false);
    }

    // REST requests accept JSON by default
    if (!headers.accept) {
      channel.setRequestHeader("accept", "application/json;q=0.9,*/*;q=0.2", false);
    }

    // Set HTTP request body.
    if (method == "PUT" || method == "POST" || method == "PATCH") {
      // Convert non-string bodies into JSON with utf-8 encoding. If a string
      // is passed we assume they've already encoded it.
      let contentType = headers["content-type"];
      if (typeof data != "string") {
        data = JSON.stringify(data);
        if (!contentType) {
          contentType = "application/json";
        }
        if (!contentType.includes("charset")) {
          data = CommonUtils.encodeUTF8(data);
          contentType += "; charset=utf-8";
        } else {
          // If someone handed us an object but also a custom content-type
          // it's probably confused. We could go to even further lengths to
          // respect it, but this shouldn't happen in practice.
          Cu.reportError("rest.js found an object to JSON.stringify but also a " +
                         "content-type header with a charset specification. " +
                         "This probably isn't going to do what you expect");
        }
      }
      if (!contentType) {
        contentType = "text/plain";
      }

      this._log.debug(method + " Length: " + data.length);
      if (this._log.level <= Log.Level.Trace) {
        this._log.trace(method + " Body: " + data);
      }

      let stream = Cc["@mozilla.org/io/string-input-stream;1"]
                     .createInstance(Ci.nsIStringInputStream);
      stream.setData(data, data.length);

      channel.QueryInterface(Ci.nsIUploadChannel);
      channel.setUploadStream(stream, contentType, data.length);
    }
    // We must set this after setting the upload stream, otherwise it
    // will always be 'PUT'. Yeah, I know.
    channel.requestMethod = method;

    // Before opening the channel, set the charset that serves as a hint
    // as to what the response might be encoded as.
    channel.contentCharset = this.charset;

    // Blast off!
    try {
      channel.asyncOpen2(this);
    } catch (ex) {
      // asyncOpen can throw in a bunch of cases -- e.g., a forbidden port.
      this._log.warn("Caught an error in asyncOpen", ex);
      this._deferred.reject(ex);
    }
    this.status = this.SENT;
    this.delayTimeout();
    return this._deferred.promise;
  },

  /**
   * Create or push back the abort timer that kills this request.
   */
  delayTimeout() {
    if (this.timeout) {
      CommonUtils.namedTimer(this.abortTimeout, this.timeout * 1000, this,
                             "timeoutTimer");
    }
  },

  /**
   * Abort the request based on a timeout.
   */
  abortTimeout() {
    this.abort(Components.Exception("Aborting due to channel inactivity.",
                                    Cr.NS_ERROR_NET_TIMEOUT));
  },

  /** nsIStreamListener **/

  onStartRequest(channel) {
    if (this.status == this.ABORTED) {
      this._log.trace("Not proceeding with onStartRequest, request was aborted.");
      // We might have already rejected, but just in case.
      this._deferred.reject(Components.Exception("Request aborted", Cr.NS_BINDING_ABORTED));
      return;
    }

    try {
      channel.QueryInterface(Ci.nsIHttpChannel);
    } catch (ex) {
      this._log.error("Unexpected error: channel is not a nsIHttpChannel!");
      this.status = this.ABORTED;
      channel.cancel(Cr.NS_BINDING_ABORTED);
      this._deferred.reject(ex);
      return;
    }

    this.status = this.IN_PROGRESS;

    this._log.trace("onStartRequest: " + channel.requestMethod + " " +
                    channel.URI.spec);

    // Create a new response object.
    this.response = new RESTResponse(this);

    this.delayTimeout();
  },

  onStopRequest(channel, context, statusCode) {
    if (this.timeoutTimer) {
      // Clear the abort timer now that the channel is done.
      this.timeoutTimer.clear();
    }

    // We don't want to do anything for a request that's already been aborted.
    if (this.status == this.ABORTED) {
      this._log.trace("Not proceeding with onStopRequest, request was aborted.");
      // We might not have already rejected if the user called reject() manually.
      // If we have already rejected, then this is a no-op
      this._deferred.reject(Components.Exception("Request aborted",
                                                 Cr.NS_BINDING_ABORTED));
      return;
    }

    try {
      channel.QueryInterface(Ci.nsIHttpChannel);
    } catch (ex) {
      this._log.error("Unexpected error: channel not nsIHttpChannel!");
      this.status = this.ABORTED;
      this._deferred.reject(ex);
      return;
    }

    this.status = this.COMPLETED;

    try {
      this.response.body = decodeString(this.response._rawBody, this.response.charset);
      this.response._rawBody = null;
    } catch (ex) {
      this._log.warn(`Exception decoding response - ${this.method} ${channel.URI.spec}`, ex);
      this._deferred.reject(ex);
      return;
    }

    let statusSuccess = Components.isSuccessCode(statusCode);
    let uri = channel && channel.URI && channel.URI.spec || "<unknown>";
    this._log.trace("Channel for " + channel.requestMethod + " " + uri +
                    " returned status code " + statusCode);

    // Throw the failure code and stop execution.  Use Components.Exception()
    // instead of Error() so the exception is QI-able and can be passed across
    // XPCOM borders while preserving the status code.
    if (!statusSuccess) {
      let message = Components.Exception("", statusCode).name;
      let error = Components.Exception(message, statusCode);
      this._log.debug(this.method + " " + uri + " failed: " + statusCode + " - " + message);
      // Additionally give the full response body when Trace logging.
      if (this._log.level <= Log.Level.Trace) {
        this._log.trace(this.method + " body", this.response.body);
      }
      this._deferred.reject(error);
      return;
    }

    this._log.debug(this.method + " " + uri + " " + this.response.status);

    // Note that for privacy/security reasons we don't log this response body

    delete this._inputStream;

    this._deferred.resolve(this.response);
  },

  onDataAvailable(channel, cb, stream, off, count) {
    // We get an nsIRequest, which doesn't have contentCharset.
    try {
      channel.QueryInterface(Ci.nsIHttpChannel);
    } catch (ex) {
      this._log.error("Unexpected error: channel not nsIHttpChannel!");
      this.abort(ex);
      return;
    }

    if (channel.contentCharset) {
      this.response.charset = channel.contentCharset;
    } else {
      this.response.charset = null;
    }

    if (!this._inputStream) {
      this._inputStream = Cc["@mozilla.org/scriptableinputstream;1"]
                            .createInstance(Ci.nsIScriptableInputStream);
    }
    this._inputStream.init(stream);

    this.response._rawBody += this._inputStream.read(count);

    this.delayTimeout();
  },

  /** nsIInterfaceRequestor **/

  getInterface(aIID) {
    return this.QueryInterface(aIID);
  },

  /** nsIBadCertListener2 **/

  notifyCertProblem(socketInfo, sslStatus, targetHost) {
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
  shouldCopyOnRedirect(oldChannel, newChannel, flags) {
    let isInternal = !!(flags & Ci.nsIChannelEventSink.REDIRECT_INTERNAL);
    let isSameURI  = newChannel.URI.equals(oldChannel.URI);
    this._log.debug("Channel redirect: " + oldChannel.URI.spec + ", " +
                    newChannel.URI.spec + ", internal = " + isInternal);
    return isInternal && isSameURI;
  },

  /** nsIChannelEventSink **/
  asyncOnChannelRedirect(oldChannel, newChannel, flags, callback) {

    let oldSpec = (oldChannel && oldChannel.URI) ? oldChannel.URI.spec : "<undefined>";
    let newSpec = (newChannel && newChannel.URI) ? newChannel.URI.spec : "<undefined>";
    this._log.debug("Channel redirect: " + oldSpec + ", " + newSpec + ", " + flags);

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
      this._log.error("Error copying headers", ex);
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
function RESTResponse(request = null) {
  this.body = "";
  this._rawBody = "";
  this.request = request;
  this._log = Log.repository.getLogger(this._logName);
  this._log.manageLevelFromPref("services.common.log.logger.rest.response");
}
RESTResponse.prototype = {

  _logName: "Services.Common.RESTResponse",

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
      this._log.debug("Caught exception fetching HTTP status code", ex);
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
      this._log.debug("Caught exception fetching HTTP status text", ex);
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
      this._log.debug("Caught exception fetching HTTP success flag", ex);
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
      channel.visitResponseHeaders(function(header, value) {
        headers[header.toLowerCase()] = value;
      });
    } catch (ex) {
      this._log.debug("Caught exception processing response headers", ex);
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
function TokenAuthenticatedRESTRequest(uri, authToken, extra) {
  RESTRequest.call(this, uri);
  this.authToken = authToken;
  this.extra = extra || {};
}
TokenAuthenticatedRESTRequest.prototype = {
  __proto__: RESTRequest.prototype,

  async dispatch(method, data) {
    let sig = CryptoUtils.computeHTTPMACSHA1(
      this.authToken.id, this.authToken.key, method, this.uri, this.extra
    );

    this.setHeader("Authorization", sig.getHeader());

    return super.dispatch(method, data);
  },
};
