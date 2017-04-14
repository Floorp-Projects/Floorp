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

// Upper limit on the server response minimumWaitDuration
const MIN_WAIT_DURATION_MAX_VALUE = 24 * 60 * 60 * 1000;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyServiceGetter(this, 'gDbService',
                                   '@mozilla.org/url-classifier/dbservice;1',
                                   'nsIUrlClassifierDBService');

XPCOMUtils.defineLazyServiceGetter(this, 'gUrlUtil',
                                   '@mozilla.org/url-classifier/utils;1',
                                   'nsIUrlClassifierUtils');

// Log only if browser.safebrowsing.debug is true
function log(...stuff) {
  let logging = Services.prefs.getBoolPref("browser.safebrowsing.debug", false);
  if (!logging) {
    return;
  }

  var d = new Date();
  let msg = "hashcompleter: " + d.toTimeString() + ": " + stuff.join(" ");
  dump(Services.urlFormatter.trimSensitiveURLs(msg) + "\n");
}

// Map the HTTP response code to a Telemetry bucket
// https://developers.google.com/safe-browsing/developers_guide_v2?hl=en
function httpStatusToBucket(httpStatus) {
  var statusBucket;
  switch (httpStatus) {
  case 100:
  case 101:
    // Unexpected 1xx return code
    statusBucket = 0;
    break;
  case 200:
    // OK - Data is available in the HTTP response body.
    statusBucket = 1;
    break;
  case 201:
  case 202:
  case 203:
  case 205:
  case 206:
    // Unexpected 2xx return code
    statusBucket = 2;
    break;
  case 204:
    // No Content - There are no full-length hashes with the requested prefix.
    statusBucket = 3;
    break;
  case 300:
  case 301:
  case 302:
  case 303:
  case 304:
  case 305:
  case 307:
  case 308:
    // Unexpected 3xx return code
    statusBucket = 4;
    break;
  case 400:
    // Bad Request - The HTTP request was not correctly formed.
    // The client did not provide all required CGI parameters.
    statusBucket = 5;
    break;
  case 401:
  case 402:
  case 405:
  case 406:
  case 407:
  case 409:
  case 410:
  case 411:
  case 412:
  case 414:
  case 415:
  case 416:
  case 417:
  case 421:
  case 426:
  case 428:
  case 429:
  case 431:
  case 451:
    // Unexpected 4xx return code
    statusBucket = 6;
    break;
  case 403:
    // Forbidden - The client id is invalid.
    statusBucket = 7;
    break;
  case 404:
    // Not Found
    statusBucket = 8;
    break;
  case 408:
    // Request Timeout
    statusBucket = 9;
    break;
  case 413:
    // Request Entity Too Large - Bug 1150334
    statusBucket = 10;
    break;
  case 500:
  case 501:
  case 510:
    // Unexpected 5xx return code
    statusBucket = 11;
    break;
  case 502:
  case 504:
  case 511:
    // Local network errors, we'll ignore these.
    statusBucket = 12;
    break;
  case 503:
    // Service Unavailable - The server cannot handle the request.
    // Clients MUST follow the backoff behavior specified in the
    // Request Frequency section.
    statusBucket = 13;
    break;
  case 505:
    // HTTP Version Not Supported - The server CANNOT handle the requested
    // protocol major version.
    statusBucket = 14;
    break;
  default:
    statusBucket = 15;
  };
  return statusBucket;
}

function FullHashMatch(table, hash, duration) {
  this.tableName = table;
  this.fullHash = hash;
  this.cacheDuration = duration;
}

FullHashMatch.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFullHashMatch]),

  tableName : null,
  fullHash : null,
  cacheDuration : null,
};

