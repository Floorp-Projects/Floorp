Cu.import("resource://testing-common/httpd.js");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

var httpserver = new HttpServer();
var testpath = "/simple";
var httpbody = "0123456789";

var last = 0, max = 0;

const STATUS_RECEIVING_FROM = 0x804b0006;
const LOOPS = 50000;

const TYPE_ONSTATUS = 1;
const TYPE_ONPROGRESS = 2;
const TYPE_ONSTARTREQUEST = 3;
const TYPE_ONDATAAVAILABLE = 4;
const TYPE_ONSTOPREQUEST = 5;

var progressCallback = {
  _listener: null,
  _got_onstartrequest: false,
  _got_onstatus_after_onstartrequest: false,
  _last_callback_handled: null,

  QueryInterface: function (iid) {
    if (iid.equals(Ci.nsISupports) ||
	iid.equals(Ci.nsIProgressEventSink) ||
        iid.equals(Ci.nsIStreamListener) ||
        iid.equals(Ci.nsIRequestObserver))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  getInterface: function (iid) {
    if (iid.equals(Ci.nsIProgressEventSink) ||
        iid.equals(Ci.nsIStreamListener) ||
        iid.equals(Ci.nsIRequestObserver))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  onStartRequest: function(request, context) {
    do_check_eq(this._last_callback_handled, TYPE_ONSTATUS);
    this._got_onstartrequest = true;
    this._last_callback_handled = TYPE_ONSTARTREQUEST;

    this._listener = new ChannelListener(checkRequest, request);
    this._listener.onStartRequest(request, context);
  },

  onDataAvailable: function(request, context, data, offset, count) {
    do_check_eq(this._last_callback_handled, TYPE_ONPROGRESS);
    this._last_callback_handled = TYPE_ONDATAAVAILABLE;

    this._listener.onDataAvailable(request, context, data, offset, count);
  },

  onStopRequest: function(request, context, status) {
    do_check_eq(this._last_callback_handled, TYPE_ONDATAAVAILABLE);
    do_check_true(this._got_onstatus_after_onstartrequest);
    this._last_callback_handled = TYPE_ONSTOPREQUEST;

    this._listener.onStopRequest(request, context, status);
    delete this._listener;
  },

  onProgress: function (request, context, progress, progressMax) {
    do_check_eq(this._last_callback_handled, TYPE_ONSTATUS);
    this._last_callback_handled = TYPE_ONPROGRESS;

    do_check_eq(mStatus, STATUS_RECEIVING_FROM);
    last = progress;
    max = progressMax;
  },

  onStatus: function (request, context, status, statusArg) {
    if (!this._got_onstartrequest) {
      // Ensure that all messages before onStartRequest are onStatus
      if (this._last_callback_handled)
        do_check_eq(this._last_callback_handled, TYPE_ONSTATUS);
    } else if (this._last_callback_handled == TYPE_ONSTARTREQUEST) {
      this._got_onstatus_after_onstartrequest = true;
    } else {
      do_check_eq(this._last_callback_handled, TYPE_ONDATAAVAILABLE);
    }
    this._last_callback_handled = TYPE_ONSTATUS;

    do_check_eq(statusArg, "localhost");
    mStatus = status;
  },

  mStatus: 0,
};

function run_test() {
  httpserver.registerPathHandler(testpath, serverHandler);
  httpserver.start(-1);
  var channel = setupChannel(testpath);
  channel.asyncOpen(progressCallback, null);
  do_test_pending();
}

function setupChannel(path) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
                       getService(Ci.nsIIOService);
  var chan = ios.newChannel(URL + path, "", null);
  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.requestMethod = "GET";
  chan.notificationCallbacks = progressCallback;
  return chan;
}

function serverHandler(metadata, response) {
  response.setHeader("Content-Type", "text/plain", false);
  for (let i = 0; i < LOOPS; i++)
    response.bodyOutputStream.write(httpbody, httpbody.length);
}

function checkRequest(request, data, context) {
  do_check_eq(last, httpbody.length*LOOPS);
  do_check_eq(max, httpbody.length*LOOPS);
  httpserver.stop(do_test_finished);
}
