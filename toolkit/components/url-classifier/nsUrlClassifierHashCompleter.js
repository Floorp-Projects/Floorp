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
// API.
// BACKOFF_ERRORS: the number of errors incurred until we start to back off.
// BACKOFF_INTERVAL: the initial time, in seconds, to wait once we start backing
//                   off.
// BACKOFF_MAX: as the backoff time doubles after each failure, this is a
//              ceiling on the time to wait, in seconds.
// BACKOFF_TIME: length of the interval of time, in seconds, during which errors
//               are taken into account.

const BACKOFF_ERRORS = 2;
const BACKOFF_INTERVAL = 30 * 60;
const BACKOFF_MAX = 8 * 60 * 60;
const BACKOFF_TIME = 5 * 60;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function HashCompleter() {
  // This is a HashCompleterRequest and is used by multiple calls to |complete|
  // in succession to avoid unnecessarily creating requests. Once it has been
  // started, this is set to null again.
  this._currentRequest = null;

  // Whether we have been informed of a shutdown by the xpcom-shutdown event.
  this._shuttingDown = false;

  // All of these backoff properties are different per completer as the DB
  // service keeps one completer per table.
  //
  // _backoff tells us whether we are "backing off" from making requests.
  // It is set in |noteServerResponse| and set after a number of failures.
  this._backoff = false;
  // _backoffTime tells us how long we should wait (in seconds) before making
  // another request.
  this._backoffTime = 0;
  // _nextRequestTime is the earliest time at which we are allowed to make
  // another request by the backoff policy. It is measured in milliseconds.
  this._nextRequestTime = 0;
  // A list of the times at which a failed request was made, recorded in
  // |noteServerResponse|. Sorted by oldest to newest and its length is clamped
  // by BACKOFF_ERRORS.
  this._errorTimes = [];

  Services.obs.addObserver(this, "xpcom-shutdown", true);
}
HashCompleter.prototype = {
  classID: Components.ID("{9111de73-9322-4bfc-8b65-2b727f3e6ec8}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIUrlClassifierHashCompleter,
                                         Ci.nsIRunnable,
                                         Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsISupports]),

  // This is mainly how the HashCompleter interacts with other components.
  // Even though it only takes one partial hash and callback, subsequent
  // calls are made into the same HTTP request by using a thread dispatch.
  complete: function HC_complete(aPartialHash, aCallback) {
    if (!this._currentRequest) {
      this._currentRequest = new HashCompleterRequest(this);

      // It's possible for getHashUrl to not be set, usually at start-up.
      if (this._getHashUrl) {
        Services.tm.currentThread.dispatch(this, Ci.nsIThread.DISPATCH_NORMAL);
      }
    }

    this._currentRequest.add(aPartialHash, aCallback);
  },

  // This is called when either the getHashUrl has been set or after a few calls
  // to |complete|. It starts off the HTTP request by making a |begin| call
  // to the HashCompleterRequest.
  run: function HC_run() {
    if (this._shuttingDown) {
      this._currentRequest = null;
      throw Cr.NS_ERROR_NOT_INITIALIZED;
    }

    if (!this._currentRequest) {
      return;
    }

    if (!this._getHashUrl) {
      throw Cr.NS_ERROR_NOT_INITIALIZED;
    }

    let url = this._getHashUrl;

    let uri = Services.io.newURI(url, null, null);
    this._currentRequest.setURI(uri);

    // If |begin| fails, we should get rid of our request.
    try {
      this._currentRequest.begin();
    }
    finally {
      this._currentRequest = null;
    }
  },

  get gethashUrl() {
    return this._getHashUrl;
  },
  // Because we hold off on making a request until we have a valid getHashUrl,
  // we kick off the process here.
  set gethashUrl(aNewUrl) {
    this._getHashUrl = aNewUrl;

    if (this._currentRequest) {
      Services.tm.currentThread.dispatch(this, Ci.nsIThread.DISPATCH_NORMAL);
    }
  },

  // This handles all the logic about setting a back off time based on
  // server responses. It should only be called once in the life time of a
  // request.
  // The logic behind backoffs is documented in the Google Safe Browsing API,
  // the general idea is that a completer should go into backoff mode after
  // BACKOFF_ERRORS errors in the last BACKOFF_TIME seconds. From there,
  // we do not make a request for BACKOFF_INTERVAL seconds and for every failed
  // request after that we double how long to wait, to a maximum of BACKOFF_MAX.
  // Note that even in the case of a successful response we still keep a history
  // of past errors.
  noteServerResponse: function HC_noteServerResponse(aSuccess) {
    if (aSuccess) {
      this._backoff = false;
      this._nextRequestTime = 0;
      this._backoffTime = 0;
      return;
    }

    let now = Date.now();

    // We only alter the size of |_errorTimes| here, so we can guarantee that
    // its length is at most BACKOFF_ERRORS.
    this._errorTimes.push(now);
    if (this._errorTimes.length > BACKOFF_ERRORS) {
      this._errorTimes.shift();
    }

    if (this._backoff) {
      this._backoffTime *= 2;
    } else if (this._errorTimes.length == BACKOFF_ERRORS &&
               ((now - this._errorTimes[0]) / 1000) <= BACKOFF_TIME) {
      this._backoff = true;
      this._backoffTime = BACKOFF_INTERVAL;
    }

    if (this._backoff) {
      this._backoffTime = Math.min(this._backoffTime, BACKOFF_MAX);
      this._nextRequestTime = now + (this._backoffTime * 1000);
    }
  },

  // This is not included on the interface but is used to communicate to the
  // HashCompleterRequest. It returns a time in milliseconds.
  get nextRequestTime() {
    return this._nextRequestTime;
  },

  observe: function HC_observe(aSubject, aTopic, aData) {
    if (aTopic == "xpcom-shutdown") {
      this._shuttingDown = true;
    }
  },
};

