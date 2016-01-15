/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

// COMPLETE_LENGTH and PARTIAL_LENGTH copied from nsUrlClassifierDBService.h,
// they correspond to the length, in bytes, of a hash prefix and the total
// hash.
const COMPLETE_LENGTH = 32;
const PARTIAL_LENGTH = 4;

// These backoff related constants are taken from v2 of the Google Safe Browsing
// API. All times are in milliseconds.
// BACKOFF_ERRORS: the number of errors incurred until we start to back off.
// BACKOFF_INTERVAL: the initial time to wait once we start backing
//                   off.
// BACKOFF_MAX: as the backoff time doubles after each failure, this is a
//              ceiling on the time to wait.

const BACKOFF_ERRORS = 2;
const BACKOFF_INTERVAL = 30 * 60 * 1000;
const BACKOFF_MAX = 8 * 60 * 60 * 1000;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// Log only if browser.safebrowsing.debug is true
function log(...stuff) {
  let logging = null;
  try {
    logging = Services.prefs.getBoolPref("browser.safebrowsing.debug");
  } catch(e) {
    return;
  }
  if (!logging) {
    return;
  }

  var d = new Date();
  let msg = "hashcompleter: " + d.toTimeString() + ": " + stuff.join(" ");
  dump(msg + "\n");
}

function HashCompleter() {
  // The current HashCompleterRequest in flight. Once it is started, it is set
  // to null. It may be used by multiple calls to |complete| in succession to
  // avoid creating multiple requests to the same gethash URL.
  this._currentRequest = null;
  // A map of gethashUrls to HashCompleterRequests that haven't yet begun.
  this._pendingRequests = {};

  // A map of gethash URLs to RequestBackoff objects.
  this._backoffs = {};

  // Whether we have been informed of a shutdown by the xpcom-shutdown event.
  this._shuttingDown = false;

  Services.obs.addObserver(this, "xpcom-shutdown", true);
}

HashCompleter.prototype = {
  classID: Components.ID("{9111de73-9322-4bfc-8b65-2b727f3e6ec8}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIUrlClassifierHashCompleter,
                                         Ci.nsIRunnable,
                                         Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsITimerCallback,
                                         Ci.nsISupports]),

  // This is mainly how the HashCompleter interacts with other components.
  // Even though it only takes one partial hash and callback, subsequent
  // calls are made into the same HTTP request by using a thread dispatch.
  complete: function HC_complete(aPartialHash, aGethashUrl, aCallback) {
    if (!aGethashUrl) {
      throw Cr.NS_ERROR_NOT_INITIALIZED;
    }

    if (!this._currentRequest) {
      this._currentRequest = new HashCompleterRequest(this, aGethashUrl);
    }
    if (this._currentRequest.gethashUrl == aGethashUrl) {
      this._currentRequest.add(aPartialHash, aCallback);
    } else {
      if (!this._pendingRequests[aGethashUrl]) {
        this._pendingRequests[aGethashUrl] =
          new HashCompleterRequest(this, aGethashUrl);
      }
      this._pendingRequests[aGethashUrl].add(aPartialHash, aCallback);
    }

    if (!this._backoffs[aGethashUrl]) {
      // Initialize request backoffs separately, since requests are deleted
      // after they are dispatched.
      var jslib = Cc["@mozilla.org/url-classifier/jslib;1"]
                  .getService().wrappedJSObject;
      this._backoffs[aGethashUrl] = new jslib.RequestBackoff(
        BACKOFF_ERRORS /* max errors */,
        60*1000 /* retry interval, 1 min */,
        10 /* keep track of max requests */,
        0 /* don't throttle on successful requests per time period */,
        BACKOFF_INTERVAL /* backoff interval, 60 min */,
        BACKOFF_MAX /* max backoff, 8hr */);
    }
    // Start off this request. Without dispatching to a thread, every call to
    // complete makes an individual HTTP request.
    Services.tm.currentThread.dispatch(this, Ci.nsIThread.DISPATCH_NORMAL);
  },

  // This is called after several calls to |complete|, or after the
  // currentRequest has finished.  It starts off the HTTP request by making a
  // |begin| call to the HashCompleterRequest.
  run: function() {
    // Clear everything on shutdown
    if (this._shuttingDown) {
      this._currentRequest = null;
      this._pendingRequests = null;
      for (var url in this._backoffs) {
        this._backoffs[url] = null;
      }
      throw Cr.NS_ERROR_NOT_INITIALIZED;
    }

    // If we don't have an in-flight request, make one
    let pendingUrls = Object.keys(this._pendingRequests);
    if (!this._currentRequest && (pendingUrls.length > 0)) {
      let nextUrl = pendingUrls[0];
      this._currentRequest = this._pendingRequests[nextUrl];
      delete this._pendingRequests[nextUrl];
    }

    if (this._currentRequest) {
      try {
        this._currentRequest.begin();
      } finally {
        // If |begin| fails, we should get rid of our request.
        this._currentRequest = null;
      }
    }
  },

  // Pass the server response status to the RequestBackoff for the given
  // gethashUrl and fetch the next pending request, if there is one.
  finishRequest: function(url, aStatus) {
    this._backoffs[url].noteServerResponse(aStatus);
    Services.tm.currentThread.dispatch(this, Ci.nsIThread.DISPATCH_NORMAL);
  },

  // Returns true if we can make a request from the given url, false otherwise.
  canMakeRequest: function(aGethashUrl) {
    return this._backoffs[aGethashUrl].canMakeRequest();
  },

  // Notifies the RequestBackoff of a new request so we can throttle based on
  // max requests/time period. This must be called before a channel is opened,
  // and finishRequest must be called once the response is received.
  noteRequest: function(aGethashUrl) {
    return this._backoffs[aGethashUrl].noteRequest();
  },

  observe: function HC_observe(aSubject, aTopic, aData) {
    if (aTopic == "xpcom-shutdown") {
      this._shuttingDown = true;
    }
  },
};

