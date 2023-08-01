"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

ChromeUtils.defineLazyGetter(this, "URL", function () {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

var httpserver = new HttpServer();
var testpath = "/simple";
var httpbody = "0123456789";

var last = 0,
  max = 0;

const STATUS_RECEIVING_FROM = 0x4b0006;
const LOOPS = 50000;

const TYPE_ONSTATUS = 1;
const TYPE_ONPROGRESS = 2;
const TYPE_ONSTARTREQUEST = 3;
const TYPE_ONDATAAVAILABLE = 4;
const TYPE_ONSTOPREQUEST = 5;

var ProgressCallback = function () {};

ProgressCallback.prototype = {
  _listener: null,
  _got_onstartrequest: false,
  _got_onstatus_after_onstartrequest: false,
  _last_callback_handled: null,
  statusArg: "",
  finish: null,

  QueryInterface: ChromeUtils.generateQI([
    "nsIProgressEventSink",
    "nsIStreamListener",
    "nsIRequestObserver",
  ]),

  getInterface(iid) {
    if (
      iid.equals(Ci.nsIProgressEventSink) ||
      iid.equals(Ci.nsIStreamListener) ||
      iid.equals(Ci.nsIRequestObserver)
    ) {
      return this;
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
  },

  onStartRequest(request) {
    Assert.equal(this._last_callback_handled, TYPE_ONSTATUS);
    this._got_onstartrequest = true;
    this._last_callback_handled = TYPE_ONSTARTREQUEST;

    this._listener = new ChannelListener(checkRequest, request);
    this._listener.onStartRequest(request);
  },

  onDataAvailable(request, data, offset, count) {
    Assert.equal(this._last_callback_handled, TYPE_ONPROGRESS);
    this._last_callback_handled = TYPE_ONDATAAVAILABLE;

    this._listener.onDataAvailable(request, data, offset, count);
  },

  onStopRequest(request, status) {
    Assert.equal(this._last_callback_handled, TYPE_ONDATAAVAILABLE);
    Assert.ok(this._got_onstatus_after_onstartrequest);
    this._last_callback_handled = TYPE_ONSTOPREQUEST;

    this._listener.onStopRequest(request, status);
    delete this._listener;
    this.finish();
  },

  onProgress(request, progress, progressMax) {
    Assert.equal(this._last_callback_handled, TYPE_ONSTATUS);
    this._last_callback_handled = TYPE_ONPROGRESS;

    Assert.equal(this.mStatus, STATUS_RECEIVING_FROM);
    last = progress;
    max = progressMax;
  },

  onStatus(request, status, statusArg) {
    if (!this._got_onstartrequest) {
      // Ensure that all messages before onStartRequest are onStatus
      if (this._last_callback_handled) {
        Assert.equal(this._last_callback_handled, TYPE_ONSTATUS);
      }
    } else if (this._last_callback_handled == TYPE_ONSTARTREQUEST) {
      this._got_onstatus_after_onstartrequest = true;
    } else {
      Assert.equal(this._last_callback_handled, TYPE_ONDATAAVAILABLE);
    }
    this._last_callback_handled = TYPE_ONSTATUS;

    Assert.equal(statusArg, this.statusArg);
    this.mStatus = status;
  },

  mStatus: 0,
};

registerCleanupFunction(async () => {
  await httpserver.stop();
});

function chanPromise(uri, statusArg) {
  return new Promise(resolve => {
    var chan = NetUtil.newChannel({
      uri,
      loadUsingSystemPrincipal: true,
    });
    chan.QueryInterface(Ci.nsIHttpChannel);
    chan.requestMethod = "GET";
    let listener = new ProgressCallback();
    listener.statusArg = statusArg;
    chan.notificationCallbacks = listener;
    listener.finish = resolve;
    chan.asyncOpen(listener);
  });
}

add_task(async function test_http1_1() {
  httpserver.registerPathHandler(testpath, serverHandler);
  httpserver.start(-1);
  await chanPromise(URL + testpath, "localhost");
});

function serverHandler(metadata, response) {
  response.setHeader("Content-Type", "text/plain", false);
  for (let i = 0; i < LOOPS; i++) {
    response.bodyOutputStream.write(httpbody, httpbody.length);
  }
}

function checkRequest(request, data, context) {
  Assert.equal(last, httpbody.length * LOOPS);
  Assert.equal(max, httpbody.length * LOOPS);
}