function HashCompleterRequest(aCompleter) {
  // HashCompleter object that created this HashCompleterRequest.
  this._completer = aCompleter;
  // The internal set of hashes and callbacks that this request corresponds to.
  this._requests = [];
  // URI to query for hash completions. Largely comes from the
  // browser.safebrowsing.gethashURL pref.
  this._uri = null;
  // nsIChannel that the hash completion query is transmitted over.
  this._channel = null;
  // Response body of hash completion. Created in onDataAvailable.
  this._response = "";
  // Whether we have been informed of a shutdown by the xpcom-shutdown event.
  this._shuttingDown = false;
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
      responses: [],
    });
  },

  // This initiates the HTTP request. It can fail due to backoff timings and
  // will notify all callbacks as necessary.
  begin: function HCR_begin() {
    if (Date.now() < this._completer.nextRequestTime) {
      this.notifyFailure(Cr.NS_ERROR_ABORT);
      return;
    }

    Services.obs.addObserver(this, "xpcom-shutdown", false);

    try {
      this.openChannel();
    }
    catch (err) {
      this.notifyFailure(err);
      throw err;
    }
  },

  setURI: function HCR_setURI(aURI) {
    this._uri = aURI;
  },

  // Creates an nsIChannel for the request and fills the body.
  openChannel: function HCR_openChannel() {
    let loadFlags = Ci.nsIChannel.INHIBIT_CACHING |
                    Ci.nsIChannel.LOAD_BYPASS_CACHE;

    let channel = Services.io.newChannelFromURI(this._uri);
    channel.loadFlags = loadFlags;

    this._channel = channel;

    let body = this.buildRequest();
    this.addRequestBody(body);

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

    let start = 0;

    let length = this._response.length;
    while (start != length)
      start = this.handleTable(start);
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

    if (Components.isSuccessCode(aStatusCode)) {
      let channel = aRequest.QueryInterface(Ci.nsIHttpChannel);
      let success = channel.requestSucceeded;
      if (!success) {
        aStatusCode = Cr.NS_ERROR_ABORT;
      }
    }

    let success = Components.isSuccessCode(aStatusCode);
    this._completer.noteServerResponse(success);

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