function HashCompleter() {
  // The current HashCompleterRequest in flight. Once it is started, it is set
  // to null. It may be used by multiple calls to |complete| in succession to
  // avoid creating multiple requests to the same gethash URL.
  this._currentRequest = null;
  // A map of gethashUrls to HashCompleterRequests that haven't yet begun.
  this._pendingRequests = {};

  // A map of gethash URLs to RequestBackoff objects.
  this._backoffs = {};

  // Whether we have been informed of a shutdown by the shutdown event.
  this._shuttingDown = false;

  // A map of gethash URLs to next gethash time in miliseconds
  this._nextGethashTimeMs = {};

  Services.obs.addObserver(this, "quit-application");

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
  complete: function HC_complete(aPartialHash, aGethashUrl, aTableName, aCallback) {
    if (!aGethashUrl) {
      throw Cr.NS_ERROR_NOT_INITIALIZED;
    }

    if (!this._currentRequest) {
      this._currentRequest = new HashCompleterRequest(this, aGethashUrl);
    }
    if (this._currentRequest.gethashUrl == aGethashUrl) {
      this._currentRequest.add(aPartialHash, aCallback, aTableName);
    } else {
      if (!this._pendingRequests[aGethashUrl]) {
        this._pendingRequests[aGethashUrl] =
          new HashCompleterRequest(this, aGethashUrl);
      }
      this._pendingRequests[aGethashUrl].add(aPartialHash, aCallback, aTableName);
    }

    if (!this._backoffs[aGethashUrl]) {
      // Initialize request backoffs separately, since requests are deleted
      // after they are dispatched.
      var jslib = Cc["@mozilla.org/url-classifier/jslib;1"]
                  .getService().wrappedJSObject;

      // Using the V4 backoff algorithm for both V2 and V4. See bug 1273398.
      this._backoffs[aGethashUrl] = new jslib.RequestBackoffV4(
        10 /* keep track of max requests */,
        0  /* don't throttle on successful requests per time period */);
    }

    if (!this._nextGethashTimeMs[aGethashUrl]) {
      this._nextGethashTimeMs[aGethashUrl] = 0;
    }

    // Start off this request. Without dispatching to a thread, every call to
    // complete makes an individual HTTP request.
    Services.tm.dispatchToMainThread(this);
  },

  // This is called after several calls to |complete|, or after the
  // currentRequest has finished.  It starts off the HTTP request by making a
  // |begin| call to the HashCompleterRequest.
  run: function() {
    // Clear everything on shutdown
    if (this._shuttingDown) {
      this._currentRequest = null;
      this._pendingRequests = null;
      this._nextGethashTimeMs = null;

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
    Services.tm.dispatchToMainThread(this);
  },

  // Returns true if we can make a request from the given url, false otherwise.
  canMakeRequest: function(aGethashUrl) {
    return this._backoffs[aGethashUrl].canMakeRequest() &&
           Date.now() >= this._nextGethashTimeMs[aGethashUrl];
  },

  // Notifies the RequestBackoff of a new request so we can throttle based on
  // max requests/time period. This must be called before a channel is opened,
  // and finishRequest must be called once the response is received.
  noteRequest: function(aGethashUrl) {
    return this._backoffs[aGethashUrl].noteRequest();
  },

  observe: function HC_observe(aSubject, aTopic, aData) {
    if (aTopic == "quit-application") {
      this._shuttingDown = true;
      Services.obs.removeObserver(this, "quit-application");
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
  // Whether we have been informed of a shutdown by the quit-application event.
  this._shuttingDown = false;
  this.gethashUrl = aGethashUrl;

  // Multiple partial hashes can be associated with the same tables
  // so we use a map here.
  this.tableNames = new Map();

  this.telemetryProvider = "";
  this.telemetryClockStart = 0;
}
HashCompleterRequest.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIRequestObserver,
                                         Ci.nsIStreamListener,
                                         Ci.nsIObserver,
                                         Ci.nsISupports]),

  // This is called by the HashCompleter to add a hash and callback to the
  // HashCompleterRequest. It must be called before calling |begin|.
  add: function HCR_add(aPartialHash, aCallback, aTableName) {
    this._requests.push({
      partialHash: aPartialHash,
      callback: aCallback,
      tableName: aTableName,
      response: { matches:[] },
    });

    if (aTableName) {
      let isTableNameV4 = aTableName.endsWith('-proto');
      if (0 === this.tableNames.size) {
        // Decide if this request is v4 by the first added partial hash.
        this.isV4 = isTableNameV4;
      } else if (this.isV4 !== isTableNameV4) {
        log('ERROR: Cannot mix "proto" tables with other types within ' +
            'the same gethash URL.');
      }
      this.tableNames.set(aTableName);

      // Assuming all tables with the same gethash URL have the same provider
      if (this.telemetryProvider == "") {
        this.telemetryProvider = gUrlUtil.getTelemetryProvider(aTableName);
      }
    }
  },

  fillTableStatesBase64: function HCR_fillTableStatesBase64(aCallback) {
    gDbService.getTables(aTableData => {
      aTableData.split("\n").forEach(line => {
        let p = line.indexOf(";");
        if (-1 === p) {
          return;
        }
        // [tableName];[stateBase64]:[checksumBase64]
        let tableName = line.substring(0, p);
        if (this.tableNames.has(tableName)) {
          let metadata = line.substring(p + 1).split(":");
          let stateBase64 = metadata[0];
          this.tableNames.set(tableName, stateBase64);
        }
      });

      aCallback();
    });

  },

  // This initiates the HTTP request. It can fail due to backoff timings and
  // will notify all callbacks as necessary. We notify the backoff object on
  // begin.
  begin: function HCR_begin() {
    if (!this._completer.canMakeRequest(this.gethashUrl)) {
      log("Can't make request to " + this.gethashUrl + "\n");
      this.notifyFailure(Cr.NS_ERROR_ABORT);
      return;
    }

    Services.obs.addObserver(this, "quit-application");

    // V4 requires table states to build the request so we need
    // a async call to retrieve the table states from disk.
    // Note that |HCR_begin| is fine to be sync because
    // it doesn't appear in a sync call chain.
    this.fillTableStatesBase64(() => {
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
    });
  },

  notify: function HCR_notify() {
    // If we haven't gotten onStopRequest, just cancel. This will call us
    // with onStopRequest since we implement nsIStreamListener on the
    // channel.
    if (this._channel && this._channel.isPending()) {
      log("cancelling request to " + this.gethashUrl + "\n");
      Services.telemetry.getKeyedHistogramById("URLCLASSIFIER_COMPLETE_TIMEOUT2").
        add(this.telemetryProvider, 1);
      this._channel.cancel(Cr.NS_BINDING_ABORTED);
    }
  },

  // Creates an nsIChannel for the request and fills the body.
  openChannel: function HCR_openChannel() {
    let loadFlags = Ci.nsIChannel.INHIBIT_CACHING |
                    Ci.nsIChannel.LOAD_BYPASS_CACHE;

    let actualGethashUrl = this.gethashUrl;
    if (this.isV4) {
      // As per spec, we add the request payload to the gethash url.
      actualGethashUrl += "&$req=" + this.buildRequestV4();
    }

    log("actualGethashUrl: " + actualGethashUrl);

    let channel = NetUtil.newChannel({
      uri: actualGethashUrl,
      loadUsingSystemPrincipal: true
    });
    channel.loadFlags = loadFlags;

    // Disable keepalive.
    let httpChannel = channel.QueryInterface(Ci.nsIHttpChannel);
    httpChannel.setRequestHeader("Connection", "close", false);

    this._channel = channel;

    if (this.isV4) {
      httpChannel.setRequestHeader("X-HTTP-Method-Override", "POST", false);
    } else {
      let body = this.buildRequest();
      this.addRequestBody(body);
    }

    // Set a timer that cancels the channel after timeout_ms in case we
    // don't get a gethash response.
    this.timer_ = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    // Ask the timer to use nsITimerCallback (.notify()) when ready
    let timeout = Services.prefs.getIntPref(
      "urlclassifier.gethash.timeout_ms");
    this.timer_.initWithCallback(this, timeout, this.timer_.TYPE_ONE_SHOT);
    channel.asyncOpen2(this);
    this.telemetryClockStart = Date.now();
  },

  buildRequestV4: function HCR_buildRequestV4() {
    // Convert the "name to state" mapping to two equal-length arrays.
    let tableNameArray = [];
    let stateArray = [];
    this.tableNames.forEach((state, name) => {
      // We skip the table which is not associated with a state.
      if (state) {
        tableNameArray.push(name);
        stateArray.push(state);
      }
    });

    // Build the "distinct" prefix array.
    let prefixSet = new Set();
    this._requests.forEach(r => prefixSet.add(btoa(r.partialHash)));
    let prefixArray = Array.from(prefixSet);

    log("Build v4 gethash request with " + JSON.stringify(tableNameArray) + ', '
                                         + JSON.stringify(stateArray) + ', '
                                         + JSON.stringify(prefixArray));

    return gUrlUtil.makeFindFullHashRequestV4(tableNameArray,
                                              stateArray,
                                              prefixArray,
                                              tableNameArray.length,
                                              prefixArray.length);
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

  // Parses the response body and eventually adds items to the |response.matches| array
  // for elements of |this._requests|.
  handleResponse: function HCR_handleResponse() {
    if (this._response == "") {
      return;
    }

    if (this.isV4) {
      return this.handleResponseV4();
    }

    let start = 0;

    let length = this._response.length;
    while (start != length) {
      start = this.handleTable(start);
    }
  },

  handleResponseV4: function HCR_handleResponseV4() {
    let callback = {
      // onCompleteHashFound will be called for each fullhash found in
      // FullHashResponse.
      onCompleteHashFound : (aCompleteHash,
                             aTableNames,
                             aPerHashCacheDuration) => {
        log("V4 fullhash response complete hash found callback: " +
            JSON.stringify(aCompleteHash) + ", " +
            aTableNames + ", CacheDuration(" + aPerHashCacheDuration + ")");

        // Filter table names which we didn't requested.
        let filteredTables = aTableNames.split(",").filter(name => {
          return this.tableNames.get(name);
        });
        if (0 === filteredTables.length) {
          log("ERROR: Got complete hash which is from unknown table.");
          return;
        }
        if (filteredTables.length > 1) {
          log("WARNING: Got complete hash which has ambigious threat type.");
        }

        this.handleItem({
          completeHash: aCompleteHash,
          tableName: filteredTables[0],
          cacheDuration: aPerHashCacheDuration
        });
      },

      // onResponseParsed will be called no matter if there is match in
      // FullHashResponse, the callback is mainly used to pass negative cache
      // duration and minimum wait duration.
      onResponseParsed : (aMinWaitDuration,
                          aNegCacheDuration) => {
        log("V4 fullhash response parsed callback: " +
            "MinWaitDuration(" + aMinWaitDuration + "), " +
            "NegativeCacheDuration(" + aNegCacheDuration + ")");

        let minWaitDuration = aMinWaitDuration;

        if (aMinWaitDuration > MIN_WAIT_DURATION_MAX_VALUE) {
          log("WARNING: Minimum wait duration too large, clamping it down " +
              "to a reasonable value.");
          minWaitDuration = MIN_WAIT_DURATION_MAX_VALUE;
        } else if (aMinWaitDuration < 0) {
          log("WARNING: Minimum wait duration is negative, reset it to 0");
          minWaitDuration = 0;
        }

        this._completer._nextGethashTimeMs[this.gethashUrl] =
          Date.now() + minWaitDuration;

        // A fullhash request may contain more than one prefix, so the negative
        // cache duration should be set for all the prefixes in the request.
        this._requests.forEach(request => {
          request.response.negCacheDuration = aNegCacheDuration;
        });
      },
    };

    gUrlUtil.parseFindFullHashResponseV4(this._response, callback);
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
      this.handleItem({
        completeHash: data.substr(i * COMPLETE_LENGTH, COMPLETE_LENGTH),
        tableName: list,
        chunkId: addChunk
      });
    }

    return aStart + newlineIndex + 1 + dataLength;
  },

  // This adds a complete hash to any entry in |this._requests| that matches
  // the hash.
  handleItem: function HCR_handleItem(aData) {
    for (let i = 0; i < this._requests.length; i++) {
      let request = this._requests[i];
      if (aData.completeHash.startsWith(request.partialHash)) {
        request.response.matches.push(aData);
      }
    }
  },

  // notifySuccess and notifyFailure are used to alert the callbacks with
  // results. notifySuccess makes |completion| and |completionFinished| calls
  // while notifyFailure only makes a |completionFinished| call with the error
  // code.
  notifySuccess: function HCR_notifySuccess() {
    // V2 completion handler
    let completionV2 = (req) => {
      req.response.matches.forEach((m) => {
        req.callback.completionV2(m.completeHash, m.tableName, m.chunkId);
      });

      req.callback.completionFinished(Cr.NS_OK);
    };

    // V4 completion handler
    let completionV4 = (req) => {
      let matches = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);

      req.response.matches.forEach(m => {
        matches.appendElement(
          new FullHashMatch(m.tableName, m.completeHash, m.cacheDuration));
      });

      req.callback.completionV4(req.partialHash, req.tableName,
                                req.response.negCacheDuration, matches);

      req.callback.completionFinished(Cr.NS_OK);
    };

    let completion = this.isV4 ? completionV4 : completionV2;
    this._requests.forEach((req) => { completion(req); });
  },

  notifyFailure: function HCR_notifyFailure(aStatus) {
    log("notifying failure\n");
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
    this._completer._nextGethashTimeMs[this.gethashUrl] = 0;

    if (this.telemetryClockStart > 0) {
      let msecs = Date.now() - this.telemetryClockStart;
      Services.telemetry.getKeyedHistogramById("URLCLASSIFIER_COMPLETE_SERVER_RESPONSE_TIME").
        add(this.telemetryProvider, msecs);
    }
  },

  onStopRequest: function HCR_onStopRequest(aRequest, aContext, aStatusCode) {
    Services.obs.removeObserver(this, "quit-application");

    if (this.timer_) {
      this.timer_.cancel();
      this.timer_ = null;
    }

    this.telemetryClockStart = 0;

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
    let success = Components.isSuccessCode(aStatusCode);
    log('Received a ' + httpStatus + ' status code from the gethash server (success=' + success + ').');

    Services.telemetry.getKeyedHistogramById("URLCLASSIFIER_COMPLETE_REMOTE_STATUS2").
      add(this.telemetryProvider, httpStatusToBucket(httpStatus));
    Services.telemetry.getKeyedHistogramById("URLCLASSIFIER_COMPLETE_TIMEOUT2").
      add(this.telemetryProvider, 0);

    // Notify the RequestBackoff once a response is received.
    this._completer.finishRequest(this.gethashUrl, httpStatus);

    if (success) {
      try {
        this.handleResponse();
      }
      catch (err) {
        log(err.stack);
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
    if (aTopic == "quit-application") {
      this._shuttingDown = true;
      if (this._channel) {
        this._channel.cancel(Cr.NS_ERROR_ABORT);
        telemetryClockStart = 0;
      }

      Services.obs.removeObserver(this, "quit-application");
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