function HashCompleterRequest(aCompleter, aGethashUrl) {
  // HashCompleter object that created this HashCompleterRequest.
  this._completer = aCompleter;
  // The internal set of hashes and callbacks that this request corresponds to.
  this._requests = [];
  // nsIChannel that the hash completion query is transmitted over.
  this._channel = null;
  // Response body of hash completion. Created in onDataAvailable.
  this._response = "";
  // Whether we have been informed of a shutdown by the xpcom-shutdown event.
  this._shuttingDown = false;
  this.gethashUrl = aGethashUrl;
}
HashCompleterRequest.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIRequestObserver,
                                         Ci.nsIStreamListener,
                                         Ci.nsIObserver,
                                         Ci.nsISupports]),

  // This is called by the HashCompleter to add a hash and callback to the
  // HashCompleterRequest. It must be called before calling |begin|.
  add: function HCR_add(aPartialHash, aCallback) {
    this._requests.push({
      partialHash: aPartialHash,
      callback: aCallback,
      responses: []
    });
  },

  // This initiates the HTTP request. It can fail due to backoff timings and
  // will notify all callbacks as necessary. We notify the backoff object on
  // begin.
  begin: function HCR_begin() {
    if (!this._completer.canMakeRequest(this.gethashUrl)) {
      dump("hashcompleter: Can't make request to " + this.gethashUrl + "\n");
      this.notifyFailure(Cr.NS_ERROR_ABORT);
      return;
    }

    Services.obs.addObserver(this, "xpcom-shutdown", false);

    try {
      this.openChannel();
      // Notify the RequestBackoff if opening the channel succeeded. At this
      // point, finishRequest must be called.
      this._completer.noteRequest(this.gethashUrl);
    }
    catch (err) {
      this.notifyFailure(err);
      throw err;
    }
  },

  notify: function HCR_notify() {
    // If we haven't gotten onStopRequest, just cancel. This will call us
    // with onStopRequest since we implement nsIStreamListener on the
    // channel.
    if (this._channel && this._channel.isPending()) {
      dump("hashcompleter: cancelling request to " + this.gethashUrl + "\n");
      this._channel.cancel(Cr.NS_BINDING_ABORTED);
    }
  },

  // Creates an nsIChannel for the request and fills the body.
  openChannel: function HCR_openChannel() {
    let loadFlags = Ci.nsIChannel.INHIBIT_CACHING |
                    Ci.nsIChannel.LOAD_BYPASS_CACHE;

    let uri = Services.io.newURI(this.gethashUrl, null, null);
    let channel = Services.io.newChannelFromURI2(uri,
                                                 null,      // aLoadingNode
                                                 Services.scriptSecurityManager.getSystemPrincipal(),
                                                 null,      // aTriggeringPrincipal
                                                 Ci.nsILoadInfo.SEC_NORMAL,
                                                 Ci.nsIContentPolicy.TYPE_OTHER);
    channel.loadFlags = loadFlags;

    // Disable keepalive.
    let httpChannel = channel.QueryInterface(Ci.nsIHttpChannel);
    httpChannel.setRequestHeader("Connection", "close", false);

    this._channel = channel;

    let body = this.buildRequest();
    this.addRequestBody(body);

    // Set a timer that cancels the channel after timeout_ms in case we
    // don't get a gethash response.
    this.timer_ = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    // Ask the timer to use nsITimerCallback (.notify()) when ready
    let timeout = Services.prefs.getIntPref(
      "urlclassifier.gethash.timeout_ms");
    this.timer_.initWithCallback(this, timeout, this.timer_.TYPE_ONE_SHOT);
    channel.asyncOpen(this, null);
  },

  // Returns a string for the request body based on the contents of
  // this._requests.
  buildRequest: function HCR_buildRequest() {
    // Sometimes duplicate entries are sent to HashCompleter but we do not need
    // to propagate these to the server. (bug 633644)
    let prefixes = [];

    for (let i = 0; i < this._requests.length; i++) {
      let request = this._requests[i];
      if (prefixes.indexOf(request.partialHash) == -1) {
        prefixes.push(request.partialHash);
      }
    }

    // Randomize the order to obscure the original request from noise
    // unbiased Fisher-Yates shuffle
    let i = prefixes.length;
    while (i--) {
      let j = Math.floor(Math.random() * (i + 1));
      let temp = prefixes[i];
      prefixes[i] = prefixes[j];
      prefixes[j] = temp;
    }

    let body;
    body = PARTIAL_LENGTH + ":" + (PARTIAL_LENGTH * prefixes.length) +
           "\n" + prefixes.join("");

    log('Requesting completions for ' + prefixes.length + ' ' + PARTIAL_LENGTH + '-byte prefixes: ' + body);
    return body;
  },

  // Sets the request body of this._channel.
  addRequestBody: function HCR_addRequestBody(aBody) {
    let inputStream = Cc["@mozilla.org/io/string-input-stream;1"].
                      createInstance(Ci.nsIStringInputStream);

    inputStream.setData(aBody, aBody.length);

    let uploadChannel = this._channel.QueryInterface(Ci.nsIUploadChannel);
    uploadChannel.setUploadStream(inputStream, "text/plain", -1);

    let httpChannel = this._channel.QueryInterface(Ci.nsIHttpChannel);
    httpChannel.requestMethod = "POST";
  },

  // Parses the response body and eventually adds items to the |responses| array
  // for elements of |this._requests|.
  handleResponse: function HCR_handleResponse() {
    if (this._response == "") {
      return;
    }

    log('Response: ' + this._response);
    let start = 0;

    let length = this._response.length;
    while (start != length) {
      start = this.handleTable(start);
    }
  },

  // This parses a table entry in the response body and calls |handleItem|
  // for complete hash in the table entry.
  handleTable: function HCR_handleTable(aStart) {
    let body = this._response.substring(aStart);

    // deal with new line indexes as there could be
    // new line characters in the data parts.
    let newlineIndex = body.indexOf("\n");
    if (newlineIndex == -1) {
      throw errorWithStack();
    }
    let header = body.substring(0, newlineIndex);
    let entries = header.split(":");
    if (entries.length != 3) {
      throw errorWithStack();
    }

    let list = entries[0];
    let addChunk = parseInt(entries[1]);
    let dataLength = parseInt(entries[2]);

    log('Response includes add chunks for ' + list + ': ' + addChunk);
    if (dataLength % COMPLETE_LENGTH != 0 ||
        dataLength == 0 ||
        dataLength > body.length - (newlineIndex + 1)) {
      throw errorWithStack();
    }

    let data = body.substr(newlineIndex + 1, dataLength);
    for (let i = 0; i < (dataLength / COMPLETE_LENGTH); i++) {
      this.handleItem(data.substr(i * COMPLETE_LENGTH, COMPLETE_LENGTH), list,
                      addChunk);
    }

    return aStart + newlineIndex + 1 + dataLength;
  },

  // This adds a complete hash to any entry in |this._requests| that matches
  // the hash.
  handleItem: function HCR_handleItem(aData, aTableName, aChunkId) {
    for (let i = 0; i < this._requests.length; i++) {
      let request = this._requests[i];
      if (aData.substring(0,4) == request.partialHash) {
        request.responses.push({
          completeHash: aData,
          tableName: aTableName,
          chunkId: aChunkId,
        });
      }
    }
  },

  // notifySuccess and notifyFailure are used to alert the callbacks with
  // results. notifySuccess makes |completion| and |completionFinished| calls
  // while notifyFailure only makes a |completionFinished| call with the error
  // code.
  notifySuccess: function HCR_notifySuccess() {
    for (let i = 0; i < this._requests.length; i++) {
      let request = this._requests[i];
      for (let j = 0; j < request.responses.length; j++) {
        let response = request.responses[j];
        request.callback.completion(response.completeHash, response.tableName,
                                    response.chunkId);
      }

      request.callback.completionFinished(Cr.NS_OK);
    }
  },

  notifyFailure: function HCR_notifyFailure(aStatus) {
    dump("hashcompleter: notifying failure\n");
    for (let i = 0; i < this._requests.length; i++) {
      let request = this._requests[i];
      request.callback.completionFinished(aStatus);
    }
  },

  onDataAvailable: function HCR_onDataAvailable(aRequest, aContext,
                                                aInputStream, aOffset, aCount) {
    let sis = Cc["@mozilla.org/scriptableinputstream;1"].
              createInstance(Ci.nsIScriptableInputStream);
    sis.init(aInputStream);
    this._response += sis.readBytes(aCount);
  },

  onStartRequest: function HCR_onStartRequest(aRequest, aContext) {
    // At this point no data is available for us and we have no reason to
    // terminate the connection, so we do nothing until |onStopRequest|.
  },

  onStopRequest: function HCR_onStopRequest(aRequest, aContext, aStatusCode) {
    Services.obs.removeObserver(this, "xpcom-shutdown");

    if (this._shuttingDown) {
      throw Cr.NS_ERROR_ABORT;
    }

    // Default HTTP status to service unavailable, in case we can't retrieve
    // the true status from the channel.
    let httpStatus = 503;
    if (Components.isSuccessCode(aStatusCode)) {
      let channel = aRequest.QueryInterface(Ci.nsIHttpChannel);
      let success = channel.requestSucceeded;
      httpStatus = channel.responseStatus;
      if (!success) {
        aStatusCode = Cr.NS_ERROR_ABORT;
      }
    }
    log('Received a ' + httpStatus + ' status code from the gethash server.');

    let success = Components.isSuccessCode(aStatusCode);
    // Notify the RequestBackoff once a response is received.
    this._completer.finishRequest(this.gethashUrl, httpStatus);

    if (success) {
      try {
        this.handleResponse();
      }
      catch (err) {
        dump(err.stack);
        aStatusCode = err.value;
        success = false;
      }
    }

    if (success) {
      this.notifySuccess();
    } else {
      this.notifyFailure(aStatusCode);
    }
  },

  observe: function HCR_observe(aSubject, aTopic, aData) {
    if (aTopic != "xpcom-shutdown") {
      return;
    }

    this._shuttingDown = true;
    if (this._channel) {
      this._channel.cancel(Cr.NS_ERROR_ABORT);
    }
  },
};

// Converts a URL safe base64 string to a normal base64 string. Will not change
// normal base64 strings. This is modelled after the same function in
// nsUrlClassifierUtils.h.
function unUrlsafeBase64(aStr) {
  return !aStr ? "" : aStr.replace(/-/g, "+")
                          .replace(/_/g, "/");
}

function errorWithStack() {
  let err = new Error();
  err.value = Cr.NS_ERROR_FAILURE;
  return err;
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([HashCompleter]);
