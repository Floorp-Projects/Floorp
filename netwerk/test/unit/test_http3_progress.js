/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This test can be merged with test_progress.js once HTTP/3 tests are
// enabled on all plaforms.

var last = 0;
var max = 0;

const RESPONSE_LENGTH = 3000000;
const STATUS_RECEIVING_FROM = 0x804b0006;

const TYPE_ONSTATUS = 1;
const TYPE_ONPROGRESS = 2;
const TYPE_ONSTARTREQUEST = 3;
const TYPE_ONDATAAVAILABLE = 4;
const TYPE_ONSTOPREQUEST = 5;

var ProgressCallback = function() {};

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
  http3_clear_prefs();
});

add_task(async function setup() {
  await http3_setup_tests("h3-29");
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

function checkRequest(request, data, context) {
  Assert.equal(last, RESPONSE_LENGTH);
  Assert.equal(max, RESPONSE_LENGTH);
}

add_task(async function test_http3() {
  await chanPromise(
    "https://foo.example.com/" + RESPONSE_LENGTH,
    "foo.example.com"
  );
});
